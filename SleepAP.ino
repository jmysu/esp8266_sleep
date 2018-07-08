/*
 *
 * SleepAP
 *
 */
 #include <Arduino.h>

 #ifdef ESP8266
   #include <ESP8266WiFi.h>
   #include <ESP8266WebServer.h>
   #include <ESP8266mDNS.h>
extern ESP8266WebServer server;
 #else
   #include <WiFi.h>
   #include <WebServer.h>
   #include <ESPmDNS.h>
extern WebServer server;
 #endif

 #include <WiFiClient.h>
 #include <WiFiUdp.h>
 #include <FS.h>
 #include <ArduinoOTA.h>
 #include <DNSServer.h>
 #include <Ticker.h>

//WakeUp
extern "C" {
#include "user_interface.h"
#include "gpio.h"
}

#include <Hash.h>
#include <WebSocketsServer.h>


WebSocketsServer webSocket = WebSocketsServer(81);
//WebSocketsServer webSocket = WebSocketsServer("/ws");
// DNS server
const byte DNS_PORT = 53;
DNSServer  dnsServer;

#define DBG_OUTPUT_PORT    Serial
#define DEBUGLOG(...)    DBG_OUTPUT_PORT.printf(__VA_ARGS__)


#define BUTTON_GPIO5        5
//SLEEP_SECONDS, Timer for takeing actin every 300s
#define SLEEP_SECONDS       300
//AP_IDEL_SECONDS, Timer to go sleep for AP idle time 30s
#define AP_IDLE_SECONDS     30

#define WAKEUP_CMD_BTN      0xC001
#define WAKEUP_CMD_AP       0xC00A
#define WAKEUP_CMD_TIMER    0xC060


struct
{
   uint16_t DEAD;    //0 1
   uint16_t counter; //2 3
   uint16_t cmd;     //4 5
   uint16_t dummy;   //6 7
   uint16_t BEEF;    //8 9
   //byte     data[16]; //512 bytes available include crc32 and counter
} rtcData;

int    apClients; //count AP clients
String APname;
//------------------------------------------------------------------------------
void flashLED(int pin, int times, int delayTime)
{
   int oldState = digitalRead(pin);

   for(int i = 0; i < times; i++){
       digitalWrite(pin, LOW);  // Turn on LED
       delay(delayTime);
       digitalWrite(pin, HIGH); // Turn on LED
       delay(delayTime);
       }
   digitalWrite(pin, oldState);  // Turn on LED
}

void toggleLED(int pin)
{
   digitalWrite(pin, !digitalRead(pin));
}

//------------------------------------------------------------------------------
//100ms Timer Ticker
//------------------------------------------------------------------------------
Ticker ticker;
int    iTickerCount = 0;
int    iAPidleCount = 0;
bool   bSleepNow    = false;
void Ticker_100ms()
{
   static int iBtnCount      = 0;
   static int iBlinkInterval = 5; //500ms

   iTickerCount++;
   if(apClients > 0){       //LED ON
      digitalWrite(2, LOW); //Toggle lED ON
      iAPidleCount = 0;
      }
   else{                        //No client, Blinking
       if((iTickerCount % iBlinkInterval) == 0){
          digitalWrite(2, LOW); //Toggle lED ON
          DEBUGLOG("\nIDLE:%d", iAPidleCount);
          }
       else{
           digitalWrite(2, HIGH); //Toggle lED OFF
           }
       iAPidleCount++;
       if(iAPidleCount > AP_IDLE_SECONDS * 10){
          bSleepNow = true;
          }
       }
}

//RF_MODE(RF_DISABLED);   //NO RF
ADC_MODE(ADC_VCC);// to use getVcc


void cmd_sms()
{

//Serial.print("\nChecking SIMCARD ...");
//cmdAT("AT+CPIN?", "+CPIN: READY", 500);
//cmdAT("AT+COPS?", "OK", 1000);
//cmdAT("AT+CGMM", "OK", 1000);
//cmdAT("AT+CGSN", "OK", 1000); //IMEI
//cmdAT("AT+CIMI", "OK", 1000); //IMSI
Serial.println("Connecting to the network...");
while( (cmdAT("AT+CREG?", "+CREG: 0,1", 500) ||
          cmdAT("AT+CREG?", "+CREG: 0,5", 500)) == 0 );
Serial.print("Setting SMS mode...");
cmdAT("AT+CMGF=1", "OK", 1000);    // sets the SMS mode to text

Serial.print("\nWaiting SMS come in...");
//-----------------------------------------------------
Simcom_sendSMS("0932902190", "Hello,\nAnother SMS Test!!!");
//-----------------------------------------------------
}

