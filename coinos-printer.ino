#include "AudioFileSourceBuffer.h"
#include "AudioFileSourceHTTPStream.h"
#include "AudioFileSourceLittleFS.h"
#include "AudioFileSourcePROGMEM.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"
#include "defines.h"
#include <ArduinoOTA.h>
#include <CRC32.h>
#include <FS.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <TimeLib.h>
#include <WiFi.h>

#define printer Serial2
#define CHECKSUM_SIZE 4

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

const size_t bufferSize = 64 * 1024;
unsigned char source[bufferSize];
size_t bytesRead = 0;

const int MAX_CHUNKS = 128;
byte receivedChunks[MAX_CHUNKS] = {0};

void callback(char *topic, byte *payload, unsigned int length) {
  char message[length + 1];
  memcpy(message, payload, length);
  message[length] = '\0';

  int totalChunks = 0;

  if (strncmp(message, "end:", 4) == 0) {
    strtok(message, ":");
    totalChunks = atoi(strtok(NULL, ":"));

    String missingChunks = "";

    for (int i = 0; i < totalChunks; i++) {
      if (receivedChunks[i] == 0) {
        if (missingChunks.length() > 0) {
          missingChunks += ",";
        }
        missingChunks += String(i + 1);
      }
    }

    if (missingChunks.isEmpty()) {
      buffering = false;
      ready = true;
    } else {
      Serial.println("Missing chunks " + missingChunks);
      mqtt.publish(username, ("missing:" + missingChunks).c_str());
    }

    return;
  }

  if (strcmp(message, "start") == 0) {
    memset(source, 0, bufferSize);
    bytesRead = 0;
    Serial.println("Starting");
    buffering = true;
    return;
  }

  if (buffering) {
    for (int i = 0; i < 4; i++) {
      if (payload[i] != 0xDD)
        return;
    }

    int chunk = (payload[4] << 8) | payload[5];
    if (chunk < 0 || chunk > MAX_CHUNKS)
      return;

    uint32_t expected;
    memcpy(&expected, &payload[6], CHECKSUM_SIZE);

    payload += 10;
    length -= 10;

    uint32_t calculated = CRC32::calculate(payload, length);
    if (calculated != expected)
      return;

    receivedChunks[chunk] = 1;

    if (bytesRead + length <= bufferSize) {
      memcpy(source + bytesRead, payload, length);
      bytesRead += length;
    }

    return;
  }

  if (strncmp(message, "pay:", 4) == 0) {
    strcpy(message, message + 4);
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
}

WiFiClient wifiClient;
HTTPClient http;

void setup() {
  Serial.begin(115200);
  printer.begin(9600);

  while (!Serial)
    delay(10);

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
      mqtt.subscribe(username, 1);
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
  
  out->SetPinout(15, 4, 5);
  
  // bclk, lrc, din
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
    playAudio("/received.mp3");
    playBuffer();
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
