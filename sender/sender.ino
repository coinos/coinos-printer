#include <SPI.h>
#include <Arduhdlc.h>

// MOSI, MISO and SCK are default pins GPIO13, GPIO12 and GPIO14 respectively on the ESP8266
#define SPI_SS     5  // Default is GPIO15, but you can use any GPIO
#define MAX_HDLC_FRAME_LENGTH 32

static const int spiClk = 1600000; // 4 MHz

void send_character(uint8_t data);

void hdlc_frame_handler(const uint8_t *data, uint16_t length)
{
  return;
}

Arduhdlc hdlc(&send_character, &hdlc_frame_handler, MAX_HDLC_FRAME_LENGTH);

void setup() {
  Serial.begin(115200);
  SPI.begin();
  pinMode(SPI_SS, OUTPUT); //SPI SS
}

void loop() {
  spi_send_command();
  delay(1000);
}

void send_character(uint8_t data) {
  SPI.beginTransaction(SPISettings(spiClk, MSBFIRST, SPI_MODE0));
  digitalWrite(SPI_SS, LOW);
  SPI.transfer(data);
  digitalWrite(SPI_SS, HIGH);
  SPI.endTransaction();
}

void spi_send_command() {
  send_character('a');
  delay(1000);
  send_character('b');
  delay(1000);
  send_character('c');
  delay(1000);
}

/*
void spi_send_command() {
  Serial.println("Sending");
  // String to send
  String dataToSend = "Hello, SPI!";
  
  // Convert string to character array
  char charBuffer[dataToSend.length() + 1];
  dataToSend.toCharArray(charBuffer, sizeof(charBuffer));
  
  // Send the data using Arduhdlc
  hdlc.sendFrame((uint8_t*)charBuffer, dataToSend.length());
  Serial.println("Sent frame");
}
*/
