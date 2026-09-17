#!/usr/bin/env bash
# Copia o firmware PlatformIO (src/main.cpp) para o sketch Arduino IDE (firmware.h).
set -euo pipefail
cd "$(dirname "$0")"
SRC="../esp32_bluetooth_chair/src/main.cpp"
DST="esp32_bluetooth_chair_s3/firmware.h"
{
  echo "// GERADO AUTOMATICAMENTE por sync_arduino_sketch.sh a partir de ESP32/esp32_bluetooth_chair/src/main.cpp"
  echo "// Nao edite este arquivo; edite o main.cpp e rode o script novamente."
  echo "#pragma once"
  cat "$SRC"
} > "$DST"
cp ../esp32_bluetooth_chair/partitions_16MB_ota.csv esp32_bluetooth_chair_s3/partitions.csv
echo "OK: $DST atualizado"
