#include "defines.h"

#include <ArduinoJson.h>
#include <TimeLib.h>
#include <WebSockets2_Generic.h>
#include <ESP8266WiFi.h>
#include <SPI.h>
#include <ESP8266HTTPClient.h>

using namespace websockets2_generic;

#define SS_PIN 15
#define MOSI_PIN 13
#define MISO_PIN 12
#define SCK_PIN 14

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

WiFiClientSecure wifiClient;
WebsocketsClient client;
const char echo_org_ssl_fingerprint[] PROGMEM   = "68 82 CB EB D4 76 F5 0A D4 B4 B2 38 9B 4E 43 95 B3 20 FB BA";
void setup()
{
  Serial.begin(115200);
  Serial1.begin(19200);

  pinMode(SS_PIN, OUTPUT);
  digitalWrite(SS_PIN, HIGH); // Set Slave Select high to ensure we're not sending any data yet
  SPI.begin(); // No arguments
  // Set the SPI settings: clock speed, bit order, data mode
  SPI.setClockDivider(SPI_CLOCK_DIV2);
  SPI.setBitOrder(MSBFIRST);
  SPI.setDataMode(SPI_MODE0);

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

bool transmitted = false;
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

    Serial.println(transmitted);
    if (!transmitted) {
      HTTPClient https;
      
      wifiClient.setInsecure();
      https.begin(wifiClient, "https://coinos.io/api/public/guh.mp3");

      int httpCode = https.GET();

      Serial.println(httpCode);

      if (httpCode == HTTP_CODE_OK) {
        uint8_t buffer[512];
        int bytesRead;

        SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
        digitalWrite(SS_PIN, LOW);

        Serial.println("Transffering");

        while ((bytesRead = https.getStream().readBytes(buffer, sizeof(buffer))) > 0) {
          for (int i = 0; i < bytesRead; i++) {
            SPI.transfer(buffer[i]);
          }
        }

        digitalWrite(SS_PIN, HIGH);
        SPI.endTransaction();
      }
      
      https.end();
      transmitted = true;
    }
  } else {
    Serial.println("Not connected to Wi-Fi network");
  }

  /* if (client.available()) { */
  /*   String msg = "{\"type\":\"heartbeat\",\"data\":\"" + String(token) + "\"}"; */
  /*  */
  /*   client.send(msg); */
  /*  */
  /* } else { */
  /*   Serial.println("No socket"); */
  /*   failures++; */
  /*  */
  /*   socketConnected = client.connect(websockets_connection_string); */
  /*  */
  /*   if (socketConnected) { */
  /*     Serial.println("Socket Connected!"); */
  /*  */
  /*     String msg = "{\"type\":\"login\",\"data\":\"" + String(token) + "\"}"; */
  /*  */
  /*     client.send(msg); */
  /*  */
  /*   } else { */
  /*     Serial.println("Socket Not Connected!"); */
  /*     client.close(CloseReason_NormalClosure); */
  /*   } */
  /* }  */
  /*  */
  /* if (failures > 5) ESP.restart(); */
}
