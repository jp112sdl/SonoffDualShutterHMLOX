/*
  Generic ESP8285 Module
  Flash Mode: DOUT
  Flash Frequency: 40 MHz
  CPU Frequency: 80 MHz
  Flash Size: 1M (64k SPIFFS)
*/
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
#include "css_global.h"
#include "js_global.h"
#include "js_fwupd.h"
#include <EEPROM.h>

const String FIRMWARE_VERSION = "1.0";
const char GITHUB_REPO_URL[] PROGMEM = "https://api.github.com/repos/jp112sdl/SonoffDualShutterHMLOX/releases/latest";

#define IPSIZE                              16
#define VARIABLESIZE                       255
#define LEDPinDual                          13

//HVIO:
#define LEDPinHVIO            15
#define Relay1PinHVIO          4
#define Relay2PinHVIO          5
#define Switch1PinHVIO        12
#define Switch2PinHVIO        13

//Dual R2:
#define Relay1PinDualR2       12
#define Relay2PinDualR2        5
#define Switch1PinDualR2       0
#define Switch2PinDualR2       9
#define SwitchPinHeadDualR2   10

byte Switch1 = 0;
byte Switch2 = 0;
byte Relay1 = 0;
byte Relay2 = 0;

#define ConfigPortalTimeout                180  //Timeout (Sekunden) des AccessPoint-Modus
#define UDPPORT                           6176
#define HTTPTimeOut                       1000
#define EXTRADRIVETIMEFORENDPOSTIONMILLIS 1500
#define                                   SERIALDEBUG
//#define                                   UDPDEBUG

#ifdef UDPDEBUG
const char * SYSLOGIP = "192.168.1.251";
#define SYSLOGPORT          514
#endif

enum BackendTypes_e {
  BackendType_HomeMatic,
  BackendType_Loxone
};

enum Model_e {
  Model_Dual,
  Model_HVIO,
  Model_DualR2
};

enum OnOff { Off, On };

enum Directions { DIRECTION_NONE, DIRECTION_DOWN, DIRECTION_UP };
enum Keys       { KEY_NONE, KEY_DOWN, KEY_UP };

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
  String Hostname = "Shutter";
  byte Model = Model_Dual;
} GlobalConfig;

struct hmconfig_t {
  String ChannelName = "";
  bool UseCCU = false;
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
  byte DrivesUntilCalib = 0;
  byte CurrentValue = 0;
  byte DriveCounter = 0;
} ShutterConfig;

