#include "defines.h"

#include <ArduinoJson.h>
#include <Thermal_Printer.h>
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
    Serial.println("YES");
    getPayment(message.data());
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

  Serial.print("\nStart Secured-ESP32-Client on ");
  Serial.println(ARDUINO_BOARD);
  Serial.println(WEBSOCKETS2_GENERIC_VERSION);

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

bool getPayment(String body)
{
  WiFiClientSecure client;
  client.setInsecure(); //Some versions of WiFiClientSecure need this

  if (!client.connect("coinos.io", 443))
  {
    Serial.println("Http connection failed");
    delay(3000);
    return false;
  }

  const String url = "/text";
  client.print(String("POST ") + url + " HTTP/1.1\r\n" +
               "Host: coinos.io\r\n" +
               "User-Agent: ESP32\r\n" +
               "Content-Type: application/json\r\n" +
               "Connection: close\r\n" +
               "Content-Length: " + body.length() + "\r\n" +
               "\r\n" +
               body + "\n");

  while (client.connected())
  {
    const String line = client.readStringUntil('\n');

    if (line == "\r")
    {
      break;
    }
  }
  const String line = client.readString();

  Serial.println(line);

  // Connect to printer
  if (tpScan())
  {
    if (tpConnect())
    {
      Serial.println("Connected to printer");
      tpSetFont(0, 0, 0, 0, 0);
      tpPrint(const_cast<char*>(line.c_str()));
    }
  }

  /* int i = 0; */
  /* int j = 0; */
  /*  */
  /* while (i < line.length()) { */
  /*   Serial.println(line.charAt(i)); */
  /*   if (line.charAt(i) == '\n') { */
  /*     String print = (line.substring(j, i - 1) + "\r"); */
  /*     Serial.println(print); */
  /*     tpPrint(const_cast<char*>(print.c_str())); */
  /*     j = i; */
  /*   }  */
  /*  */
  /*   i++; */
  /* }  */
  return true;
}
