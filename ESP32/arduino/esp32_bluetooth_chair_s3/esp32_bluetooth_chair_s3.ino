// Sketch Arduino IDE para ESP32-S3 (equivalente ao env "esp32s3" do platformio.ini).
//
// Placa: ESP32S3 Dev Module
//   Flash Size: 16MB (128Mb)
//   Partition Scheme: 16M Flash (3MB APP/9.9MB FATFS)  (ou "Custom" com partitions.csv desta pasta)
//   PSRAM: OPI PSRAM (ou Disabled, conforme o módulo)
//   USB CDC On Boot: Enabled (se gravar/monitorar pela USB nativa)
//   Upload Speed: 921600
//
// Bibliotecas (Gerenciador de Bibliotecas):
//   WiFiManager (tzapu) >= 2.0.17
//   ArduinoJson (Benoit Blanchon) 6.x
//   PubSubClient (Nick O'Leary) >= 2.8
//
// O código-fonte fica em firmware.h (cópia de ../../esp32_bluetooth_chair/src/main.cpp).
// Para sincronizar após editar o main.cpp: execute ../sync_arduino_sketch.sh

// 1 = testes de bancada: sem BLE/WiFi/MQTT/Supabase/OTA (comandos pelo Monitor Serial). 0 = firmware completo.
#define OFFLINE_MODE 1

#define I2C_EARLY_TEST 0
#define I2C_SDA 2
#define I2C_SCL 18
#define I2C_CLOCK_HZ 100000
#define PCF8574_INT_PIN 1
#define PIN_RF 15
#define PIN_SA 14
#define PIN_GAVETA 20
#define PIN_TREN_INT_DESCE 3
#define PIN_INT_TREND_DESCE 8
#define PORTS_ONLY 0
#define PORTS_VERIFY 0
#define TEST_MODE 0
#define PIN_ENCODER1 4
#define PIN_ENCODER2 17
#define PIN_ENCODER3 19
#define PIN_RELE_SA 16
#define PIN_RELE_DA 9
#define PIN_RELE_SE 5
#define PIN_RELE_DE 6
#define PIN_RELE_SP 7
#define PIN_RELE_DP 10
#define RELE_DP_ACTIVE_LOW 1
#define PIN_RELE_REFLETOR 11
#define PIN_RELE_TREND_DESCE 48
#define PIN_RELE_TREND_SOBE 47
#define PIN_LED 12
#define PIN_BUZZER 13

#include "firmware.h"
