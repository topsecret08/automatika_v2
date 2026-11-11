# Сборка

## 1. ESP-IDF v5.1+
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf && git checkout v5.1.4
./install.sh esp32s3
. ./export.sh

## 2. QR-код
mkdir -p main/third_party && cd main/third_party
wget https://raw.githubusercontent.com/nayuki/QR-Code-generator/master/c/qrcodegen.c
wget https://raw.githubusercontent.com/nayuki/QR-Code-generator/master/c/qrcodegen.h

## 3. Сборка
cd ../..
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor