#include "defines.h" 
#include "AudioFileSourceHTTPStream.h"
#include "AudioFileSourceBuffer.h"
#include "AudioFileSourceLittleFS.h"
#include "AudioGeneratorWAV.h"
#include "AudioOutputI2S.h"
#include <ArduinoOTA.h> 
#include <ESP8266HTTPClient.h>
#include <SoftwareSerial.h> 
#include <TimeLib.h> 
#include <ESP8266WiFi.h> 
#include <WiFiClientSecure.h> 
#include <PubSubClient.h>

SoftwareSerial printer(4, 5);

WiFiClient espClient;
PubSubClient mqtt(espClient);
String paymentHash;

void split(String message, char delimiter, String parts[], int partsSize) {
  int partIndex = 0;
  int delimiterIndex = message.indexOf(delimiter);

  while (delimiterIndex != -1) {
    if (partIndex < partsSize) {
      parts[partIndex] = message.substring(0, delimiterIndex);
      partIndex++;
    }

    message = message.substring(delimiterIndex + 1);
    
    delimiterIndex = message.indexOf(delimiter);
  }

  if (partIndex < partsSize) {
    parts[partIndex] = message;
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.println(topic);

  char message[length + 1];
  memcpy(message, payload, length);
  message[length] = '\0';

  Serial.println(message);

  String receipt = R"(



  ************************
  COINOS PAYMENT RECEIVED
  ************************

  $date
  $time

  Amount: $$fiat
  Tip:    $$fiatTip

  Total:  $$fiatTotal

  ************************



  )";

  String parts[5];
  split(message, ':', parts, 5);

  int amount = parts[0].toInt();
  int tip = parts[1].toInt(); 
  float rate = parts[2].toFloat();

  char charBuf[32];
  parts[3].toCharArray(charBuf, 32);
  unsigned long long created = strtoull(charBuf, NULL, 10);

  time_t t = (created / 1000) - 25200;

  char dateString[20];
  char timeString[9];
  char fiatString[10];
  char fiatTipString[10];
  char fiatTotalString[10];

  struct tm *createdTime = gmtime(&t);

  const int sats = 100000000;
  strftime(dateString, sizeof(dateString), "%B %d, %Y", createdTime);
  strftime(timeString, sizeof(timeString), "%I:%M %p", createdTime);
  dtostrf(amount * rate / sats, 0, 2, fiatString);
  dtostrf(tip ? tip * rate / sats : 0, 0, 2, fiatTipString);
  dtostrf((amount + tip) * rate / sats, 0, 2, fiatTotalString);

  receipt.replace("$date", dateString);
  receipt.replace("$time", timeString);

  if (tip > 0) {
    receipt.replace("$fiatTip", fiatTipString);
  } else {
    receipt.replace("  Amount: $$fiat\n", "");
    receipt.replace("  Tip:    $$fiatTip\n", "");
  } 

  receipt.replace("$fiatTotal", fiatTotalString);
  receipt.replace("$amount", String(amount));
  receipt.replace("$tip", String(tip));
  receipt.replace("$time", timeString);
  receipt.replace("$fiat", fiatString);

  Serial.println(receipt);
  printer.println(receipt);
  paymentHash = parts[4];
} 

int contentLength;
int totalBytesRead = 0;

Stream* stream = nullptr;
WiFiClient wifiClient;
HTTPClient http;
 
void setup() {
  Serial.begin(115200);
  printer.begin(9600);

  if (!LittleFS.begin()) {
    Serial.println("Failed to mount LittleFS");
    return;
  }

  connect();

  mqtt.setBufferSize(512);
  mqtt.setServer(mqtt_server, 1883);
  mqtt.setCallback(callback);

  audioLogger = &Serial;

  ArduinoOTA.begin();
}

void reconnect() {
  while (!mqtt.connected()) {
    Serial.print("Attempting MQTT connection...");

    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    
    if (mqtt.connect(clientId.c_str(), "admin", "MPJzfq97")) {
      Serial.println("connected");
      mqtt.subscribe(username);
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqtt.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void playAudio(String filename) {
  AudioGeneratorWAV *wav;
  AudioFileSourceLittleFS *file;
  AudioOutputI2S *out;

  file = new AudioFileSourceLittleFS(filename.c_str());
  wav = new AudioGeneratorWAV();
  out = new AudioOutputI2S();

  if (wav->begin(file, out)) {
    while (wav->isRunning()) {
      if (!wav->loop()) {
        wav->stop();
      }
    }
  }

  delete file;
  delete out;
  delete wav;
}

void playStream(String paymentHash) {
  AudioGeneratorWAV *wav;
  AudioFileSourceHTTPStream *stream;
  AudioFileSourceBuffer *buff;
  AudioOutputI2S *out;

  String URL = "http://ln.coinos.io:9090/" + paymentHash + ".wav";
  Serial.println("Streaming");
  Serial.println(URL);
  stream = new AudioFileSourceHTTPStream(URL.c_str());
  buff = new AudioFileSourceBuffer(stream, 4096);
  wav = new AudioGeneratorWAV();
  out = new AudioOutputI2S();

  delay(500);

  if (wav->begin(buff, out)) {
    Serial.println("Got in");
    while (wav->isRunning()) {
      if (!wav->loop()) {
        wav->stop();
      }
    }
  }

  Serial.println("Done, cleaning up");

  delete stream;
  delete buff;
  delete out;
  delete wav;
}

int period = 10000;
unsigned long time_now = 0;

void loop() {
  ArduinoOTA.handle();

  if (!paymentHash.isEmpty()) {
    playAudio("/received.wav");
    playStream(paymentHash);
    paymentHash = "";
  } 

  if (!mqtt.connected()) {
    reconnect();
  }

  mqtt.loop();
  
  if ((unsigned long)(millis() - time_now) > period) {
    connect();

    time_now = millis();
  } 
}

void connect() {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.disconnect();
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }

    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
  }
}
