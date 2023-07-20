#include <ESP32SPISlave.h>
#include <Arduhdlc.h>

#define MAX_HDLC_FRAME_LENGTH 32

ESP32SPISlave slave;

Arduhdlc hdlc(&send_character, &hdlc_frame_handler, MAX_HDLC_FRAME_LENGTH);

static constexpr uint32_t BUFFER_SIZE {32};
uint8_t spi_slave_tx_buf[BUFFER_SIZE];
uint8_t spi_slave_rx_buf[BUFFER_SIZE];

void setup() {
    Serial.begin(115200);

    // HSPI = CS: 15, CLK: 14, MOSI: 13, MISO: 12
    // VSPI = CS:  5, CLK: 18, MOSI: 23, MISO: 19
    slave.setDataMode(SPI_MODE0);
    slave.begin(VSPI);
}

void loop() {
    // if there is no transaction in queue, add transaction
    if (slave.remained() == 0)
        slave.queue(spi_slave_rx_buf, spi_slave_tx_buf, BUFFER_SIZE);

    // if transaction has completed from master,
    // available() returns size of results of transaction,
    // and `spi_slave_rx_buf` is automatically updated
    String str;
    int size = slave.available();
    while (size > 0) {
      hdlc.charReceiver(reinterpret_cast<char*>(spi_slave_rx_buf));
      size = slave.available();
    }
}

// Function to send out one 8-bit character
void send_character(uint8_t data) {
  return;
}

// Frame handler function to process received HDLC frames
void hdlc_frame_handler(const uint8_t* data, uint16_t length) {
  // Do something with the received HDLC frame
  // ...

  // Example: Print the received frame as a string
  String receivedData((const char*)data, length);
  Serial.println("Received HDLC frame: " + receivedData);
}
