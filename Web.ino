const char HTTP_TITLE_LABEL[] PROGMEM = "<div class='l lt'><label>{v}</label><hr /></div>";
const char HTTP_FW_LABEL[] PROGMEM = "<div class='l c k'><label>Firmware: {fw}</label></div>";
const char HTTP_NEWFW_BUTTON[] PROGMEM = "<div><input class='fwbtn' id='fwbtn' type='button' value='Neue Firmware verf&uuml;gbar' onclick=\"window.open('{fwurl}')\" /></div><div><input class='fwbtn' id='fwbtnupdt' type='button' value='Firmwaredatei einspielen' onclick=\"window.location.href='/update'\" /></div>";
const char HTTP_CONFIG_BUTTON[] PROGMEM = "<div></div><hr /><div></div><div><input class='lnkbtn' type='button' value='Konfiguration' onclick=\"window.location.href='/config'\" /></div>";
const char HTTP_CONF[] PROGMEM = "<div><label>Ger&auml;tename: (CUxD-Name)</label></div><div><input type='text' id='devicename' name='devicename' pattern='[A-Za-z0-9_ -]+' placeholder='Ger&auml;tename' value='{dn}'></div><div><label>{st}:</label></div><div><input type='text' id='ccuip' name='ccuip' pattern='((^|\\.)((25[0-5])|(2[0-4]\\d)|(1\\d\\d)|([1-9]?\\d))){4}$' maxlength=16 placeholder='{st}' value='{ccuip}'></div><div><label>Motorumschaltzeit<br>(0.50 - 5.00 Sek.):</label></div><div><input type='text' id='mst' name='mst' pattern='[0-9.]+' placeholder='Motorumschaltzeit' value='{mst}'></div><div><label>Fahrzeit hoch<br>(0.10 - 600.00 Sek):</label></div><div><input type='text' id='up' name='up' pattern='[0-9.]+' placeholder='Fahrzeit hoch' value='{up}'></div><div><label>Fahrzeit runter<br>(0.10 - 600.00 Sek):</label></div><div><input type='text' id='down' name='down' pattern='[0-9.]+' placeholder='Fahrzeit hoch' value='{down}'></div><div><label>Fahrten bis zur automatischen<br>Kalibrierfahrt (0 - 255)</label></div><div><input type='text' id='cal' name='cal' pattern='[0-9]+' placeholder='Kalibrierung nach x Fahrten' value='{cal}'></div>";
const char HTTP_CONF_LOX[] PROGMEM = "<div><label>UDP Port:</label></div><div><input type='text' id='lox_udpport' pattern='[0-9]{1,5}' maxlength='5' name='lox_udpport' placeholder='UDP Port' value='{udp}'></div>";
const char HTTP_STATUSLABEL[] PROGMEM = "<div class='l c'>{sl}</div>";
const char HTTP_HOME_BUTTON[] PROGMEM = "<div><input class='lnkbtn' type='button' value='Zur&uuml;ck' onclick=\"window.location.href='/'\" /></div>";
const char HTTP_SAVE_BUTTON[] PROGMEM = "<div><button name='btnSave' value='1' type='submit'>Speichern</button></div>";
const char HTTP_UPDOWN_BUTTONS[] PROGMEM = "<span class='l'><div><button name='btnAction' onclick='SetState(\"/set?val=100\"); return false;'>HOCH</button></div><div class='l ls'><label id='_ls'>{ls}%</label></div><div><button name='btnAction' onclick='SetState(\"/set?val=0\"); return false;'>RUNTER</button></div><hr /><div><button class='stpbtn' name='btnAction' onclick='SetState(\"/set?val=stop\"); return false;'>STOP</button></div></span><hr /><div><button class='btnWert' name='btnAction' onclick='SetState(\"/set?val=\"+document.getElementById(\"shutter_value\").value); return false;'>FAHRE ZU WERT</button><input class='i' type='text' id='shutter_value' name='shutter_value' placeholder='Wert %' pattern='[0-9]{1,3}' value='' maxlength='3'></div>";

