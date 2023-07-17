#include "defines.h"

#include <ArduinoJson.h>
#include <TimeLib.h>
#include <WebSockets2_Generic.h>
#include <ESP8266WiFi.h>

using namespace websockets2_generic;

#define SIGNAL_PIN 2

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

    Serial1.print(receipt.c_str());
    digitalWrite(SIGNAL_PIN, HIGH);
    delay(10);
    digitalWrite(SIGNAL_PIN, LOW);
    // tpPrint(const_cast<char*>(receipt.c_str()));
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
const char echo_org_ssl_fingerprint[] PROGMEM   = "68 82 CB EB D4 76 F5 0A D4 B4 B2 38 9B 4E 43 95 B3 20 FB BA";
void setup()
{
  Serial.begin(115200);
  Serial1.begin(19200);
  pinMode(SIGNAL_PIN, OUTPUT);

  while (!Serial && millis() < 5000);

  client.onMessage(onMessageCallback);
  client.onEvent(onEventsCallback);
  client.setFingerprint(echo_org_ssl_fingerprint);

  connect();

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
    client.close(CloseReason_NormalClosure);
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
      client.close(CloseReason_NormalClosure);
    }
  } 

  if (failures > 5) ESP.restart();
}
