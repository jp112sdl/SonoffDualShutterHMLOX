#define JSONCONFIG_IP                     "ip"
#define JSONCONFIG_NETMASK                "netmask"
#define JSONCONFIG_GW                     "gw"
#define JSONCONFIG_CCUIP                  "ccuip"
#define JSONCONFIG_SONOFF                 "sonoff"
#define JSONCONFIG_LOXUDPPORT             "loxudpport"
//#define JSONCONFIG_LOXUSERNAME            "loxusername"
//#define JSONCONFIG_LOXPASSWORD            "loxpassword"
#define JSONCONFIG_BACKENDTYPE            "backendtype"
#define JSONCONFIG_SHUTTER_MOTORSWITCHINGTIME "sh_mst"
#define JSONCONFIG_SHUTTER_DRIVETIMEUP        "sh_dtup"
#define JSONCONFIG_SHUTTER_DRIVETIMEDOWN      "sh_dtdown"
#define JSONCONFIG_SHUTTER_DRIVEUNTILCALIB    "sh_cal"

bool loadSystemConfig() {
  DEBUG(F("loadSystemConfig mounting FS..."), "loadSystemConfig()", _slInformational);
  if (SPIFFS.begin()) {
    DEBUG(F("loadSystemConfig mounted file system"), "loadSystemConfig()", _slInformational);
    if (SPIFFS.exists("/" + configJsonFile)) {
      DEBUG(F("loadSystemConfig reading config file"), "loadSystemConfig()", _slInformational);
      File configFile = SPIFFS.open("/" + configJsonFile, "r");
      if (configFile) {
        DEBUG(F("loadSystemConfig opened config file"), "loadSystemConfig()", _slInformational);
        size_t size = configFile.size();
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        DEBUG("Content of JSON Config-File: /" + configJsonFile, "loadSystemConfig()", _slInformational);
#ifdef SERIALDEBUG
        json.printTo(Serial);
        Serial.println();
#endif
        if (json.success()) {
          DEBUG("\nJSON OK", "loadSystemConfig()", _slInformational);
          ((json[JSONCONFIG_IP]).as<String>()).toCharArray(SonoffNetConfig.ip, IPSIZE);
          ((json[JSONCONFIG_NETMASK]).as<String>()).toCharArray(SonoffNetConfig.netmask, IPSIZE);
          ((json[JSONCONFIG_GW]).as<String>()).toCharArray(SonoffNetConfig.gw, IPSIZE);
          ((json[JSONCONFIG_CCUIP]).as<String>()).toCharArray(GlobalConfig.ccuIP, IPSIZE);
          ((json[JSONCONFIG_SONOFF]).as<String>()).toCharArray(GlobalConfig.DeviceName, VARIABLESIZE);
          //((json[JSONCONFIG_LOXUSERNAME]).as<String>()).toCharArray(LoxoneConfig.Username, VARIABLESIZE);
          //((json[JSONCONFIG_LOXPASSWORD]).as<String>()).toCharArray(LoxoneConfig.Password, VARIABLESIZE);
          ((json[JSONCONFIG_LOXUDPPORT]).as<String>()).toCharArray(LoxoneConfig.UDPPort, 10);
          GlobalConfig.BackendType = json[JSONCONFIG_BACKENDTYPE];
          GlobalConfig.Hostname = "Sonoff-" + String(GlobalConfig.DeviceName);
          ShutterConfig.MotorSwitchingTime = json[JSONCONFIG_SHUTTER_MOTORSWITCHINGTIME].as<float>();
          ShutterConfig.DriveTimeUp = json[JSONCONFIG_SHUTTER_DRIVETIMEUP].as<float>();
          ShutterConfig.DriveTimeDown = json[JSONCONFIG_SHUTTER_DRIVETIMEDOWN].as<float>();
          ShutterConfig.DriveUntilCalib = json[JSONCONFIG_SHUTTER_DRIVEUNTILCALIB];
        } else {
          DEBUG(F("\nloadSystemConfig ERROR loading config"), "loadSystemConfig()", _slInformational);
        }
      }
      return true;
    } else {
      DEBUG("/" + configJsonFile + " not found.", "loadSystemConfig()", _slInformational);
      return false;
    }
    SPIFFS.end();
  } else {
    DEBUG(F("loadSystemConfig failed to mount FS"), "loadSystemConfig()", _slCritical);
    return false;
  }
}

bool saveSystemConfig() {
  SPIFFS.begin();
  DEBUG(F("saving config"), "saveSystemConfig()", _slInformational);
  DynamicJsonBuffer jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
  json[JSONCONFIG_IP] = SonoffNetConfig.ip;
  json[JSONCONFIG_NETMASK] = SonoffNetConfig.netmask;
  json[JSONCONFIG_GW] = SonoffNetConfig.gw;
  json[JSONCONFIG_CCUIP] = GlobalConfig.ccuIP;
  json[JSONCONFIG_SONOFF] = GlobalConfig.DeviceName;
  json[JSONCONFIG_BACKENDTYPE] = GlobalConfig.BackendType;
  //json[JSONCONFIG_LOXUSERNAME] = LoxoneConfig.Username;
  //json[JSONCONFIG_LOXPASSWORD] = LoxoneConfig.Password;
  json[JSONCONFIG_LOXUDPPORT] = LoxoneConfig.UDPPort;

  json[JSONCONFIG_SHUTTER_MOTORSWITCHINGTIME] = ShutterConfig.MotorSwitchingTime;
  json[JSONCONFIG_SHUTTER_DRIVETIMEUP] = ShutterConfig.DriveTimeUp;
  json[JSONCONFIG_SHUTTER_DRIVETIMEDOWN] = ShutterConfig.DriveTimeDown;
  json[JSONCONFIG_SHUTTER_DRIVEUNTILCALIB] = ShutterConfig.DriveUntilCalib;

  SPIFFS.remove("/" + configJsonFile);
  File configFile = SPIFFS.open("/" + configJsonFile, "w");
  if (!configFile) {
    DEBUG(F("failed to open config file for writing"), "saveSystemConfig()", _slCritical);
    return false;
  }

#ifdef SERIALDEBUG
  json.printTo(Serial);
  Serial.println();
#endif
  json.printTo(configFile);
  configFile.close();
  SPIFFS.end();
  return true;
}

void setBootConfigMode() {
  if (SPIFFS.begin()) {
    DEBUG(F("setBootConfigMode mounted file system"), "setBootConfigMode()", _slInformational);
    if (!SPIFFS.exists("/" + bootConfigModeFilename)) {
      File bootConfigModeFile = SPIFFS.open("/" + bootConfigModeFilename, "w");
      bootConfigModeFile.print("0");
      bootConfigModeFile.close();
      SPIFFS.end();
      DEBUG(F("Boot to ConfigMode requested. Restarting..."), "setBootConfigMode()", _slInformational);
      WebServer.send(200, "text/plain", F("<state>enableBootConfigMode - Rebooting</state>"));
      delay(500);
      ESP.restart();
    } else {
      WebServer.send(200, "text/plain", F("<state>enableBootConfigMode - FAILED!</state>"));
      SPIFFS.end();
    }
  }
}
