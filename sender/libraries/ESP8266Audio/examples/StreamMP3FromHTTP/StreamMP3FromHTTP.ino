#include <ESP8266WiFi.h>
#include "AudioFileSourceLittleFS.h"
#include "AudioFileSourceHTTPStream.h"
#include "AudioFileSourceBuffer.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"

// To run, set your ESP8266 build to 160MHz, update the SSID info, and upload.

// Enter your WiFi setup here:
#ifndef STASSID
#define STASSID "TELUS0800"
#define STAPSK  "6chvt42zv3"
#endif

const char* ssid = STASSID;
const char* password = STAPSK;

// Randomly picked URL
const char *URL="http://ln.coinos.io:9090/30eda8f4bb4ea0760e1a85f6218bd609886bc8e9e6a5878b7616a91c68e8fca6.mp3";

AudioGeneratorMP3 *mp3;
AudioFileSourceLittleFS *file;
AudioFileSourceHTTPStream *stream;
AudioFileSourceBuffer *buff;
AudioOutputI2S *out;

bool oneDone = false;

void setup()
{
  Serial.begin(115200);
  delay(1000);
  Serial.println("Connecting to WiFi");

  WiFi.disconnect();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);

  WiFi.begin(ssid, password);

  // Try forever
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("...Connecting to WiFi");
    delay(1000);
  }
  Serial.println("Connected");

  audioLogger = &Serial;

  const char *filename = "/received.mp3";
  file = new AudioFileSourceLittleFS(filename);
  Serial.printf("BEGIN...\n");
  out = new AudioOutputI2S();
  mp3 = new AudioGeneratorMP3();
  mp3->begin(file, out);
}


void loop()
{
  static int lastms = 0;

  if (mp3->isRunning()) {
    if (millis()-lastms > 1000) {
      lastms = millis();
      Serial.printf("Running for %d ms...\n", lastms);
      Serial.flush();
     }
    if (!mp3->loop()) mp3->stop();
  } else {
    if (!oneDone) {
      stream = new AudioFileSourceHTTPStream(URL);
      buff = new AudioFileSourceBuffer(stream, 4096);
      mp3->begin(buff, out);
      oneDone = true;
    }

    Serial.printf("MP3 done\n");
    delay(1000);
  }
}