void initWebServer() {
  WebServer.on("/set", webSetValue);
  WebServer.on("/getState", []() {
    sendDefaultWebCmdReply();
  });
  WebServer.on("/config", configHtml);
  WebServer.on("/bootConfigMode", setBootConfigMode);
  WebServer.on("/reboot", []() {
    WebServer.send(200, "text/plain", "rebooting");
    ESP.restart();
  });
  WebServer.on("/restart", []() {
    WebServer.send(200, "text/plain", "rebooting");
    ESP.restart();
  });
  httpUpdater.setup(&WebServer);
  WebServer.onNotFound(defaultHtml);
  WebServer.begin();
}

void webSetValue() {
  if (WebServer.args() > 0) {
    for (int i = 0; i < WebServer.args(); i++) {
      if (WebServer.argName(i) == "val")
        if (WebServer.arg(i) != "")  {
          if (WebServer.arg(i) == "stop") {
            switch_dual_relay(DIRECTION_NONE);
            switch_dual_relay(DIRECTION_NONE);
            break;
          }
          int val = atoi(WebServer.arg(i).c_str());
          val = val;
          DEBUG("Web val = "+String(val));
          if (val >= 0 && val <= 100)
            ShutterControl(val);
        }
    }
  }
  sendDefaultWebCmdReply();
}

void defaultHtml() {
  String page = FPSTR(HTTP_HEAD);
  //page += FPSTR(HTTP_SCRIPT);
  page += FPSTR(HTTP_ALL_STYLE);
  if (GlobalConfig.BackendType == BackendType_HomeMatic)
    page += FPSTR(HTTP_HM_STYLE);
  if (GlobalConfig.BackendType == BackendType_Loxone)
    page += FPSTR(HTTP_LOX_STYLE);
  page += FPSTR(HTTP_HEAD_END);
  page += F("<div class='fbg'>");

  //page += F("<form method='post' action='control'>");
  page += FPSTR(HTTP_TITLE_LABEL);
  page += FPSTR(HTTP_UPDOWN_BUTTONS);

  page.replace("{v}", GlobalConfig.DeviceName);
  page.replace("{ls}", String(ShutterConfig.CurrentValue));

  page += FPSTR(HTTP_CONFIG_BUTTON);

  page += FPSTR(HTTP_FW_LABEL);

  page += FPSTR(HTTP_NEWFW_BUTTON);
  String fwurl = FPSTR(GITHUB_REPO_URL);
  String fwjsurl = FPSTR(GITHUB_REPO_URL);
  fwurl.replace("api.", "");
  fwurl.replace("repos/", "");
  page.replace("{fwurl}", fwurl);
  page += F("</div><script>");
  page += FPSTR(HTTP_CUSTOMSCRIPT);
  page += FPSTR(HTTP_CUSTOMUPDATESCRIPT);
  page.replace("{fwjsurl}", fwjsurl);
  page.replace("{fw}", FIRMWARE_VERSION);

  page += F("</script></div></body></html>");
  WebServer.sendHeader("Content-Length", String(page.length()));
  WebServer.send(200, "text/html", page);
}

