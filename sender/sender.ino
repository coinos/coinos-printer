#include "defines.h"
#include <ArduinoOTA.h>
#include <Arduhdlc.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <WebSockets2_Generic.h>
#include <ESP8266SAM.h>
#include <AudioOutputI2S.h>
#include <DollarsToWords.h>
#include <math.h>

#include "AudioFileSourceLittleFS.h"
#include "AudioGeneratorMP3.h"

#define MAX_HDLC_FRAME_LENGTH 512

Arduhdlc hdlc(&send_character, &hdlc_frame_handler, MAX_HDLC_FRAME_LENGTH);
websockets2_generic::WebsocketsClient client;

AudioOutputI2S *out;
AudioGeneratorMP3 *mp3;
AudioFileSourceLittleFS *mp3file;

void setup()
{
  Serial.begin(115200);

  while (!Serial && millis() < 5000);

  audioLogger = &Serial;
  // mp3file = new AudioFileSourceLittleFS("/output.mp3");
  out = new AudioOutputI2S();

  // bool SetPinout(int bclkPin, int wclkPin, int doutPin);
  out->SetPinout(4, 5, 2);

  // mp3 = new AudioGeneratorMP3();
  Serial.printf("BEGIN...\n");

  client.onMessage(onMessageCallback);
  client.onEvent(onEventsCallback);
  client.setFingerprint(fingerprint);

  connect();
  ArduinoOTA.begin();

  ESP8266SAM *sam = new ESP8266SAM;
  sam->Say(out, "I'm ready!");
  // delay(200);
  // sam->Say(out, DollarsToWords::convertToWords(round((int)d["amount"] * (float)d["rate"] * 100.0 / 100000000) / 100.0).c_str());
  delete sam;
}

int period = 4000;
unsigned long time_now = 0;
int failures = 0;

void loop()
{
  // if (mp3->isRunning()) {
  //   if (!mp3->loop()) {
  //     mp3->stop();
  //   }
  // }

  ArduinoOTA.handle();

  if (WiFi.status() == WL_CONNECTED){
    client.poll();
  }

  if ((unsigned long)(millis() - time_now) > period) {
    connect();
    time_now = millis();
  } 
}

void connect() {
  bool socketConnected;

  if (WiFi.status() != WL_CONNECTED) {
    client.close(websockets2_generic::CloseReason_NormalClosure);
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    WiFi.disconnect();
    delay(1000);
    WiFi.begin(ssid, password);
    delay(5000);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Connected to Wi-Fi network with IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Not connected to Wi-Fi network");
  }

  if (client.available()) {
    String msg = "{\"type\":\"heartbeat\",\"data\":\"" + String(token) + "\"}";

    client.send(msg);

  } else {
    Serial.println("No socket");
    failures++;

    socketConnected = client.connect(websockets_connection_string);

    if (socketConnected) {
      Serial.println("Socket Connected!");

      String msg = "{\"type\":\"login\",\"data\":\"" + String(token) + "\"}";

      client.send(msg);

    } else {
      Serial.println("Socket Not Connected!");
      client.close(websockets2_generic::CloseReason_NormalClosure);
    }
  } 

  if (failures > 5) ESP.restart();
}


void send_character(uint8_t data) {
  Serial.print((char)data);
}

void hdlc_frame_handler(const uint8_t *data, uint16_t length) {};



void onMessageCallback(websockets2_generic::WebsocketsMessage message)
{
  String json = message.data();
  Serial.println(json);
  if (json.indexOf("payment") != -1) {
    StaticJsonDocument<1000> doc;
    deserializeJson(doc, json);
    JsonObject d = doc["data"];
    String s = String(d["amount"]) + ":" + String(d["tip"]) + ":" + String(d["rate"]) + ":" + String(d["created"]);
    char charBuffer[s.length() + 1];
    s.toCharArray(charBuffer, MAX_HDLC_FRAME_LENGTH);
    hdlc.sendFrame((uint8_t*)charBuffer, s.length());
    // mp3->begin(mp3file, out);


    ESP8266SAM *sam = new ESP8266SAM;
    sam->Say(out, "Bitcoin payment received!");
    delay(200);
    sam->Say(out, DollarsToWords::convertToWords(round((int)d["amount"] * (float)d["rate"] * 100.0 / 100000000) / 100.0).c_str());
    delete sam;
  }
}

void onEventsCallback(websockets2_generic::WebsocketsEvent event, String data)
{
  (void) data;

  if (event == websockets2_generic::WebsocketsEvent::ConnectionOpened)
  {
    Serial.println("Connnection Opened");
  }
  else if (event == websockets2_generic::WebsocketsEvent::ConnectionClosed)
  {
    Serial.println("Connnection Closed");
  }
}
