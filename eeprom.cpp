//#include "global.h"
#include <Arduino.h>
#include <EEPROM.h>
//#include <EDB.h>
//#define DEBUGLOG(...)
#define DEBUGLOG(...)    Serial.printf(__VA_ARGS__)

/*
 * ---------------------------
 * version
 * appid[16]
 * ---------------------------
 * tst,bat,lac,cid,lat,lon
 * ...
 * ---------------------------
 */
#define EE_SIZE    4096
struct EESETTING {
    int  iVersion;
    char sAppid[16];
    };
//---------------------------1234567890123456
EESETTING myPROGsetting = {
    1,
    "Tracker99 V1"
    };

void eepromInitialize()
{
   DEBUGLOG("\nEEPROM - Initializing");

   for(int i = 0; i < EE_SIZE; ++i){
       EEPROM.write(i, 0);
       yield();
       }
   EEPROM.commit();
}

void eepromSaveSetting()
{
   EEPROM.put(0, myPROGsetting);
   EEPROM.commit();
}

void eepromLoadSetting()
{
   EESETTING eeSetting;

   EEPROM.get(0, eeSetting);
   if(eeSetting.iVersion != myPROGsetting.iVersion){
      DEBUGLOG("\nEEPROM version changes from %d to %d, Initializing...\n", eeSetting.iVersion, myPROGsetting.iVersion);
      eepromInitialize();
      eepromSaveSetting();
      EEPROM.get(0, eeSetting); //read again afrer initialize
      }
   DEBUGLOG("\nEEPROM setting version=%d, appid=%s\n", eeSetting.iVersion, eeSetting.sAppid);
   //for (int i=0; i<32; i++) DEBUGLOG("%02X", (byte)EEPROM.read(i));
}
