#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <FS.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include <PubSubClient.h>
#include <TimeLib.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>

#define FILE_PATH "/config.txt"
#define AP_MODE_TIMEOUT 600000

WebServer server(80);
unsigned long apModeStartTime = 0;
bool isAPMode = false;
const char *mqtt_server = "mqtt.coinos.io";

String ssid, key, username, password;

String APSSID = "CoinosPrinter";
String APPASS = "21bitcoin";

#include <HardwareSerial.h>
HardwareSerial printer(1);

WiFiClient espClient;
PubSubClient mqtt(espClient);
String paymentHash;
String clientId;

const char *url = "https://coinos.io/api/public/coinos-printer.ino.bin";

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

void update() {
  HTTPClient http;

  http.begin(url);

  Serial.println("HTTP request started");
  int httpCode = http.GET();
  if (httpCode > 0) {
    if (httpCode == HTTP_CODE_OK) {
      int contentLength = http.getSize();

      bool canBegin = Update.begin(contentLength);
      if (canBegin) {
        Serial.println("Begin OTA. This may take 2 - 5 minutes to complete.");
        size_t written = Update.writeStream(http.getStream());

        if (written == contentLength) {
          Serial.println("Written : " + String(written) + " successfully");
        } else {
          Serial.println("Written only : " + String(written) + "/" +
                         String(contentLength) + ". Retry?");
        }

        if (Update.end()) {
          Serial.println("OTA done!");
          if (Update.isFinished()) {
            Serial.println("Update successfully completed. Rebooting.");
            ESP.restart();
          } else {
            Serial.println("Update not finished? Something went wrong!");
          }
        } else {
          Serial.println("Error Occurred. Error #: " +
                         String(Update.getError()));
        }

      } else {
        Serial.println("Not enough space to begin OTA");
        http.end();
      }
    }
  } else {
    Serial.println("HTTP error: " + http.errorToString(httpCode));
  }
  http.end();
}

void callback(char *topic, byte *payload, unsigned int length) {
  char message[length + 1];
  memcpy(message, payload, length);
  message[length] = '\0';

  Serial.println(message);

  if (strncmp(message, "update:", 7) == 0) {
    update();
  }

  if (strncmp(message, "creds:", 5) == 0) {
    String parts[5];
    split(message, ':', parts, 5);
    writeCredentials(parts[1], parts[2], parts[3], parts[4]);
  }

  if (strncmp(message, "msg:", 4) == 0) {
    String parts[2];
    split(message, ':', parts, 2);
    printer.println(parts[1]);
  }

  if (strncmp(message, "pay:", 4) == 0) {
    memmove(message, message + 4, strlen(message + 4) + 1);
    Serial.println("Receipt");

    String receipt = R"(

--------------------------------
        COINOS PAYMENT
--------------------------------

$date
$time

$items

Amount: $$fiat
Tip:    $$fiatTip

Total:  $$fiatTotal

Notes: $memo

https://coinos.io/payment/$paymentHash

--------------------------------





  )";

    String parts[7];
    split(message, ':', parts, 7);

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
      receipt.replace("Tip:    $$fiatTip\n\n", "");
      receipt.replace("Amount: $$fiat\n", "");
    }

    receipt.replace("$fiatTotal", fiatTotalString);
    receipt.replace("$amount", String(amount));
    receipt.replace("$tip", String(tip));
    receipt.replace("$time", timeString);
    receipt.replace("$fiat", fiatString);
    receipt.replace("$paymentHash", parts[4]);

    if (parts[5].equals("") || parts[5].equals("undefined") || parts[5].equals("null")) {
      receipt.replace("Notes: $memo\n\n", "");
    } else {
      receipt.replace("$memo", parts[5]);
    }

    if (parts[6].equals("") || parts[6].equals("undefined") || parts[6].equals("null")) {
      receipt.replace("$items\n\n", "");
    } else {
      receipt.replace("$items", parts[6]);
    }

    Serial.println(receipt);
    printer.println(receipt);
  }
}

