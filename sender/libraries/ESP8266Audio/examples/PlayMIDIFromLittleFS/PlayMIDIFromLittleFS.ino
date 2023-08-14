#include <Arduino.h>
#include <AudioOutputI2S.h>
#include <AudioGeneratorWAV.h>
#include <AudioFileSourceLittleFS.h>

AudioFileSourceLittleFS *file;
AudioOutputI2S *dac;
AudioGeneratorWAV *wav;

void setup()
{
  const char *filename = "/output.wav";

  Serial.begin(115200);
  delay(1000);
  Serial.println("Starting up...\n");

  audioLogger = &Serial;
  file = new AudioFileSourceLittleFS(filename);
  
  dac = new AudioOutputI2S();
  wav = new AudioGeneratorWAV();
  Serial.printf("BEGIN...\n");
  wav->begin(file, dac);
}

void loop()
{
  if (wav->isRunning()) {
    if (!wav->loop()) {
      wav->stop();
    }
  } else {
    Serial.printf("MIDI done\n");
    delay(1000);
  }
}

