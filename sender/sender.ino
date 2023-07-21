#include "defines.h"
#include <Arduhdlc.h>
#include <ArduinoJson.h>
#include <TimeLib.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <WebSockets2_Generic.h>
#include <ESP8266SAM.h>
#include <AudioOutputI2S.h>
#include <DollarsToWords.h>
#include <math.h>

#include "AudioFileSourceHTTPStream.h"
#include "AudioFileSourceBuffer.h"
#include "AudioGeneratorMP3.h"

#define MAX_HDLC_FRAME_LENGTH 512

Arduhdlc hdlc(&send_character, &hdlc_frame_handler, MAX_HDLC_FRAME_LENGTH);
websockets2_generic::WebsocketsClient client;

AudioOutputI2S *out = NULL;
AudioGeneratorMP3 *mp3;
AudioFileSourceHTTPStream *file;
AudioFileSourceBuffer *buff;

const char *URL="http://192.168.1.77:3000/output.mp3";

void setup()
{
  Serial.begin(115200);

  while (!Serial && millis() < 5000);

  client.onMessage(onMessageCallback);
  client.onEvent(onEventsCallback);
  client.setFingerprint(fingerprint);

  connect();

  out = new AudioOutputI2S();
  out->begin();
}

int period = 4000;
unsigned long time_now = 0;
int failures = 0;

void loop()
{
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

    file = new AudioFileSourceHTTPStream(URL);
    buff = new AudioFileSourceBuffer(file, 2048);
    mp3 = new AudioGeneratorMP3();
    mp3->begin(buff, out);

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
