#include <Arduino.h>
#include <ESP8266SAM.h>
#include <AudioOutputI2S.h>

AudioOutputI2S *out = NULL;

void setup()
{
  Serial.begin(115200);
  out = new AudioOutputI2S();
  // out->SetPinout(4, 5, 2);
  out->begin();
}

void loop()
{
  ESP8266SAM *sam = new ESP8266SAM;
  sam->Say(out, "Bitcoin payment received.");
  delay(500);
  sam->Say(out, "One doller and forty five cents.");
  delete sam;
  Serial.println("Done");
}
