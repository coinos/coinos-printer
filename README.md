# Coinos Arduino

This code can be flashed onto an ESP32 development board to instruct it to connect to a WiFi AP and Bluetooth Thermal Printer. The code will establish a Websocket connection to the <a href="https://coinos.io/docs">Coinos API</a> using the JWT auth token of a specific account and listen for payments that are received into the account. When a payment is detected, a receipt will be printed with the amount and time of the transaction.

This way, staff can confirm payment receipt without needing to login to the app on a phone or tablet device, which makes training easier.

Customers can initate the payment by scanning a static QR code that directs them to the merchant's payment page to generate an invoice and add a tip, which they an then click on or copy to their bitcoin wallet of choice.

## Setup

   cp defines.h.sample defines.h
   # edit this file to configure your JWT auth token and WiFi ssid / password

## Flashing

sudo arduino-cli compile -b esp32:esp32:esp32da --libraries /path/to/arduino/libraries --build-property build.partitions=min_spiffs --build-property upload.maximum_size=1966080 -u -p /dev/ttyUSB0 

## Monitoring Serial Output

sudo arduino-cli monitor -p /dev/ttyUSB0 -l serial -b esp32:esp32:esp32da -c baudrate=115200