bool OTAStart = false;
bool UDPReady = false;
bool WiFiConnected = false;
unsigned long KeyPressMillis = 0;
unsigned long LastWiFiReconnectMillis = 0;
byte LEDPin = 13;
unsigned long relay_switched_millis = 0;
unsigned long LastMillis = 0;
int relay_drive_time = 0;
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
  switch_relay(DIRECTION_NONE);
  Serial.println("\nSonoffDual (R2) / HVIO " + WiFi.macAddress() + " startet...");
  pinMode(LEDPinDual, OUTPUT);
  pinMode(LEDPinHVIO, OUTPUT);
  pinMode(Switch2PinHVIO, INPUT_PULLUP);
  pinMode(SwitchPinHeadDualR2, INPUT_PULLUP);

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
      if (digitalRead(SwitchPinHeadDualR2) == LOW || digitalRead(Switch2PinHVIO) == LOW || ButtonPressed()) {
        startWifiManager = true;
        break;
      }
      digitalWrite(LEDPinDual, HIGH);
      digitalWrite(LEDPinHVIO, LOW);
      delay(100);
      digitalWrite(LEDPinDual, LOW);
      digitalWrite(LEDPinHVIO, HIGH);
      delay(100);
    }
    Serial.println("Config-Modus " + String(((startWifiManager) ? "" : "nicht ")) + "aktiviert.");
  }

  if (!loadSystemConfig()) startWifiManager = true;
  //Ab hier ist die Config geladen und alle Variablen sind mit deren Werten belegt!

  //bei Loxone die Fahrzeiten fiktiv setzen
  if (GlobalConfig.BackendType == BackendType_Loxone) {
    ShutterConfig.DriveTimeUp = 0.0;
    ShutterConfig.DriveTimeDown = 0.0;
    ShutterConfig.MotorSwitchingTime  = 1.0;
    ShutterConfig.DrivesUntilCalib = 0;
  }

  HomeMaticConfig.UseCCU = (String(GlobalConfig.ccuIP) != "0.0.0.0");

  if (doWifiConnect()) {
    Serial.println(F("\nWLAN erfolgreich verbunden!"));
    printWifiStatus();
  } else ESP.restart();

  switch (GlobalConfig.Model) {
    case Model_Dual:
      DEBUG("\nModell = Sonoff Dual");
      LEDPin = LEDPinDual;
      break;
    case Model_HVIO:
      DEBUG("\nModell = HVIO");
      LEDPin = LEDPinHVIO;
      pinMode(Relay1PinHVIO, OUTPUT);
      pinMode(Relay2PinHVIO, OUTPUT);
      pinMode(Switch1PinHVIO, INPUT_PULLUP);
      pinMode(Switch2PinHVIO, INPUT_PULLUP);
      Switch1 = Switch1PinHVIO;
      Switch2 = Switch2PinHVIO;
      Relay1 = Relay1PinHVIO;
      Relay2 = Relay2PinHVIO;
      break;
    case Model_DualR2:
      DEBUG("\nModell = DualR2");
      LEDPin = LEDPinDual;
      pinMode(Relay1PinDualR2, OUTPUT);
      pinMode(Relay2PinDualR2, OUTPUT);
      pinMode(Switch1PinDualR2, INPUT_PULLUP);
      pinMode(Switch2PinDualR2, INPUT_PULLUP);
      pinMode(SwitchPinHeadDualR2, INPUT_PULLUP);
      Switch1 = Switch1PinDualR2;
      Switch2 = Switch2PinDualR2;
      Relay1 = Relay1PinDualR2;
      Relay2 = Relay2PinDualR2;
      break;
  }
  pinMode(LEDPin, OUTPUT);

  initWebServer();

  startOTAhandling();

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
  if (relay_switched_millis > millis())
    relay_switched_millis = millis();
  if (LastWiFiReconnectMillis > millis())
    LastWiFiReconnectMillis = millis();
    
  //Reconnect WiFi wenn nicht verbunden (alle 30 Sekunden)
  if (WiFi.status() != WL_CONNECTED) {
    WiFiConnected = false;
    if (millis() - LastWiFiReconnectMillis > 30000) {
      LastWiFiReconnectMillis = millis();
      DEBUG("WiFi Connection lost! Reconnecting...");
      WiFi.reconnect();
    }
  } else {
    if (!WiFiConnected) {
      DEBUG("WiFi reconnected!");
      WiFiConnected = true;
    }
  }  
    
  ArduinoOTA.handle();
  if (!OTAStart) {
    WebServer.handleClient();
  }

  //Tasterbedienung Taster abarbeiten
  if (GlobalConfig.Model != Model_Dual) {
    byte KeyPressed = KEY_NONE;
    if (digitalRead(Switch1) == LOW) KeyPressed = KEY_UP;
    if (digitalRead(Switch2) == LOW) KeyPressed = KEY_DOWN;

    if (KeyPressed > KEY_NONE) {
      if (KeyPressMillis == 0) {
        DEBUG("KEY " + String(KeyPressed) + " PRESSED");
        bool doNothing = false;
        if (DRIVING_DIRECTION > DIRECTION_NONE) {
          doNothing = true;
          switch_relay(DIRECTION_NONE);
          delay(ShutterConfig.MotorSwitchingTime * 1000);
        }

        if (!doNothing) {
          if (KeyPressed == KEY_UP) {
            ShutterControl(100);
          }

          if (KeyPressed == KEY_DOWN) {
            ShutterControl(0);
          }
          KeyPressMillis = millis();
        }
      }
    } else {
      if (KeyPressMillis > 0 && millis() - KeyPressMillis > 2000) {
        DEBUG("KEY RELEASED, KeyPressMillis = " + String(millis() - KeyPressMillis));
        switch_relay(DIRECTION_NONE);
        delay(ShutterConfig.MotorSwitchingTime * 1000);
      }
      KeyPressMillis = 0;
    }
  }

  //aktuellen Stand während der Fahrt berechnen
  byte CurrentValueTemp = calculatePercent(shutter_start_value_percent, DRIVING_DIRECTION);
  if (shutter_start_value_percent != CurrentValueTemp) {
    ShutterConfig.CurrentValue = CurrentValueTemp;
  }

  //Schalte Relais ab, wenn die Fahrzeit erreicht wurde
  if (ShutterConfig.DriveTimeUp   > 0.0  &&
      ShutterConfig.DriveTimeDown > 0.0  &&
      DRIVING_DIRECTION > DIRECTION_NONE &&
      relay_switched_millis > 0     &&
      millis() - relay_switched_millis > relay_drive_time) {
    DEBUG("Schalte Relais ab!");
    switch_relay(DIRECTION_NONE);
    switch_relay(DIRECTION_NONE); //doppelt hält besser... sicher ist sicher
    relay_switched_millis = 0;
    relay_drive_time = 0;
  }

  //Fahrt-Zähler für Kalibrierungsfahrten auf 0 setzen, wenn eine der beiden Endlagen angefahren wurde
  if (ShutterConfig.CurrentValue == 0 ||  ShutterConfig.CurrentValue == 100)  ShutterConfig.DriveCounter = 0;

  //Wenn Kalibrierfahrt läuft und Endposition 100% erreicht, dann fahre ursprüngliche Zielposition an
  if (ShutterConfig.CurrentValue == 100 && TempTargetValue > 0 && DRIVING_DIRECTION == DIRECTION_NONE) {
    delay(ShutterConfig.MotorSwitchingTime * 1000);
    ShutterControl(TempTargetValue);
  }

  //alle 2 Sekunden Wert in EEPROM speichern und an CCU senden
  if (millis() - LastMillis > 2000) {
    LastMillis = millis();
    //aber nur während der Fahrt
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

  if (ShutterConfig.DrivesUntilCalib > 0) ShutterConfig.DriveCounter++;

  if (ShutterConfig.DriveCounter > ShutterConfig.DrivesUntilCalib && ShutterConfig.DrivesUntilCalib > 0) {
    ShutterConfig.DriveCounter = 1;
    TempTargetValue = TargetValue;
    TargetValue = 100;
  } else {
    TempTargetValue = 0;
  }

  if (TargetValue > ShutterConfig.CurrentValue || TargetValue == 100) {
    relay_drive_time = calculateDriveTime(TargetValue, ShutterConfig.CurrentValue, DIRECTION_UP);
    if (relay_drive_time > 0) {
      if (DRIVING_DIRECTION == DIRECTION_DOWN) {
        switch_relay(DIRECTION_NONE);
        delay(ShutterConfig.MotorSwitchingTime * 1000);
      }
      switch_relay(DIRECTION_UP);
    }
  }

  if (TargetValue < ShutterConfig.CurrentValue || TargetValue == 0) {
    relay_drive_time = calculateDriveTime(TargetValue, ShutterConfig.CurrentValue, DIRECTION_DOWN);
    if (relay_drive_time > 0) {
      if (DRIVING_DIRECTION == DIRECTION_UP) {
        switch_relay(DIRECTION_NONE);
        delay(ShutterConfig.MotorSwitchingTime * 1000);
      }
      switch_relay(DIRECTION_DOWN);
    }
  }
}

