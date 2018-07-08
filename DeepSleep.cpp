#include <Arduino.h>
//#include <EEPROM.h>

extern "C" {
#include "gpio.h"
#include "user_interface.h"
}

//#define DEBUGLOG(...)
#define DEBUGLOG(...)    Serial.printf(__VA_ARGS__)

#define _LED2_ON                digitalWrite(2, LOW)
#define _LED2_OFF               digitalWrite(2, HIGH)
#define _LED2_TOGGLE            digitalWrite(2, !digitalRead(2))

#define _GPIO15_VOUT_ENABLE     digitalWrite(15, HIGH)
#define _GPIO15_VOUT_DISABLE    digitalWrite(15, LOW)
#define _LED4_ON                digitalWrite(4, LOW)
#define _LED4_OFF               digitalWrite(4, HIGH)
#define _LED4_TOGGLE            digitalWrite(4, !digitalRead(4))
#define _LED5_ON                digitalWrite(5, LOW)
#define _LED5_OFF               digitalWrite(5, HIGH)
#define _LED5_TOGGLE            digitalWrite(5, !digitalRead(5))

struct
{
   uint16_t DEAD;    //0 1
   uint16_t counter; //2 3
   uint16_t cmd;     //4 5
   uint16_t dummy;   //6 7
   uint16_t BEEF;    //8 9
   //byte     data[16]; //512 bytes available include crc32 and counter
} rtcData;

void eepromLoadSetting();

/*
 *  lightsleep1s()
 *      Light Sleep for 1sec
 */
void lightsleep1s()
{
   wifi_set_opmode(NULL_MODE);
   wifi_fpm_set_sleep_type(LIGHT_SLEEP_T);  // set sleep type
   wifi_fpm_open();                         // Enables force sleep
   wifi_fpm_do_sleep(1e6);                  // Sleep for 1 sec
   delay(1001);                             // Delay 1000ms+1
}

/*
 * checkButton()
 *    return  0: nill
 *           >0: pressed time (ms)
 */
int checkButton(const int pin, int hiloState)
{
   //pinMode(pin, INPUT);
   //Check button --------------------------------------------------------------
   if(digitalRead(pin) == hiloState){ //GROVE BUTTON HIGH when pressed
      uint32_t iBtn0 = millis();
      uint32_t iBtnPressed;
      while(digitalRead(pin) == hiloState){
            _LED2_ON;
            delay(30);
            _LED2_OFF;
            delay(30);
            iBtnPressed = millis() - iBtn0;
            if(iBtnPressed > 5000){ //5000ms
               _LED2_ON;
               while(digitalRead(pin) == hiloState){
                     }               //Hold untill released button
               }
            }
      //---------------------------------------------------------------------------
      iBtnPressed = millis() - iBtn0;
      //DEBUGLOG("\nButton Pressed for %dms!", iBtnPressed);
      return(iBtnPressed);
      }
   return(0);
}

//ADC_MODE(ADC_VCC);// to use getVcc
//==============================================================================
void setup()
{
   //Init GPIO---------------
   _LED2_ON;
   pinMode(2, OUTPUT);     //ESP12E LED

   _GPIO15_VOUT_DISABLE;
   pinMode(15, OUTPUT);    //TRACKER99 VOUT
   _LED4_OFF;
   pinMode(4, OUTPUT);     //TRACKER99 LED 3V3
   _LED5_OFF;
   pinMode(5, OUTPUT);     //TRACKER99 LED VSYS

   //pinMode(0, INPUT); //NodeMCU, TRACKER99
   //pinMode(5, INPUT); //WIO NODE
   pinMode(12, INPUT_PULLUP);

   //------------------------
   Serial.begin(74800);
   DEBUGLOG("\n\nRESET:%s (@%dms)", ESP.getResetReason().c_str(), millis());
   //Load EEPROM----------------------------------------------------------------
   //EEPROM.begin(4096);
   //eepromLoadSetting();
   //DEBUGLOG("EEPROM (@%dms)", millis());
   //---------------------------------------------------------------------------
   //---------------------------------------------------------------------------
   ESP.rtcUserMemoryRead(0, (uint32_t *)&rtcData, sizeof(rtcData));
   DEBUGLOG("\nRTC Memory:%04X %04X %04X %04X %04X", rtcData.DEAD, rtcData.counter, rtcData.cmd, rtcData.dummy, rtcData.BEEF);
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
   //---------------------------------------------------------------------------
   DEBUGLOG("\nWakeUp Counter:%d\n", rtcData.counter);
   //int iADC = (analogRead(A0) / (1024 * 0.1803)) * 10;
   int   iADC = analogRead(A0);
   float fVCC = iADC / 205.0;
   DEBUGLOG("\nADC=%d(", iADC);
   Serial.print(fVCC);
   Serial.println("V)");

   rtcData.cmd = 0;
   ESP.rtcUserMemoryWrite(0, (uint32_t *)&rtcData, sizeof(rtcData));  //Save rtcData

   DEBUGLOG("@%d", millis());
   DEBUGLOG("PIN12=%d\n", digitalRead(12));
   Serial.flush();
   //---------------------------------------------------------------------------
   if(digitalRead(12) == LOW){
      DEBUGLOG("***");
      }
   else{
       _LED2_OFF;
       //DeepSleep again
       ESP.deepSleep(5e6, WAKE_RF_DISABLED); //normal sleep 10sec with no RF
       }
}

void loop()
{
   int iDeltaTime;
   int iButtonPressedTime;

   //============================================================================
   _LED2_ON;
   while (digitalRead(12)==LOW) {
      DEBUGLOG("%d", digitalRead(12));
      _LED2_TOGGLE;
      delay(30);

      yield();
      }

/*
   iButtonPressedTime = checkButton(12, LOW);
   if(iButtonPressedTime > 5000){   //Long Press
      DEBUGLOG("\nLong!!! %d", iButtonPressedTime);
      }
   else if(iButtonPressedTime > 60){   //Short Press
           DEBUGLOG("\nShort! %d", iButtonPressedTime);
           }
   else{   //No Press
       }
*/
   //===========================================================================
/*   iDeltaTime = millis() / 1000;
   if(iDeltaTime % 60 == 0){
      DEBUGLOG("\n%03d", iDeltaTime);
      }
   else{
       DEBUGLOG(".");
       }
*/
   Serial.flush();
   //===========================================================================
   _LED2_OFF;
   //lightsleep1s();
   ESP.deepSleep(10, WAKE_RF_DISABLED); //normal sleep 10sec with no RF

}
