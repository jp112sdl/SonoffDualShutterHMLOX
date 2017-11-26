int calculateDriveTime(byte to, byte from, byte DIRECTION) {
  byte diff = abs(from - to);
  int dt_millis = 0;
  switch (DIRECTION) {
    case DIRECTION_UP:
      dt_millis = diff * (ShutterConfig.DriveTimeUp * 1000) / 100;
      if (to == 100) dt_millis = dt_millis + EXTRADRIVETIMEFORENDPOSTIONMILLIS;
      DEBUG("Drive Time UP = " + String(dt_millis) + "ms");
      break;
    case DIRECTION_DOWN:
      dt_millis = diff * (ShutterConfig.DriveTimeDown * 1000) / 100;
      if (to == 0) dt_millis = dt_millis + EXTRADRIVETIMEFORENDPOSTIONMILLIS;
      DEBUG("Drive Time DOWN = " + String(dt_millis) + "ms");
      break;
  }
  return dt_millis;
}

byte calculatePercent(byte startValue, byte DIRECTION) {
  int x = 0;
  switch (DIRECTION) {
    case DIRECTION_UP:
      x = (millis() - dual_relay_switched_millis) / (ShutterConfig.DriveTimeUp * 10);
      x = startValue + x;
      if (x > 100) x = 100;
      return x;
    case DIRECTION_DOWN:
      x = (millis() - dual_relay_switched_millis) / (ShutterConfig.DriveTimeDown * 10);
      x = startValue - x;
      if (x < 0) x = 0;
      return x;
    default:
      return startValue;
  }
}

void printWifiStatus() {
  DEBUG("SSID: " + WiFi.SSID());
  DEBUG("IP Address: " + IpAddress2String(WiFi.localIP()));
  DEBUG("Gateway Address: " + IpAddress2String(WiFi.gatewayIP()));
  DEBUG("signal strength (RSSI):" + String(WiFi.RSSI()) + " dBm");
}

void switchLED(bool State) {
  digitalWrite(LEDPin, State);
}

void blinkLED(int count) {
  byte oldState = digitalRead(LEDPin);
  delay(100);
  for (int i = 0; i < count; i++) {
    switchLED(!oldState);
    delay(100);
    switchLED(oldState);
    delay(100);
  }
  delay(200);
}

String IpAddress2String(const IPAddress& ipAddress) {
  return String(ipAddress[0]) + String(".") + \
         String(ipAddress[1]) + String(".") + \
         String(ipAddress[2]) + String(".") + \
         String(ipAddress[3]);
}

