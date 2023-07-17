#include <SPI.h>

// Pin Definitions
#define SCK_PIN 18
#define MISO_PIN 19
#define MOSI_PIN 23
#define CS_PIN 5

// Buffer to store received data
#define BUF_SIZE 256
uint8_t buf[BUF_SIZE];

void setup() {
  Serial.begin(115200);
  while(!Serial);

  pinMode(SCK_PIN, INPUT);
  pinMode(MOSI_PIN, INPUT);
  pinMode(MISO_PIN, OUTPUT);
  pinMode(CS_PIN, INPUT_PULLUP);

  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, CS_PIN);  // Init SPI
  SPI.setHwCs(false);  // Handle CS manually
}

void loop() {
  // Check if CS is low (active)
  if (digitalRead(CS_PIN) == LOW) {
    // Read data while CS is active
    int i = 0;
    while (digitalRead(CS_PIN) == LOW && i < BUF_SIZE) {
      buf[i++] = SPI.transfer(0xFF);  // Read data, send dummy data
    }

    // Process received data
    Serial.print("Received data: ");
    for (int j = 0; j < i; j++) {
      Serial.print(buf[j], HEX); 
      Serial.print(" ");
    }
    Serial.println();

    // Clear buffer
    memset(buf, 0, sizeof(buf));
  }
}
