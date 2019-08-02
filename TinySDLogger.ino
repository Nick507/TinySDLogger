
#include "TinySDLogger.h"

TinySDLog SDLog;

void setup() 
{  
  Serial.begin(9600);
    
  Serial.print(F("\nInitialize TinySDLog..."));
  TinySDLog::ResultCode res = SDLog.init();
  if(res)
  {
    Serial.print(F("failed with result code: "));
    Serial.print(res);
    return;
  }
  
  Serial.print(F("OK\nStart writing loop\n"));
  for(int i = 0; i < 100; i++)
  {
    if(!SDLog.writeTimestamp()) Serial.print(F("Failed to read RTC\n"));
    SDLog.print(F("This is a TinySDLogger test line: "));
    SDLog.print(i);
    SDLog.print(F("\n"));
  }
  Serial.print(F("Stop writing loop\nClose TinySDLog..."));
  
  res = SDLog.close();
  if(res)
  {
    Serial.print(F("failed with result code: "));
    Serial.print(res);
    return;
  }
  Serial.print(F("OK\n"));
}

void loop() 
{
}
