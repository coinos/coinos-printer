#include "defines.h"
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <TimeLib.h>
#include <WiFi.h>
#include <Thermal_Printer.h>

#define CHECKSUM_SIZE 4

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

void callback(char *topic, byte *payload, unsigned int length) {
  char message[length + 1];
  memcpy(message, payload, length);
  message[length] = '\0';

  if (strncmp(message, "pay:", 4) == 0) {
    memmove(message, message + 4, strlen(message + 4) + 1);
    Serial.println("Receipt");
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

https://coinos.io/payment/$paymentHash

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
      receipt.replace("Tip:    $$fiatTip\n", "");
      receipt.replace("Amount: $$fiat\n", "");
    }

    receipt.replace("$fiatTotal", fiatTotalString);
    receipt.replace("$amount", String(amount));
    receipt.replace("$tip", String(tip));
    receipt.replace("$time", timeString);
    receipt.replace("$fiat", fiatString);
    receipt.replace("$paymentHash", parts[4]);

    Serial.println(receipt);
    tpSetFont(0, 0, 0, 0, 0);
    tpPrint(const_cast<char*>(receipt.c_str()));
  }
}

void setup() {
  Serial.begin(115200);

  while (!Serial)
    delay(10);

  connect();

  mqtt.setBufferSize(512);
  mqtt.setServer(mqtt_server, 1883);
  mqtt.setCallback(callback);

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

int period = 10000;
unsigned long time_now = 0;

void loop() {
  ArduinoOTA.handle();

  if (!mqtt.connected()) {
    reconnect();
  }

  mqtt.loop();

  if ((unsigned long)(millis() - time_now) > period) {
    connect();
    if (!tpIsConnected()) {
      tpScan("", 3) && tpConnect();
    } 

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
