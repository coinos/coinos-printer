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

    tpPrint(const_cast<char*>(receipt.c_str()));
  } 
}

void onEventsCallback(WebsocketsEvent event, String data)
{
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

  // Connect to wifi
  WiFi.begin(ssid, password);

  // Wait some time to connect to wifi
  for (int i = 0; i < 10 && WiFi.status() != WL_CONNECTED; i++)
  {
    Serial.print(".");
    delay(1000);
  }

  // Check if connected to wifi
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("No Wifi!");
    return;
  }

  // run callback when messages are received
  client.onMessage(onMessageCallback);

  // run callback when events are occuring
  client.onEvent(onEventsCallback);

  // Before connecting, set the ssl fingerprint of the server
  client.setCACert(cert);

  // Connect to server
  bool connected = client.connect(websockets_connection_string);

  if (connected)
  {
    Serial.println("Connected!");

    String msg = String("{\"type\":\"login\",\"data\":\"eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpZCI6ImE5NzcwNDIxLTNmNjUtMTFlZC05ZjU3LTAyNDJhYzJhMDAwNCIsImlhdCI6MTY4MjQ0MzQ3N30.ENhwdaGmmgDMUsAce7y3_50lAyRUk_Z8PcAm9JeGhG4\"}");
    client.send(msg);
  }
  else
  {
    Serial.println("Not Connected!");
  }

  if (tpScan())
  {
    if (tpConnect())
    {
      tpSetFont(0, 0, 0, 0, 0);
    }
  }
}

int period = 5000;
unsigned long time_now = 0;

void loop()
{
  client.poll();
  if ((unsigned long)(millis() - time_now) > period) {
    time_now = millis();

    String msg = String("{\"type\":\"heartbeat\",\"data\":\"eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpZCI6ImE5NzcwNDIxLTNmNjUtMTFlZC05ZjU3LTAyNDJhYzJhMDAwNCIsImlhdCI6MTY4MjQ0MzQ3N30.ENhwdaGmmgDMUsAce7y3_50lAyRUk_Z8PcAm9JeGhG4\"}");
    client.send(msg);
  } 
}
