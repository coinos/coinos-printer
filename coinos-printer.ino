#include "defines.h" 
#include "AudioFileSourceBuffer.h"
#include "AudioFileSourceLittleFS.h"
#include "AudioFileSourceHTTPStream.h"
#include "AudioFileSourcePROGMEM.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"
#include <ArduinoOTA.h> 
#include <HTTPClient.h>
#include <TimeLib.h> 
#include <WiFi.h> 
#include <PubSubClient.h>
// #include <HardwareSerial.h>
#define printer Serial2


// HardwareSerial printer(1);

WiFiClient espClient;
PubSubClient mqtt(espClient);
String paymentHash;
bool ready = false;
bool buffering = false;

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

const size_t bufferSize = 48 * 1024;
unsigned char source[bufferSize];
size_t bytesRead = 0;

void callback(char* topic, byte* payload, unsigned int length) {
  char message[length + 1];
  memcpy(message, payload, length);
  message[length] = '\0';

  if (strcmp(message, "end") == 0) {
    Serial.println("Ready!");
    buffering = false;
    ready = true;
    return;
  } 
  
  if (strcmp(message, "start") == 0) {
    Serial.println("Starting");
    buffering = true;
    return;
  } 

  if (buffering) {
    Serial.print(".");
    if (bytesRead + length <= bufferSize) memcpy(source + bytesRead, payload, length);
    bytesRead += length;
    return;
  } 

  Serial.println("Receipt");

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

  paymentHash = parts[4];

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
} 

int contentLength;
int totalBytesRead = 0;

WiFiClient wifiClient;
HTTPClient http;
 
void setup() {
  Serial.begin(115200);
  printer.begin(9600);
  // printer.begin(9600, SERIAL_8N1, 7, 6);

  while (!Serial) { delay(10); }

  String mac = WiFi.macAddress();
  Serial.print("MAC address of this device: ");
  Serial.println(mac);

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

    String clientId = "Client-";
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
  AudioGeneratorMP3 *mp3;
  AudioFileSourceLittleFS *file;
  AudioOutputI2S *out;

  file = new AudioFileSourceLittleFS(filename.c_str());
  mp3 = new AudioGeneratorMP3();
  out = new AudioOutputI2S();
  // bclk, lrc, din
  out->SetPinout(15, 4, 5);
  // out->SetPinout(9, 8, 10);

  if (mp3->begin(file, out)) {
    while (mp3->isRunning()) {
      if (!mp3->loop()) {
        mp3->stop();
      }
    }
  }

  delete file;
  delete out;
  delete mp3;
}

void playBuffer() {
  Serial.println("Playing buffer");
  Serial.println(bytesRead);
  AudioGeneratorMP3 *mp3;
  AudioFileSourcePROGMEM *file;
  AudioOutputI2S *out;

  file = new AudioFileSourcePROGMEM(source, bytesRead);
  mp3 = new AudioGeneratorMP3();
  out = new AudioOutputI2S();
  // bclk, lrc, din
  out->SetPinout(15, 4, 5);
  // out->SetPinout(9, 8, 10);

  if (mp3->begin(file, out)) {
    while (mp3->isRunning()) {
      if (!mp3->loop()) {
        mp3->stop();
      }
    }
  }

  memset(source, 0, bufferSize);
  bytesRead = 0;

  delete file;
  delete out;
  delete mp3;
}

void playStream(String paymentHash) {
  String URL = "http://ln.coinos.io:9090/" + paymentHash + ".mp3";

  AudioGeneratorMP3 *mp3;
  AudioFileSourceHTTPStream *file;
  AudioFileSourceBuffer *buff;
  AudioOutputI2S *out;

  file = new AudioFileSourceHTTPStream(URL.c_str());
  buff = new AudioFileSourceBuffer(file, 16384);
  mp3 = new AudioGeneratorMP3();
  out = new AudioOutputI2S();

  out->SetPinout(15, 4, 5);
  // out->SetPinout(9, 8, 10);

  if (mp3->begin(buff, out)) {
    while (mp3->isRunning()) {
      if (!mp3->loop()) {
        mp3->stop();
      }
    }
  }

  delete file;
  delete buff;
  delete out;
  delete mp3;
}

int period = 10000;
unsigned long time_now = 0;

void loop() {
  ArduinoOTA.handle();

  if (ready && !paymentHash.isEmpty()) {
    Serial.println("Ready!");
    // delay(3500);
    playAudio("/received.mp3");
    playBuffer();
    // playStream(paymentHash);
    paymentHash = "";
    ready = false;
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
