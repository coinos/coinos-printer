# Coinos Arduino

This code can be flashed onto an <a href="https://www.amazon.ca/WayinTop-Development-ESP-WROOM-32-Bluetooth-Microcontroller/dp/B086ZMDB7H/">ESP32 development board</a> to instruct it to connect to a WiFi AP and <a href="https://www.amazon.ca/gp/product/B09QPQ76SN/">Bluetooth thermal printer</a>.

The code will establish a Websocket connection to the <a href="https://coinos.io/docs">Coinos API</a> using the JWT auth token of a specific account and listen for payments that are received into the account. 

When a payment is detected, a receipt will be printed with the amount and time of the transaction.

This way, staff can confirm payment receipt without needing to login to the app on a phone or tablet device, which makes training easier.

## Setup

   cp defines.h.sample defines.h

Edit `defines.h` to configure your JWT auth token and WiFi ssid / password

## Flashing

sudo arduino-cli compile -b esp32:esp32:esp32da --libraries /path/to/arduino/libraries --build-property build.partitions=min_spiffs --build-property upload.maximum_size=1966080 -u -p /dev/ttyUSB0 

## Monitoring Serial Output

sudo arduino-cli monitor -p /dev/ttyUSB0 -l serial -b esp32:esp32:esp32da -c baudrate=115200
