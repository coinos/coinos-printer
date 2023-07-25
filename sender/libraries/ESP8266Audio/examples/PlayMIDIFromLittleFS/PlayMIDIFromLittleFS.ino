#include <Arduino.h>
#ifdef ESP32
    void setup() {
        Serial.begin(115200);
        Serial.printf("ERROR - ESP32 does not support LittleFS\n");
    }
    void loop() {}
#else
#if defined(ARDUINO_ARCH_RP2040)
    #define WIFI_OFF
    class __x { public: __x() {}; void mode() {}; };
    __x WiFi;
#else
    #include <ESP8266WiFi.h>
#endif
#include <AudioOutputI2S.h>
#include <AudioGeneratorMIDI.h>
#include <AudioFileSourceLittleFS.h>
#include <AudioGeneratorMP3.h>

AudioFileSourceLittleFS *sf2;
AudioFileSourceLittleFS *mid;
AudioFileSourceLittleFS *mp3file;
AudioOutputI2S *dac;
AudioGeneratorMIDI *midi;
AudioGeneratorMP3 *mp3;

void setup()
{
  /* const char *soundfont = "/1mgm.sf2"; */
  /* const char *midifile = "/furelise.mid"; */

  WiFi.mode(WIFI_OFF); 

  Serial.begin(115200);
  Serial.println("Starting up...\n");

  audioLogger = &Serial;
  /* sf2 = new AudioFileSourceLittleFS(soundfont); */
  /* mid = new AudioFileSourceLittleFS(midifile); */
  mp3file = new AudioFileSourceLittleFS("/output.mp3");

  dac = new AudioOutputI2S();
  mp3 = new AudioGeneratorMP3();
  /* midi = new AudioGeneratorMIDI(); */
  /* midi->SetSoundfont(sf2); */
  /* midi->SetSampleRate(22050); */
  Serial.printf("BEGIN...\n");
  mp3->begin(mp3file, dac);
}

void loop()
{
  if (mp3->isRunning()) {
    if (!mp3->loop()) {
      mp3->stop();
    }
  } else {
    Serial.printf("MP3 done\n");
    delay(1000);
  }
}

#endif
