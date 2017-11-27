#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <ArduinoOTA.h>
#include "WM.h"
#include <FS.h>
#include <ArduinoJson.h>
#include <WiFiUdp.h>
#include <Arduino.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266Ping.h>
#include <ESP8266mDNS.h>
#include "css_global.h"
#include "js_global.h"
#include "js_fwupd.h"
#include <EEPROM.h>

const String FIRMWARE_VERSION = "0.1";
const char GITHUB_REPO_URL[] PROGMEM = "https://api.github.com/repos/jp112sdl/SonoffDualShutterHMLOX/releases/latest";

#define IPSIZE                              16
#define VARIABLESIZE                       255
#define LEDPin                              13
#define ConfigPortalTimeout                180  //Timeout (Sekunden) des AccessPoint-Modus
#define UDPPORT                           6176
#define HTTPTimeOut                       2000
#define EXTRADRIVETIMEFORENDPOSTIONMILLIS 1500
//#define                                   SERIALDEBUG
#define                                   UDPDEBUG

#ifdef UDPDEBUG
const char * SYSLOGIP = "192.168.1.251";
#define SYSLOGPORT          514
#endif

enum BackendTypes_e {
  BackendType_HomeMatic,
  BackendType_Loxone
};

enum OnOff { OFF, ON };

enum Directions { DIRECTION_NONE, DIRECTION_DOWN, DIRECTION_UP };

enum _SyslogSeverity {
  _slEmergency,
  _slAlert,
  _slCritical,
  _slError,
  _slWarning,
  _slNotice,
  _slInformational,
  _slDebug
};

struct globalconfig_t {
  char ccuIP[IPSIZE]   = "";
  char DeviceName[VARIABLESIZE] = "";
  byte BackendType = BackendType_HomeMatic;
  String Hostname = "Sonoff";
} GlobalConfig;

struct hmconfig_t {
  String ChannelName = "";
} HomeMaticConfig;

struct loxoneconfig_t {
  char Username[VARIABLESIZE] = "";
  char Password[VARIABLESIZE] = "";
  char UDPPort[10] = "";
} LoxoneConfig;

struct sonoffnetconfig_t {
  char ip[IPSIZE]      = "0.0.0.0";
  char netmask[IPSIZE] = "0.0.0.0";
  char gw[IPSIZE]      = "0.0.0.0";
} SonoffNetConfig;

struct shutterconfig_t {
  float MotorSwitchingTime  = 1.0;
  float DriveTimeUp = 50.0;
  float DriveTimeDown = 50.0;
  byte DriveUntilCalib = 0;
  byte CurrentValue = 0;
  byte DriveCounter = 0;
} ShutterConfig;

bool OTAStart = false;
bool UDPReady = false;
unsigned long dual_relay_switched_millis = 0;
unsigned long LastMillis = 0;
int dual_relay_drive_time = 0;
byte DRIVING_DIRECTION = DIRECTION_NONE;
byte shutter_start_value_percent = 0;
byte oldShutterValue = 0;
bool startWifiManager = false;
byte TempTargetValue = 0;
const String configJsonFile         = "config.json";
const String bootConfigModeFilename = "bootcfg.mod";
bool wm_shouldSaveConfig        = false;
#define wifiManagerDebugOutput   false

ESP8266WebServer WebServer(80);
ESP8266HTTPUpdateServer httpUpdater;

struct udp_t {
  WiFiUDP UDP;
  char incomingPacket[255];
} UDPClient;

#define idxLastShutterValue 0

void setup() {
  Serial.begin(19200);
  switch_dual_relay(DIRECTION_NONE);
  Serial.println("\nSonoff DUAL " + WiFi.macAddress() + " startet...");
  pinMode(LEDPin,    OUTPUT);

  Serial.println(F("Config-Modus durch bootConfigMode aktivieren? "));
  if (SPIFFS.begin()) {
    Serial.println(F("-> bootConfigModeFilename mounted file system"));
    if (SPIFFS.exists("/" + bootConfigModeFilename)) {
      startWifiManager = true;
      Serial.println("-> " + bootConfigModeFilename + " existiert, starte Config-Modus");
      SPIFFS.remove("/" + bootConfigModeFilename);
      SPIFFS.end();
    } else {
      Serial.println("-> " + bootConfigModeFilename + " existiert NICHT");
    }
  } else {
    Serial.println(F("-> Nein, SPIFFS mount fail!"));
  }
  if (!startWifiManager) {
    Serial.println(F("Config-Modus mit Taster aktivieren?"));
    Serial.flush();
    for (int i = 0; i < 20; i++) {
      if (ButtonPressed()) {
        startWifiManager = true;
        break;
      }
      digitalWrite(LEDPin, HIGH);
      delay(100);
      digitalWrite(LEDPin, LOW);
      delay(100);
    }
    Serial.println("Config-Modus " + String(((startWifiManager) ? "" : "nicht ")) + "aktiviert.");
  }

  if (!loadSystemConfig()) startWifiManager = true;
  //Ab hier ist die Config geladen und alle Variablen sind mit deren Werten belegt!

  if (doWifiConnect()) {
    Serial.println(F("\nWLAN erfolgreich verbunden!"));
    printWifiStatus();
  } else ESP.restart();

  startOTAhandling();

  initWebServer();
  if (!MDNS.begin(GlobalConfig.Hostname.c_str())) {
    DEBUG("Error setting up MDNS responder!");
  }

  DEBUG("Starte UDP-Handler an Port " + String(UDPPORT) + "...");
  UDPClient.UDP.begin(UDPPORT);
  UDPReady = true;

  if (GlobalConfig.BackendType == BackendType_HomeMatic) {
    HomeMaticConfig.ChannelName =  "CUxD." + getStateCUxD(GlobalConfig.DeviceName, "Address");
    DEBUG("HomeMaticConfig.ChannelName =  " + HomeMaticConfig.ChannelName);
  }

  EEPROM.begin(512);
  byte eeShutterValue = EEPROM.read(idxLastShutterValue);
  if (eeShutterValue > 100) eeShutterValue = 100;
  ShutterConfig.CurrentValue = eeShutterValue;
  DEBUG("Restored last shutter value " + String(ShutterConfig.CurrentValue));

  DEBUG(String(GlobalConfig.DeviceName) + " - Boot abgeschlossen, SSID = " + WiFi.SSID() + ", IP = " + String(IpAddress2String(WiFi.localIP())) + ", RSSI = " + WiFi.RSSI() + ", MAC = " + WiFi.macAddress(), "Setup", _slInformational);
}