void configHtml() {
  bool sc = false;
  bool saveSuccess = false;
  bool showHMDevError = false;
  if (WebServer.args() > 0) {
    for (int i = 0; i < WebServer.args(); i++) {
      if (WebServer.argName(i) == "btnSave")
        sc = (WebServer.arg(i).toInt() == 1);
      if (WebServer.argName(i) == "ccuip")
        strcpy(GlobalConfig.ccuIP, WebServer.arg(i).c_str());
      if (WebServer.argName(i) == "devicename")
        strcpy(GlobalConfig.DeviceName, WebServer.arg(i).c_str());
      if (WebServer.argName(i) == "lox_udpport")
        strcpy(LoxoneConfig.UDPPort, WebServer.arg(i).c_str());
      if (WebServer.argName(i) == "mst")
        ShutterConfig.MotorSwitchingTime = strtod(WebServer.arg(i).c_str(), NULL);
      if (WebServer.argName(i) == "down")
        ShutterConfig.DriveTimeDown = strtod(WebServer.arg(i).c_str(), NULL);
      if (WebServer.argName(i) == "up")
        ShutterConfig.DriveTimeUp = strtod(WebServer.arg(i).c_str(), NULL);
      if (WebServer.argName(i) == "cal")
        ShutterConfig.DriveUntilCalib = atoi(WebServer.arg(i).c_str());
    }
    if (sc) {
      saveSuccess = saveSystemConfig();
      if (GlobalConfig.BackendType == BackendType_HomeMatic) {
        String devName = getStateCUxD(GlobalConfig.DeviceName, "Address") ;
        if (devName != "null") {
          showHMDevError = false;
          HomeMaticConfig.ChannelName =  "CUxD." + devName;
        } else {
          showHMDevError = true;
        }
      }
    }
  }
  String page = FPSTR(HTTP_HEAD);

  //page += FPSTR(HTTP_SCRIPT);
  page += FPSTR(HTTP_ALL_STYLE);
  if (GlobalConfig.BackendType == BackendType_HomeMatic)
    page += FPSTR(HTTP_HM_STYLE);
  if (GlobalConfig.BackendType == BackendType_Loxone)
    page += FPSTR(HTTP_LOX_STYLE);
  page += FPSTR(HTTP_HEAD_END);
  page += F("<div class='fbg'>");
  page += F("<form method='post' action='config'>");
  page += FPSTR(HTTP_TITLE_LABEL);
  page += FPSTR(HTTP_CONF);

  if (GlobalConfig.BackendType == BackendType_HomeMatic) {
    page.replace("{st}", "CCU2 IP");
  }
  if (GlobalConfig.BackendType == BackendType_Loxone) {
    page += FPSTR(HTTP_CONF_LOX);
    page.replace("{st}", "MiniServer IP");
    page.replace("{udp}", LoxoneConfig.UDPPort);
  }

  page.replace("{dn}", GlobalConfig.DeviceName);
  page.replace("{ccuip}", GlobalConfig.ccuIP);

  page.replace("{mst}", String(ShutterConfig.MotorSwitchingTime));
  page.replace("{up}", String(ShutterConfig.DriveTimeUp));
  page.replace("{down}", String(ShutterConfig.DriveTimeDown));
  page.replace("{cal}", String(ShutterConfig.DriveUntilCalib));


  page += FPSTR(HTTP_STATUSLABEL);

  if (sc && !showHMDevError) {
    if (saveSuccess) {
      page.replace("{sl}", F("<label class='green'>Speichern erfolgreich.</label>"));
    } else {
      page.replace("{sl}", F("<label class='red'>Speichern fehlgeschlagen.</label>"));
    }
  }

  if (showHMDevError)
    page.replace("{sl}", F("<label class='red'>Ger&auml;tenamen in CUxD pr&uuml;fen!</label>"));

  if (!sc && !showHMDevError)
    page.replace("{sl}", "");

  page += FPSTR(HTTP_SAVE_BUTTON);
  page += FPSTR(HTTP_HOME_BUTTON);
  page += FPSTR(HTTP_FW_LABEL);
  page.replace("{fw}", FIRMWARE_VERSION);

  page += F("</form></div>");
  page += F("</div></body></html>");
  page.replace("{v}", GlobalConfig.DeviceName);

  WebServer.send(200, "text/html", page);
}


void sendDefaultWebCmdReply() {
  String reply =  "{\"state\": \"" + String(ShutterConfig.CurrentValue) + "\"}";
  //DEBUG("Sending Web-Reply: " + reply);
  WebServer.send(200, "application/json", reply);
}
