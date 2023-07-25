#include <Arduhdlc.h>
#include <Thermal_Printer.h>
#include <TimeLib.h>

#define MAX_HDLC_FRAME_LENGTH 512

Arduhdlc hdlc(&send_character, &hdlc_frame_handler, MAX_HDLC_FRAME_LENGTH);

void setup() {
  Serial.begin(115200);
}

void loop() {
  while (Serial.available()) {
    char inChar = (char)Serial.read();
    hdlc.charReceiver(inChar);
    Serial.print(inChar);
  }

  if (!tpIsConnected()) {
    // Serial.println("Connecting to Bluetooth");
    tpScan("", 3) && tpConnect();
  } 
}

void send_character(uint8_t data) {};

void hdlc_frame_handler(const uint8_t* data, uint16_t length) {
  String message((const char*)data, length);
  
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

  // Serial.println(message);

  String parts[4];
  split(message, ':', parts, 4);

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
    receipt.replace("+ $$fiatTip", "");
  } 

  receipt.replace("$fiatTotal", fiatTotalString);
  receipt.replace("$amount", String(amount));
  receipt.replace("$tip", String(tip));
  receipt.replace("$time", timeString);
  receipt.replace("$fiat", fiatString);

  tpSetFont(0, 0, 0, 0, 0);
  tpPrint(const_cast<char*>(receipt.c_str()));
}

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
