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

WiFiClient wifi;
PubSubClient mqtt(wifi);
HTTPClient http;
WiFiClient wifi2;

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

void callback(char* topic, byte* payload, unsigned int length) {
  char message[length + 1];
  memcpy(message, payload, length);
  message[length] = '\0';

  if (strcmp(message, "end") == 0) {
    ready = true;
    return;
  } 
  
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

void setup() {
  Serial.begin(115200);
  printer.begin(9600);
  // printer.begin(9600, SERIAL_8N1, 7, 6);

  while (!Serial) { delay(10); }

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

void playStream(String paymentHash) {
  String URL = "http://ln.coinos.io:9090/" + paymentHash + ".mp3";

  http.begin(wifi2, URL.c_str());
  int httpCode = http.GET();

  size_t dataSize = 42 * 1024;
  size_t chunkSize = 2048;
  unsigned char* buff = new unsigned char[chunkSize];
  unsigned char* data = new unsigned char[dataSize];
  size_t totalRead = 0;

  if (httpCode == HTTP_CODE_OK) {
    int contentLength = http.getSize();

    Stream &stream = http.getStream();
    while (stream.available()) {
      size_t bytesRead = stream.readBytes(buff, chunkSize);
      if (bytesRead + totalRead <= dataSize) memcpy(data + totalRead, buff, bytesRead);
      totalRead += bytesRead;
    }

    http.end();

    delete[] buff;

    AudioGeneratorMP3 *mp3;
    AudioFileSourcePROGMEM *file;
    AudioOutputI2S *out;

    file = new AudioFileSourcePROGMEM(data, contentLength);
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
    delete[] data;
  } else {
    Serial.println("HTTP request failed");
  }
}

int period = 10000;
unsigned long time_now = 0;

void loop() {
  ArduinoOTA.handle();

  if (ready && !paymentHash.isEmpty()) {
    // delay(3500);
    playAudio("/received.mp3");
    playStream(paymentHash);
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
