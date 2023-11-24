# Coinos Printer

This code can be flashed onto an <a href="https://www.aliexpress.com/item/33011482127.html">Xiao Seeed ESP32 C3</a> to instruct it to connect to a WiFi AP and <a href="https://www.aliexpress.com/item/1005006024057955.html">thermal printer</a>.

The code will establish a connection to the <a href="https://coinos.io/docs">Coinos API</a> and listen for payments that are received.

When a payment is detected, a receipt will be printed with the amount and time of the transaction.

## Install arduino-cli

    curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh
    arduino-cli config init

Edit `$HOME/.arduino15/arduino-cli.yaml` to add ESP32 package url:

    board_manager:
      additional_urls:
        - https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json

    arduino-cli core update-index
    arduino-cli core install esp32:esp32

## Install mklittlefs

    wget https://github.com/earlephilhower/mklittlefs/releases/download/3.2.0/x86_64-linux-gnu-mklittlefs-975bd0f.tar.gz
    tar xf x86_64-linux-gnu-mklittlefs-975bd0f.tar.gz

## Install esptool

    pip install esptool

## Configure your WiFi and Coinos credentials

    # edit data/config.txt

## Create the LittleFS partition

    mklittlefs -c data -p 256 -b 4096 -s 0x20000 littlefs.img

## Flashing

    arduino-cli compile -b esp32:esp32:esp32c3 --libraries $PWD/libraries --build-property build.partitions=min_spiffs -u -p /dev/ttyACM0
    esptool.py --port /dev/ttyACM0 write_flash 0x3D0000 littlefs.img