void loop() {
  if (dual_relay_switched_millis > millis())
    dual_relay_switched_millis = millis();
  ArduinoOTA.handle();
  if (!OTAStart) {
    WebServer.handleClient();
  }

  //aktuellen Stand während der Fahrt berechnen
  byte CurrentValueTemp = calculatePercent(shutter_start_value_percent, DRIVING_DIRECTION);
  if (shutter_start_value_percent != CurrentValueTemp) {
    ShutterConfig.CurrentValue = CurrentValueTemp;
  }

  //Schalte Relais ab, wenn die Fahrzeit erreicht wurde
  if (DRIVING_DIRECTION > DIRECTION_NONE && dual_relay_switched_millis > 0 && millis() - dual_relay_switched_millis > dual_relay_drive_time) {
    DEBUG("Schalte Relais ab!");
    switch_dual_relay(DIRECTION_NONE);
    switch_dual_relay(DIRECTION_NONE); //doppelt hält besser... sicher ist sicher
    dual_relay_switched_millis = 0;
    dual_relay_drive_time = 0;
  }

  //Fahrt-Zähler für Kalibrierungsfahrten auf 0 setzen, wenn eine der beiden Endlagen angefahren wurde
  if (ShutterConfig.CurrentValue == 0 ||  ShutterConfig.CurrentValue == 100)  ShutterConfig.DriveCounter = 0;

  //Wenn Kalibrierfahrt läuft und Endposition 100% erreicht, dann fahre ursprüngliche Zielposition an
  if (ShutterConfig.CurrentValue == 100 && TempTargetValue > 0 && DRIVING_DIRECTION == DIRECTION_NONE) {
    delay(ShutterConfig.MotorSwitchingTime * 1000);
    ShutterControl(TempTargetValue);
  }

  //alle 2 Sekunden Wert an CCU senden
  if (millis() - LastMillis > 2000) {
    LastMillis = millis();
    if (oldShutterValue != ShutterConfig.CurrentValue) {
      oldShutterValue = ShutterConfig.CurrentValue;
      EEPROM.write(idxLastShutterValue, ShutterConfig.CurrentValue);
      EEPROM.commit();
      float ccuVal = float(ShutterConfig.CurrentValue / 100.0);
      if (GlobalConfig.BackendType == BackendType_HomeMatic) setStateCUxD(HomeMaticConfig.ChannelName + ".SET_STATE", String(ccuVal));
    }
  }
}

void ShutterControl(byte TargetValue) {
  DEBUG("ShutterControl(" + String(TargetValue) + ")");

  ShutterConfig.DriveCounter++;

  if (ShutterConfig.DriveCounter > ShutterConfig.DriveUntilCalib) {
    ShutterConfig.DriveCounter = 1;
    TempTargetValue = TargetValue;
    TargetValue = 100;
  } else {
    TempTargetValue = 0;
  }

  if (TargetValue > ShutterConfig.CurrentValue || TargetValue == 100) {
    dual_relay_drive_time = calculateDriveTime(TargetValue, ShutterConfig.CurrentValue, DIRECTION_UP);
    if (dual_relay_drive_time > 0) {
      if (DRIVING_DIRECTION == DIRECTION_DOWN) {
        switch_dual_relay(DIRECTION_NONE);
        delay(ShutterConfig.MotorSwitchingTime * 1000);
      }
      switch_dual_relay(DIRECTION_UP);
    }
  }

  if (TargetValue < ShutterConfig.CurrentValue || TargetValue == 0) {
    dual_relay_drive_time = calculateDriveTime(TargetValue, ShutterConfig.CurrentValue, DIRECTION_DOWN);
    if (dual_relay_drive_time > 0) {
      if (DRIVING_DIRECTION == DIRECTION_UP) {
        switch_dual_relay(DIRECTION_NONE);
        delay(ShutterConfig.MotorSwitchingTime * 1000);
      }
      switch_dual_relay(DIRECTION_DOWN);
    }
  }
}

