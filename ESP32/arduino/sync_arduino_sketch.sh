#!/usr/bin/env bash
# Gera o sketch Arduino IDE a partir do firmware PlatformIO (src/main.cpp):
#  - esp32_bluetooth_chair_s3/firmware.h  (usado pelo .ino modular)
#  - esp32_bluetooth_chair_s3_single.ino  (arquivo unico para copiar/colar na IDE)
set -euo pipefail
cd "$(dirname "$0")"
SRC="../esp32_bluetooth_chair/src/main.cpp"
DIR="esp32_bluetooth_chair_s3"
{
  echo "// GERADO AUTOMATICAMENTE por sync_arduino_sketch.sh a partir de ESP32/esp32_bluetooth_chair/src/main.cpp"
  echo "// Nao edite este arquivo; edite o main.cpp e rode o script novamente."
  echo "#pragma once"
  cat "$SRC"
} > "$DIR/firmware.h"
cp ../esp32_bluetooth_chair/partitions_16MB_ota.csv "$DIR/partitions.csv"
{
  echo "// ARQUIVO UNICO gerado por sync_arduino_sketch.sh - copie/cole inteiro na Arduino IDE."
  sed '/^#include "firmware.h"/d' "$DIR/esp32_bluetooth_chair_s3.ino"
  echo
  echo "// ===================== firmware (ESP32/esp32_bluetooth_chair/src/main.cpp) ====================="
  cat "$SRC"
} > esp32_bluetooth_chair_s3_single/esp32_bluetooth_chair_s3_single.ino
echo "OK: $DIR/firmware.h e esp32_bluetooth_chair_s3_single/*.ino atualizados"