//------------------------------------------------------------------------------
void setup()
{
   pinMode(5, INPUT);
   pinMode(4, INPUT);
   pinMode(2, INPUT);
   pinMode(0, INPUT);

   Serial.begin(115200);
   Serial.println();
   delay(100);
   DEBUGLOG("\n\nRESET REASON:");
   DEBUGLOG(ESP.getResetReason().c_str());
   //---------------------------------------------------------------------------
   ESP.rtcUserMemoryRead(0, (uint32_t *)&rtcData, sizeof(rtcData));
   if((rtcData.DEAD != 0xDEAD) || (rtcData.BEEF != 0xBEEF)){
      //Init RTC Memory
      rtcData.DEAD    = 0xDEAD;
      rtcData.counter = 0;
      rtcData.cmd     = 0;
      rtcData.dummy   = 0;
      rtcData.BEEF    = 0xBEEF;
      }
   else{
       rtcData.counter++;
       }
   DEBUGLOG("\nRTC Memory:%04X %04X %04X %04X %04X", rtcData.DEAD, rtcData.counter, rtcData.cmd, rtcData.dummy, rtcData.BEEF);
   //---------------------------------------------------------------------------
   uint32_t getVcc = ESP.getVcc();
   //DEBUGLOG("\nVCC=%d", getVcc);
   float fVCC = getVcc / 1000.0;
   DEBUGLOG("\nVCC=");
   Serial.println(fVCC);
   //Check button --------------------------------------------------------------
   if(digitalRead(BUTTON_GPIO5) == HIGH){   //GROVE BUTTON HIGH when pressed
      uint32_t iBtn0 = millis();
      pinMode(2, OUTPUT);
      uint32_t iBtnPressed;
      while(digitalRead(BUTTON_GPIO5) == HIGH){
            digitalWrite(2, LOW);  //Turn LED(GPIO2) ON
            delay(30);
            digitalWrite(2, HIGH); //Turn LED(GPIO2) ON
            delay(30);
            iBtnPressed = millis() - iBtn0;
            if(iBtnPressed > 6000){
               digitalWrite(2, LOW);  //Turn LED(GPIO2) ON
               while(digitalRead(BUTTON_GPIO5) == HIGH){
                     }                //Hold untill released button
               }
            }
      //---------------------------------------------------------------------------
      iBtnPressed = millis() - iBtn0;
      DEBUGLOG("\nButton Pressed for %dms!", iBtnPressed);
      if(iBtnPressed > 6000){
         rtcData.cmd = WAKEUP_CMD_AP;
         //Save RTC Memory and wakeup with RF
         ESP.rtcUserMemoryWrite(0, (uint32_t *)&rtcData, sizeof(rtcData));
         flashLED(2, 3, 100);
         ESP.deepSleep(10, WAKE_RFCAL);
         }
      else{
          rtcData.cmd = WAKEUP_CMD_BTN;
          //Save RTC Memory and Sleep
          ESP.rtcUserMemoryWrite(0, (uint32_t *)&rtcData, sizeof(rtcData));
          flashLED(2, 3, 100);
          ESP.deepSleep(10, WAKE_RF_DISABLED);
          }
      }
   //Check Sleeped Time----------------------------------------------------------
   if(rtcData.counter > SLEEP_SECONDS){
      rtcData.counter = 0;
      DEBUGLOG("\n!!! WakeUp and Do Something !!!\n");
      rtcData.cmd = WAKEUP_CMD_TIMER;
      //Save RTC Memory and Sleep
      ESP.rtcUserMemoryWrite(0, (uint32_t *)&rtcData, sizeof(rtcData));
      ESP.deepSleep(10, WAKE_RF_DISABLED);
      }


   //---------------------------------------------------------------------------
   //Branch here for different cmds
   //---------------------------------------------------------------------------
   //ESP.deepSleep(10, WAKE_RF_DISABLED);
   //ESP.deepSleep(10, WAKE_RFCAL);
   int cmd = rtcData.cmd;
   rtcData.cmd = 0;
   ESP.rtcUserMemoryWrite(0, (uint32_t *)&rtcData, sizeof(rtcData));

   switch(cmd){
       default:
          ESP.deepSleep(1e6, WAKE_RF_DISABLED); //normal sleep 1sec with no RF
          break;

       case WAKEUP_CMD_BTN:
          DEBUGLOG("\r\n---BTN---\r\n");
          pinMode(2, OUTPUT);
          flashLED(2, 3, 250);
          ESP.deepSleep(10, WAKE_RF_DISABLED);
          break;

       case WAKEUP_CMD_TIMER:
          DEBUGLOG("\r\n---Timer%ds---\r\n", SLEEP_SECONDS);
          pinMode(2, OUTPUT);
          flashLED(2, 3, 250);
          ESP.deepSleep(10, WAKE_RF_DISABLED);
          break;

       case WAKEUP_CMD_AP:
          DEBUGLOG("\r\n---softAP---\r\n");
          pinMode(2, OUTPUT);
          flashLED(2, 3, 250);
          setupSPIFF();
          if(!load_config()){  // Try to load configuration from file system
             defaultConfig();  // Load defaults if any error
             }
          //--------------------------------------------------------------------
          WiFi.mode(WIFI_AP);
          IPAddress ip(1, 1, ESP.getChipId() % 100, 1);
          IPAddress subnet(255, 255, 255, 0);
          WiFi.softAPConfig(ip, ip, subnet);
          APname = "AP-" + ip.toString();
          WiFi.softAP(APname.c_str()); //Access point is open
          Serial.print("\nAP Name:");
          Serial.println(APname);


          otaSetup();
          /* Setup the DNS server redirecting all the domains to the apIP */
          dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
          dnsServer.start(DNS_PORT, "*", ip);
          //--------------------------------------------------------------------
          //get heap status, analog input value and all GPIO statuses in one json call
          server.on("/all", HTTP_GET, []() {
         String json = "{";
         json += "\"heap\":" + String(ESP.getFreeHeap());
         //json += ", \"analog\":" + String(analogRead(A0));
         json += ", \"analog\":" + String(random(0, 1023));
         json += ", \"gpio\":" + String((uint32_t)(((GPI | GPO) & 0xFFFF) | ((GP16I & 0x01) << 16)));
         //jmysy 0317-----------------------------------------------------------
         json += ", \"temp\":" + String(random(0, 120));
         //---------------------------------------------------------------------
         json += ", \"adc\":" + String(analogRead(A0));
         json += ", \"but\":" + String(1 - digitalRead(0));
         //---------------------------------------------------------------------
         json += "}";
         server.send(200, "text/json", json);
         json = String();
      });
          //load color
          server.on("/color", HTTP_GET, []() {
         if(!handleFileRead("/color.html")){
            server.send(404, "text/plain", "FileNotFound");
            }
      });
          //-----------
          server.on("/admin/generalvalues", send_general_configuration_values_html);
          server.on("/admin/values", send_network_configuration_values_html);
          server.on("/admin/connectionstate", send_connection_state_values_html);
          //server.on("/admin/infovalues", send_information_values_html);
          //server.on("/admin/ntpvalues", send_NTP_configuration_values_html);
          server.on("/config.html", []() {
         send_network_configuration_html();
      });
          server.on("/general.html", []() {
         send_general_configuration_html();
      });
          //server.onNotFound(handleNotFound);----------------------------------
          server.onNotFound([]() {
         if(!handleFileRead(server.uri())){
            handleNotFound();
            }
      });
          //--------------------------------------------------------------------
          server.begin();
          DEBUGLOG("\nHTTP server started");

          ticker.attach(.1, Ticker_100ms);
          //--------------------------------------------------------------------
          //wsConsoleSetup();
          // start webSocket server
          //webSocket.begin();
          //webSocket.onEvent(webSocketEvent);
          break;
          }
   //---------------------------------------------------------------------------

}

void loop()
{
   ArduinoOTA.handle();
   dnsServer.processNextRequest();
   server.handleClient();
   //WebSocket
   webSocket.loop();
   //serialEvent();
   // add main program code here------------------------------------------------


   //Check Sleep or Not
   int iClients = wifi_softap_get_station_num();
   if(apClients != iClients){
      apClients = iClients;
      DEBUGLOG("\nClients change to:%d\n", apClients);
      }

   if(bSleepNow){
      ticker.detach();                     //Detach timer tickr
      bSleepNow    = false;
      iTickerCount = 0;
      iAPidleCount = 0;
      DEBUGLOG("\nGoing to sleep! Bye-Bye\n");
      ESP.deepSleep(10, WAKE_RF_DISABLED); //normal sleep 1sec with no RF
      }
}
