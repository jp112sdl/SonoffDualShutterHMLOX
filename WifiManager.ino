bool doWifiConnect() {
  String _ssid = WiFi.SSID();
  String _psk = WiFi.psk();

  String _pskMask = "";
  for (int i = 0; i < _psk.length(); i++) {
    _pskMask += "*";
  }
  DEBUG("ssid = " + _ssid + ", psk = " + _pskMask);


  const char* ipStr = SonoffNetConfig.ip; byte ipBytes[4]; parseBytes(ipStr, '.', ipBytes, 4, 10);
  const char* netmaskStr = SonoffNetConfig.netmask; byte netmaskBytes[4]; parseBytes(netmaskStr, '.', netmaskBytes, 4, 10);
  const char* gwStr = SonoffNetConfig.gw; byte gwBytes[4]; parseBytes(gwStr, '.', gwBytes, 4, 10);

  if (!startWifiManager && _ssid != "" && _psk != "" ) {
    DEBUG(F("Connecting WLAN the classic way..."));
    //WiFi.disconnect();
    WiFi.mode(WIFI_STA);
    WiFi.hostname(GlobalConfig.Hostname);
    WiFi.setSleepMode(WIFI_NONE_SLEEP);
    WiFi.begin(_ssid.c_str(), _psk.c_str());
    int waitCounter = 0;
    if (String(SonoffNetConfig.ip) != "0.0.0.0")
      WiFi.config(IPAddress(ipBytes[0], ipBytes[1], ipBytes[2], ipBytes[3]), IPAddress(gwBytes[0], gwBytes[1], gwBytes[2], gwBytes[3]), IPAddress(netmaskBytes[0], netmaskBytes[1], netmaskBytes[2], netmaskBytes[3]));

    while (WiFi.status() != WL_CONNECTED) {
      waitCounter++;
      Serial.print(".");
      digitalWrite(LEDPinHVIO, (!(digitalRead(LEDPinHVIO))));
      digitalWrite(LEDPinDual, (!(digitalRead(LEDPinDual))));
      if (waitCounter == 30) {
        return false;
      }
      delay(500);
    }
    DEBUG("Wifi Connected");
    WiFiConnected = true;
    return true;
  } else {
    WiFiManager wifiManager;
    wifiManager.setDebugOutput(wifiManagerDebugOutput);
    digitalWrite(LEDPinDual, LOW);
    digitalWrite(LEDPinHVIO, LOW);
    wifiManager.setAPCallback(configModeCallback);
    wifiManager.setSaveConfigCallback(saveConfigCallback);
    WiFiManagerParameter custom_ccuip("ccu", "IP der CCU2", GlobalConfig.ccuIP, IPSIZE, 0, "pattern='((^|\\.)((25[0-5])|(2[0-4]\\d)|(1\\d\\d)|([1-9]?\\d))){4}$'");
    //WiFiManagerParameter custom_loxusername("loxusername", "Loxone Username", "", VARIABLESIZE);
    //WiFiManagerParameter custom_loxpassword("loxpassword", "Loxone Password", "", VARIABLESIZE,4);
    WiFiManagerParameter custom_loxudpport("loxudpport", "Loxone UDP Port", LoxoneConfig.UDPPort, 10, 0, "pattern='[0-9]{1,5}'");
    WiFiManagerParameter custom_sonoffname("sonoff", "Sonoff Ger&auml;tename", GlobalConfig.DeviceName, VARIABLESIZE, 0, "pattern='[A-Za-z0-9_ -]+'");

    String backend = "";
    switch (GlobalConfig.BackendType) {
      case BackendType_HomeMatic:
        backend = F("<option selected value='0'>HomeMatic</option><option value='1'>Loxone</option>");
        break;
      case BackendType_Loxone:
        backend = F("<option value='0'>HomeMatic</option><option selected value='1'>Loxone</option>");
        break;
      default:
        backend = F("<option selected value='0'>HomeMatic</option><option value='1'>Loxone</option>");
        break;
    }
    WiFiManagerParameter custom_backendtype("backendtype", "Backend", "", 8, 2, backend.c_str());

    String model = "";
    switch (GlobalConfig.Model) {
      case Model_Dual:
        model = F("<option selected value='0'>Sonoff Dual</option><option value='1'>HVIO</option><option value='2'>Sonoff Dual R2</option>");
        break;
      case Model_HVIO:
        model = F("<option value='0'>Sonoff Dual</option><option selected value='1'>HVIO</option><option value='2'>Sonoff Dual R2</option>");
        break;
      case Model_DualR2:
        model = F("<option value='0'>Sonoff Dual</option><option value='1'>HVIO</option><option selected value='2'>Sonoff Dual R2</option>");
        break;
      default:
        model = F("<option selected value='0'>Sonoff Dual</option><option value='1'>HVIO</option><option value='2'>Sonoff Dual R2</option>");
        break;
    }
    WiFiManagerParameter custom_model("model", "Modell", "", 8, 2, model.c_str());

    WiFiManagerParameter custom_text("<br/><br><div>Statische IP (wenn leer, dann DHCP):</div>");
    WiFiManagerParameter custom_ip("custom_ip", "IP-Adresse", (String(SonoffNetConfig.ip) != "0.0.0.0") ? SonoffNetConfig.ip : "", IPSIZE, 0, "pattern='((^|\\.)((25[0-5])|(2[0-4]\\d)|(1\\d\\d)|([1-9]?\\d))){4}$'");
    WiFiManagerParameter custom_netmask("custom_netmask", "Netzmaske", (String(SonoffNetConfig.netmask) != "0.0.0.0") ? SonoffNetConfig.netmask : "", IPSIZE, 0, "pattern='((^|\\.)((25[0-5])|(2[0-4]\\d)|(1\\d\\d)|([1-9]?\\d))){4}$'");
    WiFiManagerParameter custom_gw("custom_gw", "Gateway",  (String(SonoffNetConfig.gw) != "0.0.0.0") ? SonoffNetConfig.gw : "", IPSIZE, 0, "pattern='((^|\\.)((25[0-5])|(2[0-4]\\d)|(1\\d\\d)|([1-9]?\\d))){4}$'");
    wifiManager.addParameter(&custom_model);
    wifiManager.addParameter(&custom_ccuip);
    //wifiManager.addParameter(&custom_loxusername);
    //wifiManager.addParameter(&custom_loxpassword);
    wifiManager.addParameter(&custom_loxudpport);
    wifiManager.addParameter(&custom_sonoffname);
    wifiManager.addParameter(&custom_backendtype);


    wifiManager.addParameter(&custom_text);
    wifiManager.addParameter(&custom_ip);
    wifiManager.addParameter(&custom_netmask);
    wifiManager.addParameter(&custom_gw);

    String Hostname = "Shutter-" + WiFi.macAddress();

    wifiManager.setConfigPortalTimeout(ConfigPortalTimeout);

    if (startWifiManager == true) {
      if (_ssid == "" || _psk == "" ) {
        wifiManager.resetSettings();
      }
      else {
        if (!wifiManager.startConfigPortal(Hostname.c_str())) {
          DEBUG(F("WM: failed to connect and hit timeout"));
          delay(500);
          ESP.restart();
        }
      }
    }

    wifiManager.setSTAStaticIPConfig(IPAddress(ipBytes[0], ipBytes[1], ipBytes[2], ipBytes[3]), IPAddress(gwBytes[0], gwBytes[1], gwBytes[2], gwBytes[3]), IPAddress(netmaskBytes[0], netmaskBytes[1], netmaskBytes[2], netmaskBytes[3]));

    wifiManager.autoConnect(Hostname.c_str());

    DEBUG(F("Wifi Connected"));
    DEBUG("CUSTOM STATIC IP: " + String(SonoffNetConfig.ip) + " Netmask: " + String(SonoffNetConfig.netmask) + " GW: " + String(SonoffNetConfig.gw));
    if (wm_shouldSaveConfig) {
      if (String(custom_ip.getValue()).length() > 5) {
        DEBUG("Custom IP Address is set!");
        strcpy(SonoffNetConfig.ip, custom_ip.getValue());
        strcpy(SonoffNetConfig.netmask, custom_netmask.getValue());
        strcpy(SonoffNetConfig.gw, custom_gw.getValue());

      } else {
        strcpy(SonoffNetConfig.ip,      "0.0.0.0");
        strcpy(SonoffNetConfig.netmask, "0.0.0.0");
        strcpy(SonoffNetConfig.gw,      "0.0.0.0");
      }

      GlobalConfig.BackendType = (atoi(custom_backendtype.getValue()));
      GlobalConfig.Model = (atoi(custom_model.getValue()));

      strcpy(GlobalConfig.ccuIP, custom_ccuip.getValue());
      strcpy(GlobalConfig.DeviceName, custom_sonoffname.getValue());
      //strcpy(LoxoneConfig.Username, custom_loxusername.getValue());
      //strcpy(LoxoneConfig.Password, custom_loxpassword.getValue());
      strcpy(LoxoneConfig.UDPPort, custom_loxudpport.getValue());

      saveSystemConfig();

      delay(100);
      ESP.restart();
    }
    return true;
  }
}

void configModeCallback (WiFiManager *myWiFiManager) {
  DEBUG("AP-Modus ist aktiv!");
}

void saveConfigCallback () {
  DEBUG("Should save config");
  wm_shouldSaveConfig = true;
}

void parseBytes(const char* str, char sep, byte* bytes, int maxBytes, int base) {
  for (int i = 0; i < maxBytes; i++) {
    bytes[i] = strtoul(str, NULL, base);
    str = strchr(str, sep);
    if (str == NULL || *str == '\0') {
      break;
    }
    str++;
  }
}

