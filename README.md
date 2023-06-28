# Coinos Printer

This code can be flashed onto an <a href="https://www.amazon.ca/WayinTop-Development-ESP-WROOM-32-Bluetooth-Microcontroller/dp/B086ZMDB7H/">ESP32 development board</a> to instruct it to connect to a WiFi AP and <a href="https://www.amazon.ca/gp/product/B09QPQ76SN/">Bluetooth thermal printer</a>.

The code will establish a Websocket connection to the <a href="https://coinos.io/docs">Coinos API</a> using the JWT auth token of your account and listen for payments that are received.

When a payment is detected, a receipt will be printed with the amount and time of the transaction.

## Setup

   cp defines.h.sample defines.h

Edit `defines.h` to configure your JWT auth token and WiFi ssid / password

## Install arduino-cli

    curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh
    arduino-cli config init

Edit `$HOME/.arduino15/arduino-cli.yaml` to add ESP32 package url:

    board_manager:
      additional_urls:
        - https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json

    arduino-cli core update-index
    arduino-cli core install esp32:esp32

## Flashing

    sudo chmod a+rw /dev/ttyUSB0
    arduino-cli compile -b esp32:esp32:esp32da --libraries $PWD/libraries --build-property build.partitions=min_spiffs --build-property upload.maximum_size=1966080 -u -p /dev/ttyUSB0 

## Monitoring Serial Output

    arduino-cli monitor -p /dev/ttyUSB0 -l serial -b esp32:esp32:esp32da -c baudrate=115200
