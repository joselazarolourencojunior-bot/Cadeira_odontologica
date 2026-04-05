# ESP32 Bluetooth Chair - Cadeira Odontológica

## Descrição

Projeto de controle de cadeira odontológica utilizando ESP32 com:

- **Bluetooth Serial (SPP)** - Comunicação com app Flutter
- **WiFiManager** - Configuração WiFi via portal captivo
- **Supabase** - Telemetria, horímetro e bloqueio remoto
- **Encoder Virtual** - Rastreamento de posição dos motores
- **Número de Série Único** - Baseado no MAC ID do ESP32

## Identificação Única

Cada cadeira possui um **número de série único** gerado automaticamente:

- **Serial:** `CADEIRA-XXXXXXXXXXXX` (baseado no MAC address)
- **Bluetooth:** `CadeiraOdonto-XXXX` (últimos 4 dígitos do MAC)

Isso permite identificar cada cadeira de forma única no sistema Supabase.

## Hardware Necessário

- ESP32 DevKit V1 ou similar
- Módulo de relés (8 canais)
- Buzzer
- LED indicador
- Botões de controle

## Pinagem ESP32

### Entradas (Botões)

| Pino | Função                          |
|------|---------------------------------|
| 5    | DE - Deitar (Encosto)           |
| 34   | SA - Subir Assento              |
| 17   | DA - Descer Assento             |
| 13   | SE - Sentar (Encosto)           |
| 22   | VZ - Posição Ginecológica       |
| 18   | SP - Subir Pernas               |
| 19   | DP - Descer Pernas              |
| 21   | PT - Posição de Parto           |
| 23   | RF - Refletor / Reset WiFi (5s) |
| 15   | M1 - Memória 1                  |

### Saídas (Relés e Indicadores)

| Pino | Função              |
|------|---------------------|
| 33   | Relé Subir Assento  |
| 25   | Relé Descer Assento |
| 27   | Relé Sentar         |
| 26   | Relé Deitar         |
| 12   | Relé Subir Pernas   |
| 14   | Relé Descer Pernas  |
| 4    | Relé Refletor       |
| 2    | LED Indicador       |
| 32   | Buzzer              |
| 16   | AD_PIC              |

## Comandos Bluetooth

O dispositivo aparece como **"CadeiraOdonto-XXXX"** no pareamento (XXXX = últimos 4 dígitos do MAC).

### Comandos de Controle

| Comando       | Resposta             | Descrição                   |
|---------------|----------------------|-----------------------------|
| `RF`          | `RF:ON/OFF`          | Liga/Desliga Refletor       |
| `SP`          | `SP:ON/OFF/LIMIT`    | Subir Pernas                |
| `DP`          | `DP:ON/OFF/LIMIT`    | Descer Pernas               |
| `SA`          | `SA:ON/OFF/LIMIT`    | Subir Assento               |
| `DA`          | `DA:ON/OFF/LIMIT`    | Descer Assento              |
| `SE`          | `SE:ON/OFF/LIMIT`    | Sentar (Encosto para cima)  |
| `DE`          | `DE:ON/OFF/LIMIT`    | Deitar (Encosto para baixo) |
| `VZ`          | `VZ:EXEC/DONE`       | Posição Ginecológica        |
| `PT`          | `PT:EXEC/DONE`       | Posição de Parto            |
| `M1`          | `M1:EXEC/DONE/SAVED` | Memória 1                   |
| `AT_SEG/STOP` | `AT_SEG:OK`          | Parada de Emergência        |
| `STATUS`      | JSON                 | Status Completo             |
| `HORIMETRO`   | `HORIMETRO:X.XX`     | Horas de uso                |
| `PING`        | `PONG`               | Teste de conexão            |

### Comandos WiFi

| Comando       | Descrição                    |
|---------------|------------------------------|
| `WIFI_STATUS` | Verifica conexão WiFi        |
| `WIFI_IP`     | Retorna endereço IP          |
| `WIFI_CONFIG` | Abre portal de configuração  |
| `WIFI_RESET`  | Apaga credenciais e reinicia |

## Instalação

### Opção 1: PlatformIO (Recomendado)

1. Instale o [VS Code](https://code.visualstudio.com/)
2. Instale a extensão PlatformIO
3. Abra esta pasta como projeto
4. Conecte o ESP32
5. Clique em "Upload" (seta para direita na barra inferior)

### Opção 2: Arduino IDE

1. Instale o [Arduino IDE](https://www.arduino.cc/en/software)
2. Adicione o suporte ao ESP32:
   - Vá em `Arquivo > Preferências`
   - Em "URLs Adicionais", adicione: `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
3. Instale as bibliotecas:
   - WiFiManager by tzapu
   - ArduinoJson by Benoit Blanchon
4. Selecione a placa: `Ferramentas > Placa > ESP32 Dev Module`
5. Faça o upload do código

## Configuração WiFi

### Primeira Configuração

1. Na primeira inicialização, o ESP32 cria uma rede WiFi: **"CadeiraOdonto-XXXX"** (XXXX = últimos 4 dígitos do MAC)
2. Conecte-se a esta rede (senha: **12345678**)
3. Acesse o portal de configuração que abrirá automaticamente
4. Selecione sua rede WiFi e insira a senha

**Nota**: O nome da rede WiFi é o mesmo do Bluetooth para facilitar identificação.

### Resetar WiFi

- **Via Botão:** Segure o botão RF por 5 segundos
- **Via Bluetooth:** Envie o comando `WIFI_RESET`

## Configuração Supabase

O número de série é gerado automaticamente a partir do MAC ID do ESP32.

Edite apenas as credenciais do Supabase no arquivo `main.cpp`:

```cpp
const char* SUPABASE_URL = "https://seu-projeto.supabase.co";
const char* SUPABASE_KEY = "sua-chave-api";
```

## Integração Flutter

Use a biblioteca `flutter_bluetooth_serial` para comunicação:

```dart
// Enviar comando
connection.output.add(Uint8List.fromList(utf8.encode("RF\n")));

// Receber resposta
connection.input.listen((data) {
  String response = utf8.decode(data);
  print(response); // "RF:ON" ou "RF:OFF"
});
```

## Notas de Segurança

- O sistema para todos os movimentos após 30 segundos de ativação contínua
- Qualquer botão físico pressionado durante movimento automático para a operação
- O comando `AT_SEG` pode ser enviado via Bluetooth para parada de emergência
- A cadeira pode ser bloqueada remotamente via Supabase
- O horímetro registra todas as horas de uso dos motores