void setup() {
  Serial.begin(115200);
  printer.begin(9600, SERIAL_8N1, -1, 6);

  if (!LittleFS.begin()) {
    Serial.println("An Error has occurred while mounting LittleFS");
    return;
  }

  clientId = "Client-" + String(random(0xffff), HEX);

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

    if (mqtt.connect(clientId.c_str(), username.c_str(), password.c_str())) {
      Serial.println("connected");
      mqtt.subscribe(username.c_str(), 1);
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

  if (isAPMode) {
    server.handleClient();
  } else if (WiFi.status() == WL_CONNECTED) {
    if (!mqtt.connected()) {
      reconnect();
    }

    mqtt.loop();
  }

  if ((unsigned long)(millis() - time_now) > period) {
    Serial.print("ssid ");
    Serial.print(ssid);
    Serial.print(" username ");
    Serial.println(username);

    connect();
    time_now = millis();
  }
}

bool apMode;
void connect() {
  if (isAPMode) {
    if (millis() - apModeStartTime > AP_MODE_TIMEOUT) {
      Serial.println("AP mode timeout reached. Rebooting...");
      ESP.restart();
    }
  } else if (WiFi.status() != WL_CONNECTED) {
    WiFi.disconnect();

    if (!readCredentials()) {
      Serial.println("Failed to read credentials. Starting AP mode...");
      startAPMode();
      return;
    }

    if (key && key != "none") {
      WiFi.begin(ssid.c_str(), key.c_str());
    } else {
      WiFi.begin(ssid.c_str());
    } 

    int retryCount = 0;
    while (WiFi.status() != WL_CONNECTED && retryCount < 50) {
      delay(500);
      Serial.print("Connecting to ");
      Serial.println(ssid);
      retryCount++;
    }

    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Failed to connect. Starting AP mode...");
      startAPMode();
    } else {
      Serial.println("Connected to WiFi");
      // Continue with normal operation
    }

    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
  }
}

bool readCredentials() {
  File file = LittleFS.open(FILE_PATH, "r");
  if (!file) {
    Serial.println("Failed to open file for reading");
    return false;
  }

  ssid = file.readStringUntil('\n');
  ssid.trim();

  key = file.readStringUntil('\n');
  key.trim();

  username = file.readStringUntil('\n');
  username.trim();

  password = file.readStringUntil('\n');
  password.trim();

  file.close();
  return true;
}

void writeCredentials(const String &ssid, const String &key,
                      const String &username, const String &password) {
  File file = LittleFS.open(FILE_PATH, "w");
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }

  file.println(ssid);
  file.println(key);
  file.println(username);
  file.println(password);
  file.close();
}

void startAPMode() {
  isAPMode = true;
  apModeStartTime = millis();

  WiFi.softAP(APSSID + "-" + WiFi.macAddress(), APPASS);

  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  server.on("/", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(
        200, "text/html",
        "<form action=\"/save\" method=\"POST\"> "
        "<input type=\"text\" name=\"ssid\" placeholder=\"wifi ssid\" style=\"font-size: 24px; margin: 10px 0; padding: 6px; \"> <br>"
        "<input type=\"text\" name=\"key\" placeholder=\"wifi key\" style=\"font-size: 24px; margin: 10px 0; padding: 6px;\"> <br> "
        "<input type=\"text\" name=\"username\" placeholder=\"coinos "
        "username\" style=\"font-size: 24px; margin: 10px 0; padding: 6px;\"> <br> "
        "<input type=\"text\" name=\"password\" placeholder=\"coinos "
        "password\" style=\"font-size: 24px; margin: 10px 0; padding: 6px;\"> <br> <br> "
        "<input type=\"submit\" value=\"Save\" style=\"font-size: 24px; margin: 20px 0; padding: 6px;\"> "
        "</form>");
  });

  server.on("/save", HTTP_POST, []() {
    String ssid = server.arg("ssid");
    String key = server.arg(" key");
    String username = server.arg(" username");
    String password = server.arg(" password");

    writeCredentials(ssid, key, username, password);

    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", "Credentials Saved. Rebooting...");
    delay(1000);
    ESP.restart();
  });

  server.begin();
}
