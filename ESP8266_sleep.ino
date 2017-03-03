#include "ESP8266WiFi.h"

void setup()
{
  pinMode(D4, OUTPUT); 
  Serial.begin(74880);
  delay(100);
  Serial.println();
  Serial.println(ESP.getResetReason());
  Serial.println();
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
}

void loop() {
    Serial.println("Woke up........................");
    digitalWrite(D4, LOW); //LED on
    listNetworks();
    Serial.println("Back to sleep..................");
    delay(1000); //delay 1sec for measureing
    digitalWrite(D4, HIGH); //LED off
    ESP.deepSleep(30  * 1000 * 1000);
    delay(100);
}

//List available scanned networks
void listNetworks()
{
    int n = WiFi.scanNetworks();
    if (n == 0)
        Serial.println("no networks found");
    else {
        Serial.print(n);
        Serial.println(" networks found");
        for (int i = 0; i < n; ++i) {
            Serial.print(i + 1);
            Serial.print(": ");
            Serial.print(WiFi.SSID(i));
            Serial.print(" (rssi:");
            Serial.print(WiFi.RSSI(i));
            Serial.print(")");
            Serial.println((WiFi.encryptionType(i) == ENC_TYPE_NONE)?" ":"*");
            }
        }
}
