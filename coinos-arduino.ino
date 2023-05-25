#include "defines.h"

#include <ArduinoJson.h>
#include <Thermal_Printer.h>
#include <TimeLib.h>
#include <WebSockets2_Generic.h>
#include <WiFi.h>

using namespace websockets2_generic;

void onMessageCallback(WebsocketsMessage message)
{
  Serial.println(message.data());
  StaticJsonDocument<1000> doc;
  DeserializationError error = deserializeJson(doc, message.data());
  if (error)
  {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.f_str());
    return;
  }

  if (doc["type"] == "payment") {
    String receipt = R"(
************************
COINOS PAYMENT RECEIVED
************************

$date
$time

$$fiat + $$fiatTip
$amount + $tip sats

Total: $$fiatTotal

https://coinos.io/invoice/$id

************************
    )";

    const int sats = 100000000;

    JsonObject data = doc["data"];
    int amount = data["amount"];
    int tip = data["tip"];
    float rate = data["rate"];
    unsigned long long created = data["created"];
    time_t t = (created / 1000) - 25200;
    String id = data["id"];

    char dateString[20];
    char timeString[9];
    char fiatString[10];
    char fiatTipString[10];
    char fiatTotalString[10];

    struct tm *createdTime = gmtime(&t);

    strftime(dateString, sizeof(dateString), "%B %d, %Y", createdTime);
    strftime(timeString, sizeof(timeString), "%I:%M %p", createdTime);
    dtostrf(amount * rate / sats, 0, 2, fiatString);
    dtostrf(tip ? tip * rate / sats : 0, 0, 2, fiatTipString);
    dtostrf((amount + tip) * rate / sats, 0, 2, fiatTotalString);
  
    receipt.replace("$date", dateString);
    receipt.replace("$time", timeString);
    receipt.replace("$fiatTip", fiatTipString);
    receipt.replace("$fiatTotal", fiatTotalString);
    receipt.replace("$amount", String(amount));
    receipt.replace("$tip", String(tip));
    receipt.replace("$time", timeString);
    receipt.replace("$fiat", fiatString);
    receipt.replace("$id", id);

    tpSetFont(0, 0, 0, 0, 0);
    tpPrint(const_cast<char*>(receipt.c_str()));
  } 
}

void onEventsCallback(WebsocketsEvent event, String data)
{
  Serial.println("SOCKET EVENT");
  Serial.println(data);
  (void) data;

  if (event == WebsocketsEvent::ConnectionOpened)
  {
    Serial.println("Connnection Opened");
  }
  else if (event == WebsocketsEvent::ConnectionClosed)
  {
    Serial.println("Connnection Closed");
  }
  else if (event == WebsocketsEvent::GotPing)
  {
    Serial.println("Got a Ping!");
  }
  else if (event == WebsocketsEvent::GotPong)
  {
    Serial.println("Got a Pong!");
  }
}

WebsocketsClient client;

void setup()
{
  Serial.begin(115200);

  while (!Serial && millis() < 5000);

  client.onMessage(onMessageCallback);
  client.onEvent(onEventsCallback);
  client.setCACert(cert);

  connect();

}

int period = 5000;
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
    client.close(CloseReason_NormalClosure);
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    WiFi.disconnect();
    delay(1000);
    WiFi.begin(ssid, password);
    delay(2000);

  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Connected to Wi-Fi network with IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Not connected to Wi-Fi network");
  }

  if (client.available()) {
    Serial.println("Sending heartbeat");
    String msg = String("{\"type\":\"heartbeat\",\"data\":\"eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpZCI6IjU2ZWRlZTNhLTdlYTktNDdlNi1hOTgxLWY0N2M2NDhmOWQ2MSIsImlhdCI6MTY4NDk2MjEwMH0.juSxoZmWYcG59aKtjxtEfhoTH7Dxe7tHAy1eArOrLks\"}");

    client.send(msg);

    if (!tpIsConnected()) {
      Serial.println("Connecting to Bluetooth");
      tpScan("", 3) && tpConnect();
    } 
  } else {
    Serial.println("No socket");
    failures++;

    socketConnected = client.connect(websockets_connection_string);

    if (socketConnected) {
      Serial.println("Socket Connected!");

      String msg = String("{\"type\":\"login\",\"data\":\"eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpZCI6IjU2ZWRlZTNhLTdlYTktNDdlNi1hOTgxLWY0N2M2NDhmOWQ2MSIsImlhdCI6MTY4NDk2MjEwMH0.juSxoZmWYcG59aKtjxtEfhoTH7Dxe7tHAy1eArOrLks\"}");

      client.send(msg);

    } else {
      client.close(CloseReason_NormalClosure);
      Serial.println("Socket Not Connected!");
    }
  } 

  if (failures > 5) ESP.restart();
}
