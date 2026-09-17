# Cadeira Odontológica - ESP32-S3 + BLE

Projeto de controle de cadeira odontológica/ginecológica com firmware para **ESP32-S3** e aplicativo Flutter via **Bluetooth Low Energy (BLE)**.

## Estrutura

| Pasta | Conteúdo |
|-------|----------|
| `ESP32/esp32_bluetooth_chair/` | Firmware PlatformIO (`default_envs = esp32s3`, board `esp32-s3-devkitc-1`, flash 16MB com OTA) |
| `ESP32/mapa_pinos_esp32s3_i2c_2_18.md` | Mapa de pinos da placa ESP32-S3 (GPIOs + PCF8574) |
| `Floater/` | App Flutter (`flutter_blue_plus`) para Android/iOS/Windows/Linux/Web |
| `Supervisorio/` | Supervisório / gerenciador de cadeiras |

## Firmware

```bash
cd ESP32/esp32_bluetooth_chair
pio run -e esp32s3 -t upload
```

Ambientes alternativos (`esp32s3_i2c_*`) variam apenas os pinos I2C do PCF8574. Veja `ESP32/esp32_bluetooth_chair/README.md` para comandos BLE/Serial/MQTT, OTA e configuração.

## App

```bash
cd Floater
flutter pub get
flutter run
```

Veja `Floater/README.md` para detalhes.
