/*
  ========================================================
  CADEIRA ODONTOLÃ“GICA - ESP32
  Com Bluetooth Serial + WiFiManager + Supabase
  ========================================================
  
  Funcionalidades:
  - Controle via Bluetooth Serial (SPP)
  - ConfiguraÃ§Ã£o WiFi via portal captivo (WiFiManager)
  - IntegraÃ§Ã£o com Supabase para telemetria
  - HorÃ­metro para rastreamento de uso
  - Bloqueio remoto da cadeira
  - Sistema de encoder virtual para posicionamento
*/

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Update.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <new>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>
#include <esp_idf_version.h>
#include <esp_task_wdt.h>
#include "driver/gpio.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// Flag para detectar queda de energia
volatile bool powerFailDetected = false;

// ISR para o Brownout Detector
void IRAM_ATTR brownout_isr(void *arg) {
  powerFailDetected = true;
}

// Configura o Brownout Detector para gerar interrupção em vez de reset imediato
void setupBrownoutDetector() {
  // Nota: Para que o salvamento funcione durante a queda, sua fonte deve ter
  // capacitores grandes o suficiente para manter o ESP ligado por ~50ms 
  // após o trigger do brownout.
  
  // Se você tiver um pino monitorando a tensão da rede (ex: divisor de tensão),
  // defina-o aqui para uma detecção muito mais rápida e segura:
  // #define PIN_POWER_SENSE 34 
}

#if defined(CONFIG_IDF_TARGET_ESP32S2) || defined(ARDUINO_ESP32S2_DEV) || defined(ARDUINO_ESP32S2)
#define HAS_BLE 0
#else
#define HAS_BLE 1
#endif

#if HAS_BLE
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#endif

#ifndef USE_PCF8574
#define USE_PCF8574 1
#endif

static const int IO_PCF_BASE = 100;

#ifndef PCF8574_ADDRESS
#define PCF8574_ADDRESS 0x20
#endif

#ifndef PCF8574_INT_PIN
#define PCF8574_INT_PIN -1
#endif

#ifndef PIN_SA
#define PIN_SA 34
#endif

#ifndef PIN_RF
#define PIN_RF 23
#endif

#ifndef PIN_GAVETA
#define PIN_GAVETA -1
#endif

#ifndef PIN_TREN_INT_DESCE
#define PIN_TREN_INT_DESCE -1
#endif

#ifndef PIN_INT_TREND_DESCE
#define PIN_INT_TREND_DESCE -1
#endif

#ifndef TREND_INPUT_MODE
#define TREND_INPUT_MODE INPUT_PULLDOWN
#endif

#ifndef PIN_TREN_INT_SOBE
#define PIN_TREN_INT_SOBE -1
#endif

#ifndef PIN_INT_TREND_SOBE
#define PIN_INT_TREND_SOBE -1
#endif

#ifndef PIN_ENCODER1
#define PIN_ENCODER1 -1
#endif

#ifndef PIN_ENCODER2
#define PIN_ENCODER2 -1
#endif

#ifndef PIN_ENCODER3
#define PIN_ENCODER3 -1
#endif

#ifndef PIN_ENCODER_TREND
#define PIN_ENCODER_TREND 46
#endif

#ifndef DISABLE_TREND_ENCODER
#define DISABLE_TREND_ENCODER 1
#endif

#if DISABLE_TREND_ENCODER
#undef PIN_ENCODER_TREND
#define PIN_ENCODER_TREND -1
#endif

#ifndef I2C_EARLY_TEST
#define I2C_EARLY_TEST 0
#endif

#ifndef I2C_CLOCK_HZ
#define I2C_CLOCK_HZ 100000
#endif

#ifndef PORTS_ONLY
#define PORTS_ONLY 0
#endif

#ifndef PORTS_VERIFY
#define PORTS_VERIFY 0
#endif

#ifndef TEST_MODE
#define TEST_MODE 0
#endif

#ifndef OFFLINE_DEMO
#define OFFLINE_DEMO 0
#endif

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "0.0.1"
#endif

static bool i2cPing(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

static String i2cScanSummary() {
  String out;
  for (uint8_t address = 0x03; address <= 0x77; address++) {
    if (i2cPing(address)) {
      if (out.length() > 0) out += ",";
      char buf[6];
      snprintf(buf, sizeof(buf), "0x%02X", address);
      out += buf;
    }
  }
  if (out.length() == 0) return "NONE";
  return out;
}

static bool i2cReady = false;
static bool pcf8574Available = false;

static bool pcf8574WriteByte(uint8_t address, uint8_t value) {
  if (!i2cReady) {
    return false;
  }
  if (!pcf8574Available) {
    pcf8574Available = i2cPing(address);
    if (!pcf8574Available) {
      return false;
    }
  }
  Wire.beginTransmission(address);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

static uint8_t pcf8574ReadByte(uint8_t address, bool* ok) {
  if (!i2cReady) {
    if (ok) *ok = false;
    return 0xFF;
  }
  if (!pcf8574Available) {
    pcf8574Available = i2cPing(address);
    if (!pcf8574Available) {
      if (ok) *ok = false;
      return 0xFF;
    }
  }
  int n = Wire.requestFrom(static_cast<int>(address), 1);
  if (n < 1 || Wire.available() < 1) {
    n = Wire.requestFrom(static_cast<int>(address), 1);
    if (n < 1 || Wire.available() < 1) {
      if (ok) *ok = false;
      return 0xFF;
    }
  }
  if (ok) *ok = true;
  return static_cast<uint8_t>(Wire.read());
}

void executaComandoBluetooth(String cmd, const char* origin);
static void inicializaEstadosDeBordaBotoes();
extern bool lastButtonState_GAVETA;

static void processaComandosSerialDebug() {
  static String serialCmd;
  static uint32_t lastRxAtMs = 0;
  while (Serial.available() > 0) {
    char c = static_cast<char>(Serial.read());
    lastRxAtMs = millis();
    
    if (c == '\n' || c == '\r') {
      if (serialCmd.length() > 0) {
        String cmd = serialCmd;
        serialCmd = "";
        cmd.trim();
        Serial.print("\n[SERIAL] Executando: "); Serial.println(cmd);
        executaComandoBluetooth(cmd, "SERIAL");
      }
      continue;
    }
    
    // Aceita apenas caracteres imprimiveis (ASCII 32 a 126)
    if (c >= 32 && c <= 126) {
      if (serialCmd.length() < 64) {
        serialCmd += c;
      }
    }
  }
  
  // Aumentado timeout para 500ms para evitar disparos falsos por ruido ou digitacao lenta
  if (serialCmd.length() > 0 && lastRxAtMs > 0 && (millis() - lastRxAtMs) > 500) {
    String cmd = serialCmd;
    serialCmd = "";
    cmd.trim();
    if (cmd.length() > 0) {
      Serial.print("\n[SERIAL_TIMEOUT] Executando: "); Serial.println(cmd);
      executaComandoBluetooth(cmd, "SERIAL");
    }
  }
}

static inline int PCF_PIN(uint8_t pin) {
  return IO_PCF_BASE + pin;
}

#if USE_PCF8574
static volatile bool pcf8574InterruptPending = false;
static volatile uint32_t pcf8574InterruptCount = 0;
static uint8_t pcf8574CachedIn = 0xFF;
static uint32_t pcf8574CachedAtMs = 0;
static uint8_t pcf8574Out = 0xFF;
static uint8_t pcf8574LastIn = 0xFF;
static bool pcf8574UseInterrupt = false;

static void IRAM_ATTR IO_onPcf8574Interrupt() {
  pcf8574InterruptPending = true;
  pcf8574InterruptCount++;
}

static const char* pcf8574PinLabel(uint8_t p) {
  switch (p) {
    case 0: return "M1";
    case 1: return "SE";
    case 2: return "PT";
    case 3: return "VZ";
    case 4: return "DP";
    case 5: return "SP";
    case 6: return "DE";
    case 7: return "DA";
    default: return "?";
  }
}

static void pcf8574PrintStatus(uint8_t in) {
  Serial.println("--- STATUS DOS BOTOES PCF8574 ---");
  Serial.print("M1: "); Serial.print((in & (1U << 0)) ? "1" : "0");
  Serial.print(" | SE: "); Serial.print((in & (1U << 1)) ? "1" : "0");
  Serial.print(" | PT: "); Serial.print((in & (1U << 2)) ? "1" : "0");
  Serial.print(" | VZ: "); Serial.println((in & (1U << 3)) ? "1" : "0");
  
  Serial.print("DP: "); Serial.print((in & (1U << 4)) ? "1" : "0");
  Serial.print(" | SP: "); Serial.print((in & (1U << 5)) ? "1" : "0");
  Serial.print(" | DE: "); Serial.print((in & (1U << 6)) ? "1" : "0");
  Serial.print(" | DA: "); Serial.println((in & (1U << 7)) ? "1" : "0");
  Serial.println("--------------------------------");
}

static void pcf8574ReportChanges(uint8_t oldIn, uint8_t newIn, const char* tag) {
  uint8_t diff = static_cast<uint8_t>(oldIn ^ newIn);
  if (diff == 0) {
    return;
  }
  for (uint8_t p = 0; p < 8; p++) {
    if ((diff & (1U << p)) == 0) {
      continue;
    }
    if (tag && tag[0] != '\0') {
      Serial.print(tag);
      Serial.print(" ");
    }
    Serial.print("P");
    Serial.print(p);
    Serial.print("(");
    Serial.print(pcf8574PinLabel(p));
    Serial.print(")=");
    Serial.println((newIn & (1U << p)) ? "1" : "0");
  }
  // Imprime a tabela completa quando houver mudanca (interrupcao)
  pcf8574PrintStatus(newIn);
}
#endif

extern const int SA;
extern const int RF;
extern int GAVETA;
extern const int TREN_INT_DESCE;
extern const int TREN_INT_SOBE;
extern const int INT_TREND_DESCE;
extern const int INT_TREND_SOBE;

static bool inputsDebugEnabled = false;
static uint32_t inputsDebugLastPollMs = 0;
static int lastGpioSa = -1;
static int lastGpioRf = -1;
static int lastGpioGaveta = -1;
static int lastGpioTrendInDesce = -1;
static int lastGpioTrendInSobe = -1;
static int lastGpioIntTrendDesce = -1;
static int lastGpioIntTrendSobe = -1;
static int lastGpioPcfInt = -1;
#if USE_PCF8574
static uint8_t inputsDebugLastPcf = 0xFF;
static uint32_t inputsDebugLastIrqCount = 0;
#endif

static void inputsDebugTick() {
  if (!inputsDebugEnabled) {
    return;
  }
  uint32_t now = millis();
  if ((now - inputsDebugLastPollMs) < 100) {
    return;
  }
  inputsDebugLastPollMs = now;

  int sa = digitalRead(SA);
  int rf = digitalRead(RF);
  int gav = (GAVETA >= 0 ? digitalRead(GAVETA) : HIGH);
  int tinD = (TREN_INT_DESCE >= 0 ? digitalRead(TREN_INT_DESCE) : HIGH);
  int tinS = (TREN_INT_SOBE >= 0 ? digitalRead(TREN_INT_SOBE) : HIGH);
  int intD = (INT_TREND_DESCE >= 0 ? digitalRead(INT_TREND_DESCE) : HIGH);
  int intS = (INT_TREND_SOBE >= 0 ? digitalRead(INT_TREND_SOBE) : HIGH);
  int pcfInt = (PCF8574_INT_PIN >= 0 ? digitalRead(static_cast<uint8_t>(PCF8574_INT_PIN)) : HIGH);

  if (sa != lastGpioSa || rf != lastGpioRf || gav != lastGpioGaveta ||
      tinD != lastGpioTrendInDesce || tinS != lastGpioTrendInSobe ||
      intD != lastGpioIntTrendDesce || intS != lastGpioIntTrendSobe ||
      pcfInt != lastGpioPcfInt) {
    Serial.print("[IN_DBG] SA=");
    Serial.print(sa == HIGH ? "1" : "0");
    Serial.print(" RF=");
    Serial.print(rf == HIGH ? "1" : "0");
    Serial.print(" GAVETA=");
    Serial.print((GAVETA >= 0) ? (gav == HIGH ? "1" : "0") : "NA");
    Serial.print(" TREN_D=");
    Serial.print((TREN_INT_DESCE >= 0) ? (tinD == HIGH ? "1" : "0") : "NA");
    Serial.print(" TREN_S=");
    Serial.print((TREN_INT_SOBE >= 0) ? (tinS == HIGH ? "1" : "0") : "NA");
    Serial.print(" INT_D=");
    Serial.print((INT_TREND_DESCE >= 0) ? (intD == HIGH ? "1" : "0") : "NA");
    Serial.print(" INT_S=");
    Serial.print((INT_TREND_SOBE >= 0) ? (intS == HIGH ? "1" : "0") : "NA");
    Serial.print(" PCF_INT=");
    Serial.println((PCF8574_INT_PIN >= 0) ? (pcfInt == HIGH ? "1" : "0") : "NA");

    lastGpioSa = sa;
    lastGpioRf = rf;
    lastGpioGaveta = gav;
    lastGpioTrendInDesce = tinD;
    lastGpioTrendInSobe = tinS;
    lastGpioIntTrendDesce = intD;
    lastGpioIntTrendSobe = intS;
    lastGpioPcfInt = pcfInt;
  }

#if USE_PCF8574
  if (pcf8574Available) {
    uint32_t irqCountSnapshot = pcf8574InterruptCount;
    if (pcf8574UseInterrupt && (pcf8574InterruptPending || irqCountSnapshot != inputsDebugLastIrqCount)) {
      Serial.print("[PCF8574_IRQ] CNT=");
      Serial.print(static_cast<uint32_t>(irqCountSnapshot));
      Serial.print(" LEVEL=");
      if (PCF8574_INT_PIN >= 0) {
        Serial.println(digitalRead(static_cast<uint8_t>(PCF8574_INT_PIN)) == HIGH ? "1" : "0");
      } else {
        Serial.println("NA");
      }
      inputsDebugLastIrqCount = irqCountSnapshot;

      bool okRead = false;
      uint8_t in = pcf8574ReadByte(static_cast<uint8_t>(PCF8574_ADDRESS), &okRead);
      if (okRead) {
        pcf8574ReportChanges(inputsDebugLastPcf, in, "[PCF8574]");
        inputsDebugLastPcf = in;
      } else {
        Serial.println("[PCF8574] READ FAIL");
      }
      pcf8574InterruptPending = false;
    } else if (!pcf8574UseInterrupt) {
      bool okRead = false;
      uint8_t in = pcf8574ReadByte(static_cast<uint8_t>(PCF8574_ADDRESS), &okRead);
      if (okRead && in != inputsDebugLastPcf) {
        pcf8574ReportChanges(inputsDebugLastPcf, in, "[PCF8574]");
        inputsDebugLastPcf = in;
      }
    }
  }
#endif
}

static inline bool IO_isPcfPin(int pin) {
  return pin >= IO_PCF_BASE;
}

static void i2cRecoverBus(int sda, int scl) {
  Wire.end();
  pinMode(sda, INPUT_PULLUP);
  pinMode(scl, INPUT_PULLUP);
  int sda0 = digitalRead(sda);
  int scl0 = digitalRead(scl);
  Serial.print("[I2C] Recover start SDA=");
  Serial.print(sda0);
  Serial.print(" SCL=");
  Serial.println(scl0);

  if (scl0 == LOW) {
    Serial.println("[I2C] Recover abort: SCL travado em LOW");
    return;
  }

  pinMode(scl, OUTPUT_OPEN_DRAIN);
  digitalWrite(scl, HIGH);
  delayMicroseconds(5);

  for (int i = 0; i < 9; i++) {
    if (digitalRead(sda) == HIGH) {
      break;
    }
    digitalWrite(scl, LOW);
    delayMicroseconds(6);
    digitalWrite(scl, HIGH);
    delayMicroseconds(6);
  }

  pinMode(sda, OUTPUT_OPEN_DRAIN);
  digitalWrite(sda, LOW);
  delayMicroseconds(6);
  digitalWrite(scl, HIGH);
  delayMicroseconds(6);
  digitalWrite(sda, HIGH);
  delayMicroseconds(6);

  pinMode(sda, INPUT_PULLUP);
  pinMode(scl, INPUT_PULLUP);

  Serial.print("[I2C] Recover end SDA=");
  Serial.print(digitalRead(sda));
  Serial.print(" SCL=");
  Serial.println(digitalRead(scl));
}

static void IO_begin() {
  #if defined(I2C_SDA) && defined(I2C_SCL)
  if (I2C_SDA == 1 || I2C_SCL == 1) {
    Serial.println("[I2C] Erro: GPIO1 causa reset/crash nesta placa. Troque SDA/SCL.");
    i2cReady = false;
    pcf8574Available = false;
    return;
  }
  pinMode(I2C_SDA, INPUT_PULLUP);
  pinMode(I2C_SCL, INPUT_PULLUP);
  Serial.print("[I2C] SDA=");
  Serial.print(I2C_SDA);
  Serial.print(" SCL=");
  Serial.println(I2C_SCL);
  Serial.print("[I2C] SDA level=");
  Serial.print(digitalRead(I2C_SDA));
  Serial.print(" SCL level=");
  Serial.println(digitalRead(I2C_SCL));
  if (digitalRead(I2C_SDA) == LOW || digitalRead(I2C_SCL) == LOW) {
    i2cRecoverBus(I2C_SDA, I2C_SCL);
  }
  if (digitalRead(I2C_SCL) == LOW) {
    Serial.println("[I2C] SCL travado em LOW. Algum circuito esta puxando para GND (ou conflito de pino).");
    i2cReady = false;
    pcf8574Available = false;
    return;
  }
  if (digitalRead(I2C_SDA) == LOW) {
    Serial.println("[I2C] SDA travado em LOW. Algum circuito esta puxando para GND (ou conflito de pino).");
    i2cReady = false;
    pcf8574Available = false;
    return;
  }
  i2cReady = Wire.begin(I2C_SDA, I2C_SCL);
  #else
  i2cReady = Wire.begin();
  #endif
  if (!i2cReady) {
    Serial.println("[I2C] Wire.begin() falhou");
    pcf8574Available = false;
    return;
  }
  Wire.setClock(I2C_CLOCK_HZ);
#if USE_PCF8574
  pcf8574Available = false;
  for (int attempt = 0; attempt < 5; attempt++) {
    if (i2cPing(PCF8574_ADDRESS)) {
      pcf8574Available = true;
      break;
    }
    delay(10);
  }
  if (!pcf8574Available) {
    String scan = i2cScanSummary();
    bool foundByScan = scan.indexOf("0x20") >= 0;
    Serial.print("[I2C] PCF8574 nao respondeu em 0x");
    Serial.println(PCF8574_ADDRESS, HEX);
    Serial.print("[I2C] Scan: ");
    Serial.println(scan);
    if (!foundByScan) {
      return;
    }
    pcf8574Available = true;
  }
  pcf8574WriteByte(PCF8574_ADDRESS, pcf8574Out);
  bool ok = false;
  pcf8574CachedIn = pcf8574ReadByte(PCF8574_ADDRESS, &ok);
  pcf8574CachedAtMs = millis();
  pcf8574LastIn = pcf8574CachedIn;
  if (PCF8574_INT_PIN >= 0) {
    int irq = digitalPinToInterrupt(static_cast<uint8_t>(PCF8574_INT_PIN));
    if (irq < 0) {
      Serial.println("[PCF8574_INT] Pino nao suporta interrupcao. Usando polling.");
      pcf8574UseInterrupt = false;
    } else {
      pinMode(static_cast<uint8_t>(PCF8574_INT_PIN), INPUT_PULLUP);
      attachInterrupt(irq, IO_onPcf8574Interrupt, FALLING);
      pcf8574UseInterrupt = true;
      Serial.print("[PCF8574_INT] Interrupcao habilitada no GPIO ");
      Serial.println(PCF8574_INT_PIN);
    }
  } else {
    pcf8574UseInterrupt = false;
  }
#endif
}

static void IO_pinMode(int pin, uint8_t mode) {
#if USE_PCF8574
  if (pin < 0) {
    return;
  }
#endif
#if USE_PCF8574
  if (IO_isPcfPin(pin)) {
    if (!pcf8574Available) {
      return;
    }
    uint8_t p = static_cast<uint8_t>(pin - IO_PCF_BASE);
    if (p > 7) {
      return;
    }
    pcf8574Out |= (1U << p);
    pcf8574WriteByte(PCF8574_ADDRESS, pcf8574Out);
    return;
  }
#endif
  pinMode(static_cast<uint8_t>(pin), mode);
}

static int IO_digitalRead(int pin) {
#if USE_PCF8574
  if (pin < 0) {
    return HIGH;
  }
  if (IO_isPcfPin(pin)) {
    if (!pcf8574Available) {
      return HIGH;
    }
    uint8_t p = static_cast<uint8_t>(pin - IO_PCF_BASE);
    if (p > 7) {
      return HIGH;
    }
    if (pcf8574UseInterrupt) {
      uint32_t now = millis();
      if (pcf8574InterruptPending || (now - pcf8574CachedAtMs) > 50) {
        bool ok = false;
        pcf8574CachedIn = pcf8574ReadByte(PCF8574_ADDRESS, &ok);
        pcf8574CachedAtMs = now;
        pcf8574InterruptPending = false;
      }
      return (pcf8574CachedIn & (1U << p)) ? HIGH : LOW;
    }
    bool ok = false;
    pcf8574CachedIn = pcf8574ReadByte(PCF8574_ADDRESS, &ok);
    return (pcf8574CachedIn & (1U << p)) ? HIGH : LOW;
  }
#endif
  return digitalRead(static_cast<uint8_t>(pin));
}

static void IO_digitalWrite(int pin, uint8_t value) {
#if USE_PCF8574
  if (pin < 0) {
    return;
  }
  if (IO_isPcfPin(pin)) {
    if (!pcf8574Available) {
      return;
    }
    uint8_t p = static_cast<uint8_t>(pin - IO_PCF_BASE);
    if (p > 7) {
      return;
    }
    if (value != LOW) {
      pcf8574Out |= (1U << p);
    }
    pcf8574WriteByte(PCF8574_ADDRESS, pcf8574Out);
    return;
  }
#endif
  digitalWrite(static_cast<uint8_t>(pin), value);
}

#define pinMode(pin, mode) IO_pinMode((pin), (mode))
#define digitalRead(pin) IO_digitalRead((pin))
#define digitalWrite(pin, value) IO_digitalWrite((pin), (value))

// ========== CONFIGURAÇÕES SUPABASE ==========
const char* SUPABASE_URL = "https://mkoqceekhnkpviixqnnk.supabase.co";
const char* SUPABASE_KEY = "sb_publishable_HLUfLEw2UuIWjzd5LfqLkw_oaodzV7V";
static String supabaseUrl = SUPABASE_URL;
static String supabaseKey = SUPABASE_KEY;
const char* SENHA_AP = "12345678";                  // Senha da rede de configuração (mínimo 8 caracteres)
// ============================================

static String mqttHost = "broker.emqx.io";
static uint16_t mqttPort = 8883;
static String mqttUser = "";
static String mqttPass = "";
static bool mqttUseTls = true;
static String mqttClientId = "";
static bool mqttCleanSession = true;
static uint32_t mqttRxCount = 0;
static uint32_t mqttTxCount = 0;
static uint32_t mqttLastRxMs = 0;
static uint32_t mqttLastTxMs = 0;
static SemaphoreHandle_t mqttMutex = NULL;
static QueueHandle_t mqttCommandQueue = NULL;
static QueueHandle_t mqttTxQueue = NULL;
static TaskHandle_t mqttTaskHandle = NULL;
typedef struct { char cmd[64]; } MqttCmdItem;
typedef struct { char topic[96]; char payload[768]; bool retain; } MqttTxItem;
String MQTT_TOPIC_BASE = "";
static bool ignoreLimitLocks = true;
static bool ignoreGavetaLock = true;
static bool seatTravelLearned = false;
static volatile bool mqttStatusDirty = false;
static uint32_t mqttLastStatusPublishMs = 0;
static bool otaInProgress = false;
static bool otaEnabled = false;
static String otaManifestUrl = "";
static uint32_t otaIntervalSec = 21600;
static uint32_t otaNextCheckAtMs = 0;
static String otaPendingVersion = "";
static String otaValidatedVersionToReport = "";
static String otaActiveDeviceUpdateId = "";
static String otaActiveFromVersion = "";
static String otaActiveToVersion = "";

typedef struct {
  bool available;
  bool mandatory;
  String version;
  String url;
  String md5;
  int size;
  int minBattery;
  int minRssi;
} OtaInfo;

typedef struct {
  bool found;
  String id;
  bool enabled;
  String currentFirmware;
} ChairOtaInfo;

static inline bool mqttLockMs(uint32_t ms) {
  if (!mqttMutex) {
    return true;
  }
  return xSemaphoreTakeRecursive(mqttMutex, pdMS_TO_TICKS(ms)) == pdTRUE;
}

static inline void mqttUnlock() {
  if (!mqttMutex) {
    return;
  }
  xSemaphoreGiveRecursive(mqttMutex);
}

WiFiClient mqttPlainClient;
static WiFiClientSecure* mqttSecureClientPtr = nullptr;
PubSubClient mqttClient(mqttPlainClient);

static inline WiFiClientSecure& mqttSecureClientRef() {
  if (!mqttSecureClientPtr) {
    mqttSecureClientPtr = new WiFiClientSecure();
  }
  return *mqttSecureClientPtr;
}

static inline void resetMqttSecureClient() {
  if (mqttSecureClientPtr) {
    delete mqttSecureClientPtr;
    mqttSecureClientPtr = nullptr;
  }
  mqttSecureClientPtr = new WiFiClientSecure();
}
static bool beepNetMqttOkDone = false;
static bool beepMqttErrorDone = false;
static bool beepSupabaseOkDone = false;
static uint32_t beepSupabaseErrorNextMs = 0;
static uint32_t mqttReadyAtMs = 0;
static bool mqttConnectEnabled = false;
static bool mqttPostConnectPending = false;
static bool mqttSubscribed = false;
static uint32_t mqttPostConnectUntilMs = 0;
#if OFFLINE_DEMO
static bool audibleErrorBeeps = false;
#else
static bool audibleErrorBeeps = true;
#endif
static uint32_t bootStartedAtMs = 0;
static uint32_t wifiOkAtMs = 0;
static uint32_t wifiLostAtMs = 0;
static uint32_t mqttOkAtMs = 0;
static uint32_t mqttLostAtMs = 0;
static uint32_t supabaseOkAtMs = 0;
static uint32_t supabaseFailAtMs = 0;
static uint32_t alarmNextMs = 0;

static bool beepSeqActive = false;
static uint8_t beepSeqRemaining = 0;
static uint32_t beepSeqNextMs = 0;
static bool beepSeqBuzzerOn = false;
static bool beepSeqRestorePulse = false;
static uint16_t beepSeqOnMs = 100;
static uint16_t beepSeqOffMs = 120;
static bool buzzerTestEnabled = false;
static bool buzzerTestIsOn = false;
static uint32_t buzzerTestNextOnMs = 0;
static uint32_t buzzerTestOffAtMs = 0;
static bool trendDebugEnabled = false;
static uint32_t trendDebugNextMs = 0;
static uint32_t trendDebugIntervalMs = 500;
static bool buzzerForceOnce = false;

// Número de série único baseado no MAC ID do ESP32
String NUMERO_SERIE_CADEIRA = "";
String NOME_DISPOSITIVO = "";  // Nome único: CadeiraOdonto-XXXX (usado em WiFi e Bluetooth)

Preferences preferences;
String comandoBLE = "";

#if HAS_BLE
BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristicTX = NULL;
BLECharacteristic* pCharacteristicRX = NULL;
bool bleClienteConectado = false;
static bool bleInicializado = false;
static BLEAdvertising* bleAdvertising = NULL;
static bool bleAdvertisingAtivo = false;
static bool blePendingHelloSync = false;
static uint32_t blePendingHelloSyncAtMs = 0;
static uint32_t blePendingHelloSyncLastSendMs = 0;
static uint8_t blePendingHelloSyncTries = 0;
static bool bleSawRxSinceConnect = false;
static uint32_t bleLastConnectAtMs = 0;
static uint32_t bleAdvCooldownUntilMs = 0;
static uint8_t bleQuickDiscCount = 0;
static uint32_t bleTxSuspendUntilMs = 0;
static int bleTxLastErrRc = 0;
static uint32_t bleTxFailCount = 0;
static uint32_t bleTxLastFailAtMs = 0;
static uint32_t bleTxLastOkAtMs = 0;
static String bleRxBuf;
static String bleCmdQueue[8];
static uint8_t bleCmdQHead = 0;
static uint8_t bleCmdQTail = 0;
static BLE2902* bleTxCccd = NULL;

static bool bleCmdQueueIsFull() {
  return static_cast<uint8_t>((bleCmdQTail + 1) % 8) == bleCmdQHead;
}

static bool bleCmdQueueIsEmpty() {
  return bleCmdQHead == bleCmdQTail;
}

static void bleCmdQueueClear() {
  bleCmdQHead = 0;
  bleCmdQTail = 0;
  for (uint8_t i = 0; i < 8; i++) {
    bleCmdQueue[i] = "";
  }
}

static bool bleCmdEnq(String cmd) {
  if (cmd.length() == 0) return false;
  if (bleCmdQueueIsFull()) {
    bleCmdQueueClear();
    return false;
  }
  bleCmdQueue[bleCmdQTail] = cmd;
  bleCmdQTail = static_cast<uint8_t>((bleCmdQTail + 1) % 8);
  return true;
}

static bool bleCmdDeq(String* out) {
  if (bleCmdQueueIsEmpty()) return false;
  if (out) *out = bleCmdQueue[bleCmdQHead];
  bleCmdQueue[bleCmdQHead] = "";
  bleCmdQHead = static_cast<uint8_t>((bleCmdQHead + 1) % 8);
  return true;
}

#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
#else
bool bleClienteConectado = false;
#endif
WiFiManager wifiManager;

static const uint32_t BLE_KEEP_MIN_INTERVAL_MS = 250;

// VariÃ¡veis de controle Supabase
bool cadeiraHabilitada = true;
bool manutencaoNecessaria = false;
bool supabaseMaintenanceRequestSent = false;
String supabaseUserId = "";
unsigned long ultimaAtualizacaoSupabase = 0;
const unsigned long INTERVALO_ATUALIZACAO_SUPABASE = 900000;  // Atualiza a cada 15 minutos
unsigned long ultimaVerificacaoStatus = 0;
const unsigned long INTERVALO_VERIFICACAO_STATUS = 900000;   // Verifica status a cada 15 minutos
unsigned long ultimoMonitoramentoSistema = 0;
const unsigned long INTERVALO_MONITORAMENTO = 10000;         // Monitora a cada 10 segundos

// MQTT
// Controle de publicação MQTT diária - removido, agora só recebe comandos

// HorÃ­metro
float horimetro = 0;
unsigned long tempoInicioMotor = 0;
bool motorLigado = false;
unsigned long totalMillisMotor = 0;

// Controle de verificação de status
bool primeiraVerificacaoStatus = true;

// Timeout para portal de configuraÃ§Ã£o (3 minutos)
const int TIMEOUT_CONFIG_WIFI = 180;

// Logo VIP Hospitalar em Base64 (WebP)
const char LOGO_VIP_HOSPITALAR[] PROGMEM = R"rawliteral(
<style>
  .logo-container { text-align: center; margin: 20px 0; }
  .logo-img { max-width: 150px; height: auto; }
  .titulo-empresa { color: #0066cc; font-size: 18px; font-weight: bold; margin-top: 10px; }
  .subtitulo { color: #666; font-size: 12px; }
</style>
<div class="logo-container">
  <img class="logo-img" src="data:image/webp;base64,UklGRgQPAABXRUJQVlA4WAoAAAAQAAAAlQAAlQAAQUxQSHEHAAABoMb8/+O0+d552+wNIU4aEqfsbDDZ6Z4MZXST1b1bshQUd++9S0W690KIZinq3kUg1IGCUHZRUISQhSzrZJ2+L+7w37k7W33XiJgA/C9cW0H5wvra6Z7UwnneG30jE9Gx4a+2TEkdbA1jjPfJ7BTB3TbOuNWvq1KDzghFh/0pgLyFCewtSX4VxxPBJ+3JzvkOEzpeluxqlcTwp2S3k4l2J7m+hM1PbhITfnVysyfupv8ipFjC1iU39CdsfpLblTBPkgtMJKgbBs1qbdu8qW1DlmQ5js7EjAWMIdVuKJABOb91jmQ1qDmZkOcdxvBvcMn+uQEnnNdNtxzpykTszYEhpZAb0049+fLes+EMSVYD+ZmY0PFpMKb/YsD/rlNa/QJw7gzLgXuXItA/Gwa9tBDwf+Bx3LQdyGu0HqD2YHgydeheN4zamgZMPXXgzwM+wLvRipC24qlBheTYlzeeJcGwl5QA/i9LA2+VAYWXWxJgTyutmRvIdUkw8JTLAf9uBxpDwMXTLMqc92aj9HkHpvYgsw0pZMlGj6NQgm2qd0NpKoGy9hIJkIraZ+FMBx5vF66XxKa1x9uWD0y7YXPca9OA2scfTuiD7e0P79q8otgtTwJH06YLLtjU4sAZd4YiFB2qEVvOeEergMsY/yE/0MwzOvH19tkOPQM73xXi70UmaDozJI931hgNjv1CPOqzHpIPZxgMRQeF2OGxIrVrhsEQ+FMoss1mQeSfJQbD7FERsl2yIg6cZTA0jgnFzpWsiN1pBpNvVEV4uMqS+IDBIN8sxKMzLYkBg8HeKcSBdEvqtBsMWV1CfNVjoGbjjFQYDTk9QnzBbRz/mqZENm9497AqpLbJRsOUESFlp3ESbqv4VIhfewyH+qgIY+eZDfAMCUXzjYclIyI8tcx0KDopwktNIK8V4qEK09keUEWeNwHkZ4X4Z7rZcHFM5E8zwPF4VIRfZ5qtLiKimgJpHwjFnncuM9fiqIhiDmROiFDZcZ6ppCtjIodMgtI+EapHTeXtpugzZsG8YRFB410XE7rINNJFVlI/TuES0wA3hA1nc7sS6gm8GqXwAY+J7NujRlu6tyehP0Uprt4tmwiud4zWRMOOLYCp7Xut6gXZXCj62pqUWTB7+aAldcD85aetR+1xWwCaxy2ntxRWKG9WLCYyHdYo32wpSlcJrNL+joUod2bAOnO+sgr14FwZVprdbQGRQwce8Euw2JJTZoj80KX/yauP333NUr9PgvXWRUzQW+bSdzrssgRrlpaOGO+HLCRB+YrUBPJLqQkcj0dTEqR9lJogI5yawN+fmmDBcGoiXZSaADeFUxP79mhKAtf7qQkc+1MTFB9ITVB+KDVBxWBqgosCiWkW+Ck7+ch2IHfJ4rjnu4C8xQ1xV9uTz3+dUlJyPxDaUazZFgo1aeQZbb+MDjw+06bjXdsbHnqsSkbBtlBo17bNy32A/ZpQqEK6KaT/QDnguO2+0H3nalytoQf9k0iXhkKhdXZNUygU2nnv6jK7YbLI8HzNOPksANeGKLVjoXQAnt0qSY7no2qUut+Vwb2fXG0b4KQtQJlKcliT/jW5eJLMr0n+OUXzFnWjj9lMYt85Qf1Yjw24MsbTHW8p77ri4AfuRNxNDpIlkzRMUjvK42FlVVxULzHJtWR4R44t7YYIebsEhawH0kqgGcz3XXKcsTIdqayyguT75dUZkA9TXRjh4yLyFh5ee5AfOfVus2X/SXZKRhvTyMPkMw4A0n1kbx5I3u6BtmqU/V64u8k6HQAguQUA/GSPr5/7fQIZh9md9QKVUr1W4Dryc4dxlBdaVq9ujGoKSAagLVKoNKCbDO/dHpAnK+gl5yRgB1lve4zHywWWkTuldeSmOLaTHZJx4nwWS0nadNwHyGbk7Y2SZE+OZqLz/RHyZKFY5ncczEIwyhul+LrIBcgm+yWd3fW3qozUwzjqyf6BgT5VU0/SoeP5gWwEMhp3nyT5hkY3tsMmNn+ULzmQ8wt7vXG5wqQDGCTzdEhG9q+RDDTR6PP5vGFNPsmATvYEuQoAHAUdpCpNcuomN4SkzSo7FtTU/UBWxrWCjJRVV3SRt+kcPc2jCyUYKDwfAMY00p/kO14AtrfIrzPgKwSQTlKuGuVgvtMuAWK+fk7+js4yjbybk/e4NTvujfFdt/HGNVilMPpCdcGsHSq5Bnj+l2Vu1+XkGKpG2e+FrlANGRkaGho6TI46NB2rV69eXDlA5fjQ0NDhCEeqNa35veQGs9i3q1RH+o7GyC9lzFI51tN1mlyn40vQO+SDPp/Pl0ayRqP9JRTmJyU+ny/vSTXWKmuwjjydZ5iYcnqeZlRRngUgXzwQURRF6d9oA7yvnlQURRm51Y7KEaXPO0mPojRrFEW5F54xJVoG7TMx5dXs/Yru6LASuVoCgJUTyreeTkW5FvKfMaXHYxD7kmCdT7OoITgT2qJFwWAwWCwBgG12MBgMnm0DvHUNc2Q9uSIYzNU0BIMlcDYE6+062UuCVe6KoG5DQ7A+E1pfXbDBPjMYzAdKlwTrfQb5v78AVlA4IGwHAACwKQCdASqWAJYAPm0ylUekIyIhKpD5iIANiU3eUKtJBNE6r/IOzowv3Dzla9/avwR+Q3TXm264P5Xn7/kvs3+4D3Bf0d/V3rj+YD9uP2q92T/b/rB7ufQH/lH+q60z0AP1y9Ln9yvhE/aX9svbCzXDtBr1PPj2eys+CfHV31xm/QXRd+uP9jvYA/VUzMkgRAy33acP12v9QLFBTO3Qdxjr5JAYDAoustLzfAPlvybg/fCSYDlXXxZ90AYce/PZA5RUXMT6kXQdcq/iH45Em5363F9v65poEcElTisHBLF+WS7M1ZY1ppbfZRKWlHlZAlb8ml/sD+ZYn92I2gPX1HdxwzgDOLAMx17W9BDxXdZUSnpYPqQHYSCb+v4I0y/YsgL5rOaBUrFN95otjWMRHdLf4eGGwTCQXbn+c04IBCcckiCk7LjP+R9WIxmIs0s5EK7tVx3yPgAA/vz4QAs9k7o4T0JD0/q2485zyDdFsiIowR/kipHrmr6MpqaxQtIpJ/cSIvK7yre5Kv+Vf5/EAr7laKrS/zVCCPWlwKWJhB+zUXvi/D/AII9odRtyTaQY3qnX5BygBCK/a1Io0UGAnDJpSjWBrtICOHxN5fcciWj0SIHMmKdVZOgQVT2Eb7gdCD6WQntXTC5L62s2tvIXZjjPOjxdLVBebRUKetarxkH+gZziydU6LxqsJLcWQFsQxzYjA96pr4UOgMQFhJtZdgii8eEABb4z97ODA7ie8qNA+be9+wW8ZQHOpYnkhYmoA1hI7nc/yRoE4CCxWuRxw/OyoeDGHNVfopoyku3/Vyf8QMNii9qjMYbscFxCFFeJ2VW3R7o65WjvgF103DZV2YXkI9BLG5Btw5mExWeze1USUez3qfbTW5WXC7iE87D6mBaIBrNty09adovxtuxz9V722f3Mmxfobr4BH8WjBY1xIw/sQ1TbvcYmtCgaM+54rZIrcqpgcmNndVMfNCQLPqDNYmx9aKAsKxUrETQNGbLGqMNsmExy6GzVoZQJmsb42FmyJvaRpZrcrrSiekQJhn28Uym9xUSEuFnv1i1ur+o5oELN+7hJb52vFx8QQTUhaN3xIw2mcKSXiys+h+4/Ot73p8fIJSMYQtzUKuCHlTMympA623gpriD4L91CJGvy+s0th/Pt6gPqAGywOSJcYoeywJYkVoO8TjbwSvMfGERx8p7PI4BPTbH9VbyrssKTGJadEPB/zms5MkkaI8+TNYFi4zenGHKDKQTih6tSmn1J6EPuDD0OzCKJwb1EmYq8zPnkCG22M7qxHqWiqWgXYC34V6oLF2buQkPwNK78c8wsby7+hp8MbPY/54B8fyvSeYeh1uPcXNUBXVsHWdFHRg+6kXloOjKNZA3LBDG/LvIaHnwtuC9dZbrTNOmP2T3JQtXhmqjOjwr+ilH/sijsxS4TJqQAiWhUDmoy2drFVIFowSUr6JvFQs7goNrftDvM6ZS0dfpSwYtGqFggOBZphw3+z1RRLLvrQv5Fkbl/Vq0svmc+5Hzb6Pai1WMU8cJaK9g0CkC0X0BnLSNTfzWaVBgidf1ol+fNhBkPTJEKBOaRrydUJ2PiCGzVR5dLlbqZd1BNfAcBfw4OTJbgjQhJDZ+QcUpkLif5b2XKrscJhYu0APltZ0iko26GkyJNDM0PC0ob9vH6giKZsTZwtg8cA/QY2mTretAO5PzSSmO7tokVq/lHifkwi7/gw9/caVUStQq4mUwzxIHI/RE13gAhzOUGRrJwT/v+RKHl6tGXNjfC/idTX5wqEjo3xcb5v6bS7cf7DvjNsAAQssahgTGFVM3snCO4IDUqb5SJObHOft7W6qZHO51vfsE4gHWe3bS9jUjoDyICfdKFCRPuXc7KjVAUlwKkvKZl/r6v9mTY0Azzak59ylVWGhaDEcr/pLTcX4EVBWAEfnzuJVT7lSPxUSRb1DaKnpTi31U5lvIg9C3q82OMLKzbefVK1Jd1aRzSau6NkCF0HjDoVx8AxofvEjB6xhMLUivbso9Q94aqmHTC6gC2+Juw+BTrEAfskaBbCl6lai7Ky8fWYgj6Zk5qAYU9MaqHqtszO8u8KUHP29ASYAaFzpbWznJRL529w1Jq1IROXKbuZxFLi4AnRW2Cix8mEu/M65pMegYkl6g/MPYDF3UTzKiRtMIvVcm0mjAJ/Cd8DffkIjam/JDFUuNWIYcJrmJ3WPMfJW9vvmsPq+3waOrKesZZuUJbnXl0U7hF6lChI/7WzshGJH8RYbzUcwxJp/d2SmSX4LNuYzwxk+Py7e5JjMUr0dRcHk1n3HwhCL/WnNqfa4j7F9UF0e2Fx+injOSO4EBI0d7l8jZjcfEVlINQMRfLhtGE5wixg6TOFQBKKsaQLj3eISmwt6bIuRrY8o8E414CaA3lPZfrpLhSMHnzmvbS4evHq6WZsG/bouG7+e0rX1MzUA/lp0P+s/v8aT8M0arojZ4wZT64/X8FH0hXk6b+skJDi2OJtL9ThZK+o7u/F5+uRgaTP49njThAT3eGO7IAAAAAAAAA" alt="VIP Hospitalar">
  <div class="titulo-empresa">VIP HOSPITALAR</div>
  <div class="subtitulo">Cadeira OdontolÃ³gica</div>
</div>
)rawliteral";

// Controle do botão de reset WiFi
unsigned long tempoPressionadoBotaoResetWifi = 0;
bool botaoResetWifiPressionado = false;

// ========== DECLARAÃ‡ÃƒO DAS FUNÃ‡Ã•ES ==========
String geraNumeroSerieDoMAC();
void processaComandosBluetooth();
void executaComandoBluetooth(String cmd, const char* origin);
void enviaStatusBluetooth();
static void bleSendAllSnapshotExtras();
static void bleStatusStreamTick();
void iniciaTimerMotor();
void paraTimerMotor();
void atualizaHorimetro();
void salvaHorimetro();
void carregaHorimetro();
void configuraWiFiManager();
void atualizaSupabase();
void verificacaoPeriodicaStatus();
void enviaHorimetroParaSupabase();
void registraCadeiraNoSupabase();
void verificaStatusCadeira();
void sincronizaNTP();
String getTimestamp();
void bipBloqueio();
void carregaPreferencias();
void executa_M1_bluetooth();
void contagem_tempo_incoder_virtual();
void Watch_Dog();
void monitora_tempo_rele();
void Button_Seg();
void iniciaCalibracaoSeNecessario();
void executaCalibracao();
void resetCalibracao();
void Button_geral();
void executa_vz();
void executa_vz_ini();
void executa_M1();
void executa_pt();
void AT_SEG();
void bip();
void bipLong();
void bipForce();
void enviarBLE(String msg);
void verificaBotaoResetWifi();
void resetaConfiguracoesWifi();
static void resetParaPrimeiraVez();
static void checkBootHoldResetTudo();
static void supabaseLogUsage(const char* action);
static void supabaseCreateMaintenanceRequestIfNeeded();
static void supabaseSaveMemoryPosition(int slot);
static bool supabaseLoadMemoryPositionFromDb(int slot);
static bool supabaseGetJson(const String& restPath, String& responseOut);
static void loadMotorTravelPreferences();
static void saveMotorTravelPreferences(bool force);
static void motorTravelAutosaveTick();
static void sendMotorTravelToSupabaseIfNeeded();
static bool supabaseUpsertMotorTravel();
static bool mqttPublish(const String& topic, const String& payload, bool retain);
static void mqttTaskMain(void* pvParameters);
void publicaStatusMQTT();

extern int incoder_virtual_encosto_service;
extern int incoder_virtual_asento_service;
extern int incoder_virtual_perneira_service;
extern bool estado_r;
extern bool estado_sp;
extern bool estado_dp;
extern bool estado_sa;
extern bool estado_da;
extern bool estado_se;
extern bool estado_de;
extern bool estado_trend_sobe;
extern bool estado_trend_desce;
extern const int TREN_INT_DESCE;
extern const int INT_TREND_DESCE;
extern const int TREN_INT_SOBE;
extern const int INT_TREND_SOBE;
extern bool faz_m1;
extern bool vzInicialEmAndamento;
extern unsigned long ultimoComandoBLE;
extern String ultimoCmdBLE;
extern unsigned long ultimoComandoRemoto;
extern unsigned long ultimoComandoTrendSobe;
extern unsigned long ultimoComandoTrendDesce;
extern unsigned long lastTrendCmdMs;
extern bool trendRemoteActive;

// ========== FUNÇÕES MQTT ==========
static bool mqttPublish(const String& topic, const String& payload, bool retain) {
  if (!mqttLockMs(20)) {
    Serial.println("[MQTT_TX] MUTEX_TIMEOUT");
    return false;
  }
  if (!mqttClient.connected()) {
    Serial.print("[MQTT_TX] OFFLINE topic=");
    Serial.println(topic);
    mqttUnlock();
    return false;
  }
  bool ok = mqttClient.publish(topic.c_str(), payload.c_str(), retain);
  mqttTxCount++;
  mqttLastTxMs = millis();
  Serial.print("[MQTT_TX] ");
  Serial.print(ok ? "OK " : "FAIL ");
  Serial.print("topic=");
  Serial.print(topic);
  Serial.print(" payload=");
  Serial.println(payload);
  mqttUnlock();
  return ok;
}

static bool mqttEnqueuePublish(const String& topic, const String& payload, bool retain) {
  if (!mqttTxQueue) {
    return false;
  }
  MqttTxItem item;
  memset(&item, 0, sizeof(item));
  strncpy(item.topic, topic.c_str(), sizeof(item.topic) - 1);
  strncpy(item.payload, payload.c_str(), sizeof(item.payload) - 1);
  item.retain = retain;
  return xQueueSend(mqttTxQueue, &item, 0) == pdTRUE;
}

static void otaLoadPreferences() {
  Preferences p;
  if (!p.begin("ota", false)) {
    return;
  }
  otaEnabled = p.isKey("enabled") ? p.getBool("enabled", true) : true;
  otaManifestUrl = p.isKey("manifest") ? p.getString("manifest", "") : "";
  otaIntervalSec = p.isKey("interval") ? p.getUInt("interval", 21600) : 21600;
  otaPendingVersion = p.isKey("pending") ? p.getString("pending", "") : "";
  p.end();
}

static void otaSavePreferences() {
  Preferences p;
  if (!p.begin("ota", false)) {
    return;
  }
  p.putBool("enabled", otaEnabled);
  p.putString("manifest", otaManifestUrl);
  p.putUInt("interval", otaIntervalSec);
  p.putString("pending", otaPendingVersion);
  p.end();
}

static int otaCompareVersions(const String& a, const String& b) {
  int ai[4] = {0, 0, 0, 0};
  int bi[4] = {0, 0, 0, 0};
  int ac = 0;
  int bc = 0;
  int last = 0;
  for (int i = 0; i <= a.length(); i++) {
    if (i == a.length() || a[i] == '.') {
      if (ac < 4) ai[ac++] = a.substring(last, i).toInt();
      last = i + 1;
    }
  }
  last = 0;
  for (int i = 0; i <= b.length(); i++) {
    if (i == b.length() || b[i] == '.') {
      if (bc < 4) bi[bc++] = b.substring(last, i).toInt();
      last = i + 1;
    }
  }
  for (int i = 0; i < 4; i++) {
    if (ai[i] < bi[i]) return -1;
    if (ai[i] > bi[i]) return 1;
  }
  return 0;
}

static bool otaParseManifestJson(const String& body, OtaInfo& out) {
  out.available = false;
  out.mandatory = false;
  out.version = "";
  out.url = "";
  out.md5 = "";
  out.size = 0;

  DynamicJsonDocument doc(3072);
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    return false;
  }

  JsonVariant root = doc.as<JsonVariant>();
  if (root.is<JsonArray>() && root.size() > 0) {
    root = root[0];
  }
  if (!root.is<JsonObject>()) {
    return false;
  }

  JsonObject obj = root.as<JsonObject>();
  if (obj.containsKey("update_available")) {
    out.available = obj["update_available"].as<bool>();
  }
  if (obj.containsKey("mandatory")) {
    out.mandatory = obj["mandatory"].as<bool>();
  }
  if (obj.containsKey("version")) {
    out.version = obj["version"].as<String>();
  }
  if (obj.containsKey("url")) {
    out.url = obj["url"].as<String>();
  }
  if (obj.containsKey("md5")) {
    out.md5 = obj["md5"].as<String>();
  }
  if (obj.containsKey("size")) {
    out.size = obj["size"].as<int>();
  }

  if (!out.available) {
    if (out.version.length() > 0 && otaCompareVersions(out.version, FIRMWARE_VERSION) > 0 && out.url.length() > 0) {
      out.available = true;
    }
  }
  if (out.available && out.url.length() == 0) {
    out.available = false;
  }
  return true;
}

static bool supabasePatchJson(const String& restPath, const String& payload) {
  if (WiFi.status() != WL_CONNECTED) return false;
  if (ESP.getFreeHeap() < 30000) return false;

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  String url = supabaseUrl + restPath;
  if (!http.begin(client, url)) {
    return false;
  }

  http.setTimeout(10000);
  http.addHeader("Content-Type", "application/json");
  if (supabaseKey.length() > 0) {
    http.addHeader("apikey", supabaseKey);
    http.addHeader("Authorization", String("Bearer ") + supabaseKey);
  }
  http.addHeader("Prefer", "return=minimal");

  int httpCode = http.PATCH(payload);
  bool ok = (httpCode == 200 || httpCode == 204);
  if (!ok && httpCode > 0) {
    Serial.print("[ERRO] Supabase PATCH ");
    Serial.print(restPath);
    Serial.print(" ");
    Serial.print(httpCode);
    Serial.print(" ");
    Serial.println(http.getString());
  }
  http.end();
  return ok;
}

static bool otaFetchManifest(OtaInfo& out) {
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }
  if (!otaEnabled || otaManifestUrl.length() == 0) {
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setTimeout(30000);
  http.begin(client, otaManifestUrl);
  if (supabaseKey.length() > 0 && otaManifestUrl.startsWith(supabaseUrl)) {
    http.addHeader("apikey", supabaseKey);
    http.addHeader("Authorization", String("Bearer ") + supabaseKey);
  }
  http.addHeader("User-Agent", String("ESP32-Chair/") + FIRMWARE_VERSION);
  http.addHeader("Accept", "application/json, */*");
  http.addHeader("Accept-Encoding", "identity");
  http.addHeader("Cache-Control", "no-cache");
  http.addHeader("Connection", "close");

  int httpCode = -1;
  if (otaManifestUrl.indexOf("/rpc/") >= 0) {
    http.addHeader("Content-Type", "application/json");
    StaticJsonDocument<256> req;
    req["p_device_id"] = NUMERO_SERIE_CADEIRA;
    req["p_current_version"] = FIRMWARE_VERSION;
    req["p_is_esp32"] = true;
    String reqBody;
    serializeJson(req, reqBody);
    httpCode = http.POST(reqBody);
  } else {
    httpCode = http.GET();
  }

  if (httpCode != 200) {
    http.end();
    return false;
  }
  String body = http.getString();
  http.end();
  return otaParseManifestJson(body, out);
}

static bool supabasePostJsonResponse(const String& restPath, const String& payload, String& responseOut) {
  responseOut = "";
  if (WiFi.status() != WL_CONNECTED) return false;
  if (ESP.getFreeHeap() < 30000) return false;

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  String url = supabaseUrl + restPath;
  if (!http.begin(client, url)) {
    return false;
  }

  http.setTimeout(10000);
  http.addHeader("Content-Type", "application/json");
  if (supabaseKey.length() > 0) {
    http.addHeader("apikey", supabaseKey);
    http.addHeader("Authorization", String("Bearer ") + supabaseKey);
  }
  http.addHeader("Prefer", "return=representation");

  int httpCode = http.POST(payload);
  bool ok = (httpCode == 200 || httpCode == 201);
  if (ok) {
    responseOut = http.getString();
  } else if (httpCode > 0) {
    Serial.print("[ERRO] Supabase POST ");
    Serial.print(restPath);
    Serial.print(" ");
    Serial.print(httpCode);
    Serial.print(" ");
    Serial.println(http.getString());
  }
  http.end();
  return ok;
}

static int otaGetBatteryPercent() {
  return -1;
}

static bool otaFetchFromSupabaseTables(OtaInfo& out, ChairOtaInfo& chairOut) {
  out.available = false;
  out.mandatory = false;
  out.version = "";
  out.url = "";
  out.md5 = "";
  out.size = 0;
  out.minBattery = -1;
  out.minRssi = -999;
  chairOut.found = false;
  chairOut.id = "";
  chairOut.enabled = true;
  chairOut.currentFirmware = "";

  String chairResp;
  String chairPath = "/rest/v1/chairs?select=id,enabled,current_firmware&serial_number=eq." + NUMERO_SERIE_CADEIRA + "&limit=1";
  if (supabaseGetJson(chairPath, chairResp)) {
    DynamicJsonDocument doc(1536);
    DeserializationError err = deserializeJson(doc, chairResp);
    if (!err && doc.is<JsonArray>() && doc.size() > 0) {
      JsonObject c = doc[0].as<JsonObject>();
      chairOut.found = true;
      chairOut.id = c["id"] | "";
      chairOut.enabled = c["enabled"] | true;
      chairOut.currentFirmware = c["current_firmware"] | "";
    }
  }

  Serial.print("[OTA] chair ");
  Serial.print(chairOut.found ? "FOUND" : "NOT_FOUND");
  Serial.print(" enabled=");
  Serial.print(chairOut.enabled ? "1" : "0");
  Serial.print(" current_firmware=");
  Serial.println(chairOut.currentFirmware.length() ? chairOut.currentFirmware : "NA");

  if (chairOut.found && !chairOut.enabled) {
    return true;
  }

  String fwResp;
  String fwPath = "/rest/v1/firmware_versions?select=version,download_url,file_size,md5_hash,min_battery,min_rssi,mandatory,enabled,published_at&enabled=eq.true&order=published_at.desc&limit=1";
  if (!supabaseGetJson(fwPath, fwResp)) {
    return false;
  }

  DynamicJsonDocument doc(2048);
  DeserializationError err = deserializeJson(doc, fwResp);
  if (err || !doc.is<JsonArray>() || doc.size() == 0) {
    return false;
  }
  JsonObject f = doc[0].as<JsonObject>();
  out.version = f["version"] | "";
  out.url = f["download_url"] | "";
  out.md5 = f["md5_hash"] | "";
  out.size = f["file_size"] | 0;
  out.mandatory = f["mandatory"] | false;
  out.minBattery = f["min_battery"] | -1;
  out.minRssi = f["min_rssi"] | -999;

  Serial.print("[OTA] target version=");
  Serial.print(out.version.length() ? out.version : "NA");
  Serial.print(" size=");
  Serial.print(out.size);
  Serial.print(" md5=");
  Serial.print(out.md5.length() ? "SET" : "NA");
  Serial.print(" min_rssi=");
  Serial.print(out.minRssi);
  Serial.print(" min_battery=");
  Serial.print(out.minBattery);
  Serial.print(" mandatory=");
  Serial.println(out.mandatory ? "1" : "0");

  if (out.version.length() == 0 || out.url.length() == 0) {
    return true;
  }
  if (chairOut.currentFirmware.length() > 0 && otaCompareVersions(out.version, chairOut.currentFirmware) <= 0) {
    Serial.println("[OTA] target <= current_firmware (no update)");
    return true;
  }
  if (otaCompareVersions(out.version, FIRMWARE_VERSION) > 0) {
    out.available = true;
  }
  Serial.print("[OTA] compare fwVersion=");
  Serial.print(FIRMWARE_VERSION);
  Serial.print(" -> update_available=");
  Serial.println(out.available ? "1" : "0");
  return true;
}

static void otaUpdateChairLastUpdateCheck() {
  String ts = getTimestamp();
  if (ts.length() == 0 || ts == "null") return;
  StaticJsonDocument<128> doc;
  doc["last_update_check"] = ts;
  String payload;
  serializeJson(doc, payload);
  bool ok = supabasePatchJson("/rest/v1/chairs?serial_number=eq." + NUMERO_SERIE_CADEIRA, payload);
  Serial.print("[OTA] chairs.last_update_check ");
  Serial.println(ok ? "OK" : "FAIL");
}

static void otaUpdateChairLastUpdateAttempt() {
  String ts = getTimestamp();
  if (ts.length() == 0 || ts == "null") return;
  StaticJsonDocument<128> doc;
  doc["last_update_attempt"] = ts;
  String payload;
  serializeJson(doc, payload);
  bool ok = supabasePatchJson("/rest/v1/chairs?serial_number=eq." + NUMERO_SERIE_CADEIRA, payload);
  Serial.print("[OTA] chairs.last_update_attempt ");
  Serial.println(ok ? "OK" : "FAIL");
}

static String otaDeviceUpdatesInsertDownloading(const ChairOtaInfo& chair, const OtaInfo& info, const String& fromVersion) {
  if (!chair.found || chair.id.length() == 0) return "";
  String ts = getTimestamp();
  if (ts.length() == 0 || ts == "null") ts = "";

  StaticJsonDocument<384> doc;
  doc["device_id"] = chair.id;
  doc["from_version"] = fromVersion;
  doc["to_version"] = info.version;
  doc["status"] = "downloading";
  doc["error_message"] = nullptr;
  if (ts.length() > 0) doc["started_at"] = ts;

  String payload;
  serializeJson(doc, payload);

  String resp;
  if (!supabasePostJsonResponse("/rest/v1/device_updates", payload, resp)) {
    return "";
  }

  DynamicJsonDocument outDoc(1024);
  DeserializationError err = deserializeJson(outDoc, resp);
  if (!err && outDoc.is<JsonArray>() && outDoc.size() > 0) {
    String id = outDoc[0]["id"] | "";
    Serial.print("[OTA] device_updates downloading id=");
    Serial.println(id.length() ? id : "NA");
    return id;
  }
  return "";
}

static void otaDeviceUpdatesFinalize(const String& updateId, const String& status, const String& errorMessage, int durationSeconds, int bytesDownloaded) {
  if (updateId.length() == 0) return;
  String ts = getTimestamp();
  if (ts.length() == 0 || ts == "null") ts = "";

  StaticJsonDocument<384> doc;
  doc["status"] = status;
  if (errorMessage.length() > 0) doc["error_message"] = errorMessage;
  else doc["error_message"] = nullptr;
  if (ts.length() > 0) doc["completed_at"] = ts;
  if (durationSeconds >= 0) doc["duration_seconds"] = durationSeconds;
  if (bytesDownloaded >= 0) doc["bytes_downloaded"] = bytesDownloaded;

  String payload;
  serializeJson(doc, payload);
  bool ok = supabasePatchJson("/rest/v1/device_updates?id=eq." + updateId, payload);
  Serial.print("[OTA] device_updates ");
  Serial.print(updateId);
  Serial.print(" -> ");
  Serial.print(status);
  Serial.print(" ");
  Serial.println(ok ? "OK" : "FAIL");
}

static void otaUpdateChairAfterSuccess(const OtaInfo& info) {
  String ts = getTimestamp();
  if (ts.length() == 0 || ts == "null") ts = "";

  StaticJsonDocument<384> doc;
  doc["current_firmware"] = info.version;
  if (ts.length() > 0) {
    doc["last_update"] = ts;
    doc["last_update_check"] = ts;
    doc["last_update_attempt"] = ts;
  }
  String payload;
  serializeJson(doc, payload);
  bool ok = supabasePatchJson("/rest/v1/chairs?serial_number=eq." + NUMERO_SERIE_CADEIRA, payload);
  Serial.print("[OTA] chairs.current_firmware=");
  Serial.print(info.version);
  Serial.print(" ");
  Serial.println(ok ? "OK" : "FAIL");
}

static bool otaFetchUpdateInfo(OtaInfo& out, ChairOtaInfo& chairOut) {
  if (otaManifestUrl.length() > 0) {
    chairOut.found = false;
    chairOut.id = "";
    chairOut.enabled = true;
    chairOut.currentFirmware = "";
    return otaFetchManifest(out);
  }
  return otaFetchFromSupabaseTables(out, chairOut);
}

static bool otaDownloadAndUpdate(const OtaInfo& info) {
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }
  if (info.url.length() == 0) {
    return false;
  }

  uint32_t startedAtMs = millis();
  otaInProgress = true;
  Serial.print("[OTA] START from=");
  Serial.print(otaActiveFromVersion.length() ? otaActiveFromVersion : String(FIRMWARE_VERSION));
  Serial.print(" to=");
  Serial.println(info.version.length() ? info.version : "NA");
  enviarBLE("OTA:START");
  mqttEnqueuePublish(MQTT_TOPIC_BASE + "tx_cmd", "OTA_START", false);
  mqttStatusDirty = true;

  if (mqttLockMs(200)) {
    if (mqttClient.connected()) {
      mqttClient.disconnect();
    }
    mqttUnlock();
  }

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setTimeout(30000);
  http.begin(client, info.url);
  if (supabaseKey.length() > 0 && info.url.startsWith(supabaseUrl)) {
    http.addHeader("apikey", supabaseKey);
    http.addHeader("Authorization", String("Bearer ") + supabaseKey);
  }
  http.addHeader("User-Agent", String("ESP32-Chair/") + FIRMWARE_VERSION);
  http.addHeader("Accept", "application/octet-stream, */*");
  http.addHeader("Accept-Encoding", "identity");
  http.addHeader("Cache-Control", "no-cache");
  http.addHeader("Connection", "close");

  Serial.print("[OTA] DOWNLOAD url=");
  Serial.println(info.url);
  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    http.end();
    otaInProgress = false;
    otaDeviceUpdatesFinalize(otaActiveDeviceUpdateId, "failed", String("HTTP ") + httpCode, static_cast<int>((millis() - startedAtMs) / 1000), 0);
    mqttEnqueuePublish(MQTT_TOPIC_BASE + "tx_cmd", "OTA_FAIL_HTTP", false);
    enviarBLE("OTA:FAIL");
    Serial.print("[OTA] FAIL HTTP=");
    Serial.println(httpCode);
    return false;
  }

  int contentLength = http.getSize();
  int expected = info.size;
  if (expected > 0) {
    contentLength = expected;
  }
  if (contentLength <= 0) {
    http.end();
    otaInProgress = false;
    otaDeviceUpdatesFinalize(otaActiveDeviceUpdateId, "failed", "LEN", static_cast<int>((millis() - startedAtMs) / 1000), 0);
    mqttEnqueuePublish(MQTT_TOPIC_BASE + "tx_cmd", "OTA_FAIL_LEN", false);
    enviarBLE("OTA:FAIL");
    Serial.println("[OTA] FAIL LEN");
    return false;
  }

  Serial.print("[OTA] INSTALL prepare size=");
  Serial.print(contentLength);
  Serial.print(" md5=");
  Serial.println(info.md5.length() ? "SET" : "NA");
  if (info.md5.length() > 0) {
    Update.setMD5(info.md5.c_str());
  }
  if (!Update.begin(contentLength)) {
    http.end();
    otaInProgress = false;
    otaDeviceUpdatesFinalize(otaActiveDeviceUpdateId, "failed", String("BEGIN ") + Update.getError(), static_cast<int>((millis() - startedAtMs) / 1000), 0);
    mqttEnqueuePublish(MQTT_TOPIC_BASE + "tx_cmd", "OTA_FAIL_BEGIN", false);
    enviarBLE("OTA:FAIL");
    Serial.print("[OTA] FAIL BEGIN err=");
    Serial.println(Update.getError());
    return false;
  }

  WiFiClient* stream = http.getStreamPtr();
  size_t writtenTotal = 0;
  uint32_t lastProgressMs = millis();
  const uint32_t progressEveryMs = 1500;
  uint8_t buf[1024];
  while (http.connected() && static_cast<int32_t>(writtenTotal - static_cast<size_t>(contentLength)) < 0) {
    size_t avail = stream->available();
    if (avail == 0) {
      delay(1);
      continue;
    }
    size_t toRead = avail;
    if (toRead > sizeof(buf)) toRead = sizeof(buf);
    int r = stream->readBytes(buf, toRead);
    if (r <= 0) {
      delay(1);
      continue;
    }
    size_t w = Update.write(buf, static_cast<size_t>(r));
    writtenTotal += w;
    if (w != static_cast<size_t>(r)) {
      Update.abort();
      http.end();
      otaInProgress = false;
      otaDeviceUpdatesFinalize(otaActiveDeviceUpdateId, "failed", String("WRITE_MISMATCH ") + Update.getError(), static_cast<int>((millis() - startedAtMs) / 1000), static_cast<int>(writtenTotal));
      mqttEnqueuePublish(MQTT_TOPIC_BASE + "tx_cmd", "OTA_FAIL_WRITE", false);
      enviarBLE("OTA:FAIL");
      Serial.print("[OTA] FAIL WRITE_MISMATCH err=");
      Serial.println(Update.getError());
      return false;
    }
    uint32_t now = millis();
    if ((now - lastProgressMs) >= progressEveryMs) {
      lastProgressMs = now;
      int pct = (contentLength > 0) ? static_cast<int>((writtenTotal * 100ULL) / static_cast<uint64_t>(contentLength)) : 0;
      Serial.print("[OTA] DOWNLOADING ");
      Serial.print(pct);
      Serial.print("% (");
      Serial.print(static_cast<uint32_t>(writtenTotal));
      Serial.print("/");
      Serial.print(contentLength);
      Serial.println(")");
    }
  }

  if (writtenTotal == 0) {
    Update.abort();
    http.end();
    otaInProgress = false;
    otaDeviceUpdatesFinalize(otaActiveDeviceUpdateId, "failed", String("WRITE ") + Update.getError(), static_cast<int>((millis() - startedAtMs) / 1000), 0);
    mqttEnqueuePublish(MQTT_TOPIC_BASE + "tx_cmd", "OTA_FAIL_WRITE", false);
    enviarBLE("OTA:FAIL");
    Serial.print("[OTA] FAIL WRITE err=");
    Serial.println(Update.getError());
    return false;
  }
  Serial.print("[OTA] DOWNLOADED bytes=");
  Serial.println(static_cast<uint32_t>(writtenTotal));

  bool okEnd = Update.end();
  bool okFinished = Update.isFinished();
  http.end();
  if (!okEnd || !okFinished) {
    otaInProgress = false;
    otaDeviceUpdatesFinalize(otaActiveDeviceUpdateId, "failed", String("END ") + Update.getError(), static_cast<int>((millis() - startedAtMs) / 1000), static_cast<int>(writtenTotal));
    mqttEnqueuePublish(MQTT_TOPIC_BASE + "tx_cmd", "OTA_FAIL_END", false);
    enviarBLE("OTA:FAIL");
    Serial.print("[OTA] FAIL END err=");
    Serial.print(Update.getError());
    Serial.print(" end=");
    Serial.print(okEnd ? "1" : "0");
    Serial.print(" finished=");
    Serial.println(okFinished ? "1" : "0");
    return false;
  }

  otaPendingVersion = info.version;
  otaSavePreferences();
  otaUpdateChairAfterSuccess(info);
  otaDeviceUpdatesFinalize(otaActiveDeviceUpdateId, "success", "", static_cast<int>((millis() - startedAtMs) / 1000), static_cast<int>(writtenTotal));
  mqttEnqueuePublish(MQTT_TOPIC_BASE + "tx_cmd", "OTA_OK_REBOOT", false);
  enviarBLE("OTA:OK");
  Serial.println("[OTA] OK REBOOT");
  delay(2000);
  ESP.restart();
  return true;
}

static void otaValidateAfterBoot() {
  if (otaPendingVersion.length() == 0) {
    return;
  }
  if (otaPendingVersion == FIRMWARE_VERSION) {
    otaValidatedVersionToReport = otaPendingVersion;
    otaPendingVersion = "";
    otaSavePreferences();
    mqttEnqueuePublish(MQTT_TOPIC_BASE + "tx_cmd", "OTA_VALIDATED", false);
    enviarBLE("OTA:VALIDATED");
  }
}

static void otaTick() {
  if (!otaEnabled) {
    return;
  }
  if (otaInProgress) {
    return;
  }
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }
  uint32_t now = millis();
  if (otaNextCheckAtMs == 0) {
    otaNextCheckAtMs = now + 5000;
    return;
  }
  if (now < otaNextCheckAtMs) {
    return;
  }
  otaNextCheckAtMs = now + (otaIntervalSec * 1000UL);

  Serial.println("[OTA] CHECK");

  if (otaValidatedVersionToReport.length() > 0) {
    String ts = getTimestamp();
    StaticJsonDocument<256> doc;
    doc["current_firmware"] = otaValidatedVersionToReport;
    doc["last_update"] = ts;
    String payload;
    serializeJson(doc, payload);
    supabasePatchJson("/rest/v1/chairs?serial_number=eq." + NUMERO_SERIE_CADEIRA, payload);
    otaValidatedVersionToReport = "";
  }

  otaUpdateChairLastUpdateCheck();

  OtaInfo info;
  ChairOtaInfo chair;
  if (otaFetchUpdateInfo(info, chair)) {
    if (chair.found && !chair.enabled) {
      Serial.println("[OTA] SKIP (chair disabled)");
      return;
    }
    if (info.available) {
      int rssi = WiFi.RSSI();
      if (info.minRssi > -200 && rssi < info.minRssi) {
        mqttEnqueuePublish(MQTT_TOPIC_BASE + "tx_cmd", "OTA_SKIP_RSSI", false);
        Serial.print("[OTA] SKIP RSSI rssi=");
        Serial.print(rssi);
        Serial.print(" min=");
        Serial.println(info.minRssi);
        return;
      }
      int bat = otaGetBatteryPercent();
      if (info.minBattery > 0 && bat >= 0 && bat < info.minBattery) {
        mqttEnqueuePublish(MQTT_TOPIC_BASE + "tx_cmd", "OTA_SKIP_BAT", false);
        Serial.print("[OTA] SKIP BAT bat=");
        Serial.print(bat);
        Serial.print(" min=");
        Serial.println(info.minBattery);
        return;
      }

      otaUpdateChairLastUpdateAttempt();
      otaActiveFromVersion = chair.currentFirmware.length() > 0 ? chair.currentFirmware : String(FIRMWARE_VERSION);
      otaActiveToVersion = info.version;
      otaActiveDeviceUpdateId = otaDeviceUpdatesInsertDownloading(chair, info, otaActiveFromVersion);
      Serial.print("[OTA] APPLY from=");
      Serial.print(otaActiveFromVersion);
      Serial.print(" to=");
      Serial.println(otaActiveToVersion);
      bool ok = otaDownloadAndUpdate(info);
      if (!ok) {
        otaActiveDeviceUpdateId = "";
        otaActiveFromVersion = "";
        otaActiveToVersion = "";
      }
      return;
    }
  }
  Serial.println("[OTA] NO_UPDATE");
}

static bool mqttIsControlCommand(const String& cmdUpper) {
  return cmdUpper == "DE" || cmdUpper == "SE" || cmdUpper == "SA" || cmdUpper == "DA" ||
         cmdUpper == "SP" || cmdUpper == "DP" || cmdUpper == "TS" || cmdUpper == "TD" || cmdUpper == "T0" || cmdUpper == "TREND_TEST" || cmdUpper == "RF" || cmdUpper == "VZ" ||
         cmdUpper == "PT" || cmdUpper == "M1" || cmdUpper == "STOP" || cmdUpper == "AT_SEG" ||
         cmdUpper == "STATUS";
}

static void mqttPublishTxCmd(const String& cmdUpper, const char* origin) {
  if (!mqttClient.connected()) {
    return;
  }
  if (origin && String(origin) == "MQTT") {
    return;
  }
  if (cmdUpper.startsWith("MQTT_")) {
    return;
  }
  if (!mqttIsControlCommand(cmdUpper)) {
    return;
  }
  mqttEnqueuePublish(MQTT_TOPIC_BASE + "tx_cmd", cmdUpper, false);
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("[MQTT] Mensagem recebida no tópico: ");
  Serial.println(topic);
  
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  mqttRxCount++;
  mqttLastRxMs = millis();
  Serial.print("[MQTT_RX] Payload: ");
  Serial.println(message);
  
  String cmd = message;
  cmd.trim();
  if (cmd.startsWith("CMD:")) {
    cmd = cmd.substring(4);
  }
  cmd.trim();
  cmd.toUpperCase();
  if (cmd.length() == 0) return;

  if (cmd == "STATUS") {
    publicaStatusMQTT();
    return;
  }

  if (mqttCommandQueue) {
    MqttCmdItem item;
    memset(&item, 0, sizeof(item));
    cmd.toCharArray(item.cmd, sizeof(item.cmd));
    xQueueSend(mqttCommandQueue, &item, 0);
  }
}

static void saveMqttPreferences();
static bool tlsHandshakeOnce(const String& host, uint16_t port);
static bool mqttRawConnackOnce(const String& host, uint16_t port, const String& clientId);

void reconnectMQTT() {
  if (!mqttClient.connected()) {
    static uint32_t mqttNextAttemptMs = 0;
    static uint8_t mqttNetFailStreak = 0;
    static uint8_t mqttTlsFailStreak = 0;
    uint32_t now = millis();
    if (static_cast<int32_t>(now - mqttNextAttemptMs) < 0) {
      return;
    }

    Serial.print("[MQTT] Tentando conectar...");
    
    String clientId = mqttClientId.length() > 0 ? mqttClientId : ("ESP32-" + NUMERO_SERIE_CADEIRA);
    String willTopic = MQTT_TOPIC_BASE + "lwt";

    bool locked = mqttLockMs(100);
    if (!locked) {
      Serial.println("falhou, mutex");
      mqttNextAttemptMs = now + 1000;
      return;
    }

    if (mqttUseTls) {
      resetMqttSecureClient();
      WiFiClientSecure& sc = mqttSecureClientRef();
      sc.stop();
      sc.setHandshakeTimeout(15);
      sc.setTimeout(20);
      sc.setInsecure();
      mqttClient.setClient(sc);
    } else {
      mqttClient.setClient(mqttPlainClient);
    }
    mqttClient.setSocketTimeout(5);
    mqttClient.setKeepAlive(30);

    mqttClient.setServer(mqttHost.c_str(), mqttPort);
    if (WiFi.status() == WL_CONNECTED) {
      IPAddress ip;
      if (WiFi.hostByName(mqttHost.c_str(), ip) == 1) {
        Serial.print("ip=");
        Serial.print(ip);
        Serial.print(" port=");
        Serial.print(mqttPort);
        Serial.print(" tls=");
        Serial.print(mqttUseTls ? "1" : "0");
        Serial.print(" clean=");
        Serial.println(mqttCleanSession ? "1" : "0");
      } else {
        Serial.print("dns_fail host=");
        Serial.print(mqttHost);
        Serial.print(" port=");
        Serial.print(mqttPort);
        Serial.print(" tls=");
        Serial.print(mqttUseTls ? "1" : "0");
        Serial.print(" clean=");
        Serial.println(mqttCleanSession ? "1" : "0");
      }
    }

    if (mqttUser.length() > 0 && mqttPass.length() == 0) {
      Serial.println("falhou, senha vazia (use MQTT_PASS=...)");
      mqttUnlock();
      mqttNextAttemptMs = now + 5000;
      return;
    }

    bool ok = false;
    if (mqttUser.length() == 0 && mqttPass.length() == 0) {
      ok = mqttClient.connect(
        clientId.c_str(),
        willTopic.c_str(),
        1,
        true,
        "offline"
      );
    } else if (mqttUser.length() > 0) {
      ok = mqttClient.connect(
        clientId.c_str(),
        mqttUser.c_str(),
        mqttPass.c_str(),
        willTopic.c_str(),
        1,
        true,
        "offline",
        mqttCleanSession
      );
    }
    if (ok) {
      Serial.println("conectado!");
      beepMqttErrorDone = false;
      mqttOkAtMs = millis();
      mqttLostAtMs = 0;
      mqttNetFailStreak = 0;
      mqttTlsFailStreak = 0;
      mqttPostConnectPending = true;
      mqttSubscribed = false;
      mqttPostConnectUntilMs = millis() + 5000;
      mqttUnlock();
      
    } else {
      Serial.print("falhou, rc=");
      int st = mqttClient.state();
      Serial.print(st);
      Serial.println(" tentando novamente em 5 segundos");
      mqttUnlock();
      if (mqttUseTls) {
        Serial.print("[MQTT_TLS] connect_fail port=");
        Serial.print(mqttPort);
        Serial.print(" fd=");
        Serial.println(mqttSecureClientPtr ? mqttSecureClientPtr->fd() : -1);
        char errBuf[160];
        memset(errBuf, 0, sizeof(errBuf));
        int errCode = mqttSecureClientPtr ? mqttSecureClientPtr->lastError(errBuf, sizeof(errBuf)) : 0;
        Serial.print("[MQTT_TLS] lastError=");
        Serial.print(errCode);
        Serial.print(" msg=");
        Serial.println(errBuf);
        static uint32_t mqttDiagNextMs = 0;
        if (static_cast<int32_t>(now - mqttDiagNextMs) >= 0) {
          mqttDiagNextMs = now + 30000;
          bool tlsOk = tlsHandshakeOnce(mqttHost, mqttPort);
          if (tlsOk) {
            String cid = mqttClientId.length() > 0 ? mqttClientId : ("ESP32-" + NUMERO_SERIE_CADEIRA);
            bool connackOk = mqttRawConnackOnce(mqttHost, mqttPort, cid);
            if (!connackOk) {
              Serial.println("[MQTT] Broker sem CONNACK no teste RAW");
            }
          }
        }
      }
      if (st == -2) {
        if (mqttNetFailStreak < 255) mqttNetFailStreak++;
        if (!mqttUseTls && mqttPort == 1883 && mqttNetFailStreak >= 1) {
          mqttUseTls = true;
          mqttPort = 8883;
          mqttTlsFailStreak = 0;
          Serial.println("[MQTT] Alternando para TLS (8883) por falha persistente em 1883");
          saveMqttPreferences();
          mqttNextAttemptMs = now + 1000;
          return;
        }
      } else {
        mqttNetFailStreak = 0;
      }
      if (mqttUseTls) {
        if (mqttTlsFailStreak < 255) mqttTlsFailStreak++;
        if (mqttTlsFailStreak >= 2) {
          mqttTlsFailStreak = 0;
          if (mqttPort != 8883) {
            mqttPort = 8883;
            Serial.println("[MQTT] Alternando TLS para porta 8883 por falha persistente");
            saveMqttPreferences();
            mqttNextAttemptMs = now + 1000;
            return;
          }
        }
      } else {
        mqttTlsFailStreak = 0;
      }
      mqttNextAttemptMs = now + 5000;
    }
  }
}

static bool tlsHandshakeOnce(const String& host, uint16_t port) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[TLS_TEST] WiFi desconectado");
    return false;
  }
  WiFiClientSecure client;
  client.stop();
  client.setHandshakeTimeout(15);
  client.setTimeout(20);
  client.setInsecure();
  uint32_t t0 = millis();
  bool ok = client.connect(host.c_str(), port);
  uint32_t dt = millis() - t0;
  Serial.print("[TLS_TEST] host=");
  Serial.print(host);
  Serial.print(" port=");
  Serial.print(port);
  Serial.print(" ok=");
  Serial.print(ok ? "1" : "0");
  Serial.print(" ms=");
  Serial.print(dt);
  Serial.print(" fd=");
  Serial.println(client.fd());
  client.stop();
  return ok;
}

static bool mqttRawConnackOnce(const String& host, uint16_t port, const String& clientId) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[MQTT_RAW] WiFi desconectado");
    return false;
  }

  WiFiClientSecure client;
  client.stop();
  client.setHandshakeTimeout(15);
  client.setTimeout(20);
  client.setInsecure();

  uint32_t t0 = millis();
  bool ok = client.connect(host.c_str(), port);
  uint32_t tlsMs = millis() - t0;

  Serial.print("[MQTT_RAW] tls host=");
  Serial.print(host);
  Serial.print(" port=");
  Serial.print(port);
  Serial.print(" ok=");
  Serial.print(ok ? "1" : "0");
  Serial.print(" ms=");
  Serial.print(tlsMs);
  Serial.print(" fd=");
  Serial.println(client.fd());

  if (!ok) {
    client.stop();
    return false;
  }

  uint8_t pkt[256];
  size_t cidLen = static_cast<size_t>(clientId.length());
  if (cidLen == 0 || cidLen > 200) {
    client.stop();
    return false;
  }

  size_t vhLen = 10;
  size_t payloadLen = 2 + cidLen;
  size_t remLen = vhLen + payloadLen;
  if (remLen > 127) {
    client.stop();
    return false;
  }

  size_t i = 0;
  pkt[i++] = 0x10;
  pkt[i++] = static_cast<uint8_t>(remLen);
  pkt[i++] = 0x00; pkt[i++] = 0x04;
  pkt[i++] = 'M'; pkt[i++] = 'Q'; pkt[i++] = 'T'; pkt[i++] = 'T';
  pkt[i++] = 0x04;
  pkt[i++] = 0x02;
  pkt[i++] = 0x00; pkt[i++] = 60;
  pkt[i++] = static_cast<uint8_t>((cidLen >> 8) & 0xFF);
  pkt[i++] = static_cast<uint8_t>(cidLen & 0xFF);
  for (size_t k = 0; k < cidLen; k++) {
    pkt[i++] = static_cast<uint8_t>(clientId[k]);
  }

  uint32_t t1 = millis();
  int wr = client.write(pkt, i);
  client.flush();
  uint32_t wrMs = millis() - t1;
  Serial.print("[MQTT_RAW] write bytes=");
  Serial.print(i);
  Serial.print(" wr=");
  Serial.print(wr);
  Serial.print(" ms=");
  Serial.println(wrMs);

  uint8_t resp[4];
  uint32_t deadline = millis() + 3000;
  size_t got = 0;
  while (static_cast<int32_t>(millis() - deadline) < 0 && got < sizeof(resp)) {
    if (!client.connected()) {
      break;
    }
    int av = client.available();
    if (av <= 0) {
      delay(10);
      continue;
    }
    int r = client.read(resp + got, sizeof(resp) - got);
    if (r > 0) got += static_cast<size_t>(r);
    else break;
  }

  Serial.print("[MQTT_RAW] resp_len=");
  Serial.println(got);
  Serial.print("[MQTT_RAW] connected=");
  Serial.println(client.connected() ? "1" : "0");
  if (got == 4) {
    Serial.print("[MQTT_RAW] resp=");
    Serial.print(resp[0], HEX); Serial.print(" ");
    Serial.print(resp[1], HEX); Serial.print(" ");
    Serial.print(resp[2], HEX); Serial.print(" ");
    Serial.println(resp[3], HEX);
  }

  bool connackOk = (got == 4 && resp[0] == 0x20 && resp[1] == 0x02 && resp[2] == 0x00 && resp[3] == 0x00);
  client.stop();
  return connackOk;
}

static void mqttTaskMain(void* pvParameters) {
  for (;;) {
    if (otaInProgress) {
      if (mqttClient.connected()) {
        if (mqttLockMs(50)) {
          mqttClient.disconnect();
          mqttUnlock();
        }
      }
      vTaskDelay(pdMS_TO_TICKS(250));
      continue;
    }
    if (WiFi.status() == WL_CONNECTED) {
      uint32_t now = millis();
      if (!mqttConnectEnabled) {
        vTaskDelay(pdMS_TO_TICKS(50));
        continue;
      }
      if (mqttReadyAtMs != 0 && static_cast<int32_t>(now - mqttReadyAtMs) < 0) {
        vTaskDelay(pdMS_TO_TICKS(50));
        continue;
      }
      reconnectMQTT();
      if (mqttClient.connected()) {
        if (mqttPostConnectPending && mqttPostConnectUntilMs != 0 && static_cast<int32_t>(now - mqttPostConnectUntilMs) >= 0) {
          mqttPostConnectPending = false;
          mqttPostConnectUntilMs = 0;
        }
        if (mqttPostConnectPending) {
          if (mqttLockMs(250)) {
            if (!mqttSubscribed) {
              mqttClient.setSocketTimeout(5);
              String commandTopic = MQTT_TOPIC_BASE + "command";
              bool subOk = mqttClient.subscribe(commandTopic.c_str(), 0);
              mqttSubscribed = subOk;
              Serial.print("[MQTT] Subscribe command ");
              Serial.print(subOk ? "OK " : "FAIL ");
              Serial.println(commandTopic);
            }
            if (mqttSubscribed) {
              String willTopic = MQTT_TOPIC_BASE + "lwt";
              bool pubOk = mqttClient.publish(willTopic.c_str(), "online", true);
              mqttTxCount++;
              mqttLastTxMs = millis();
              Serial.print("[MQTT] LWT online ");
              Serial.println(pubOk ? "OK" : "FAIL");
              publicaStatusMQTT();
              if (!beepNetMqttOkDone && WiFi.status() == WL_CONNECTED) {
                bip(); delay(120);
                bip(); delay(120);
                bip();
                Serial.println("[BUZZER] WiFi+MQTT OK");
                beepNetMqttOkDone = true;
                supabaseLogUsage("MQTT_CONNECTED");
              }
              mqttPostConnectPending = false;
              mqttPostConnectUntilMs = 0;
            }
            mqttUnlock();
          }
        }
        if (mqttLockMs(50)) {
          mqttClient.loop();
          if (mqttTxQueue) {
            MqttTxItem item;
            while (xQueueReceive(mqttTxQueue, &item, 0) == pdTRUE) {
              if (!mqttClient.connected()) {
                continue;
              }
              bool ok = mqttClient.publish(item.topic, item.payload, item.retain);
              mqttTxCount++;
              mqttLastTxMs = millis();
              Serial.print("[MQTT_TX] ");
              Serial.print(ok ? "OK " : "FAIL ");
              Serial.print("topic=");
              Serial.print(item.topic);
              Serial.print(" payload=");
              Serial.println(item.payload);
            }
          }
          mqttUnlock();
        }
      }
    } else {
      beepNetMqttOkDone = false;
      mqttPostConnectPending = false;
      mqttSubscribed = false;
      mqttPostConnectUntilMs = 0;
      if (mqttClient.connected()) {
        if (mqttLockMs(50)) {
          mqttClient.disconnect();
          mqttUnlock();
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

static bool isGavetaAbertaRaw();

void publicaStatusMQTT() {
  if (mqttClient.connected()) {
    String statusTopic = MQTT_TOPIC_BASE + "status";
    StaticJsonDocument<896> doc;
    doc["fwVersion"] = FIRMWARE_VERSION;
    doc["otaEnabled"] = otaEnabled;
    if (otaPendingVersion.length() > 0) {
      doc["otaPending"] = otaPendingVersion;
    }
    doc["wifiConnected"] = (WiFi.status() == WL_CONNECTED);
    if (WiFi.status() == WL_CONNECTED) {
      doc["wifiSsid"] = WiFi.SSID();
      doc["wifiIp"] = WiFi.localIP().toString();
    }
    doc["hourMeter"] = horimetro;
    doc["enabled"] = cadeiraHabilitada;
    doc["maintenanceRequired"] = manutencaoNecessaria;
    doc["reflectorOn"] = estado_r;
    doc["backUpOn"] = estado_se;
    doc["backDownOn"] = estado_de;
    doc["seatUpOn"] = estado_sa;
    doc["seatDownOn"] = estado_da;
    doc["upperLegsOn"] = estado_sp;
    doc["lowerLegsOn"] = estado_dp;
    doc["trendUpOn"] = estado_trend_sobe;
    doc["trendDownOn"] = estado_trend_desce;
    doc["backPosition"] = incoder_virtual_encosto_service;
    doc["seatPosition"] = incoder_virtual_asento_service;
    doc["legPosition"] = incoder_virtual_perneira_service;
    doc["gavetaOpen"] = isGavetaAbertaRaw();
    doc["gavetaLockIgnored"] = ignoreGavetaLock;
    if (TREN_INT_DESCE >= 0) doc["trenIntDown"] = (digitalRead(TREN_INT_DESCE) == HIGH);
    if (INT_TREND_DESCE >= 0) doc["trendIntDown"] = (digitalRead(INT_TREND_DESCE) == HIGH);
    if (TREN_INT_SOBE >= 0) doc["trenIntUp"] = (digitalRead(TREN_INT_SOBE) == HIGH);
    if (INT_TREND_SOBE >= 0) doc["trendIntUp"] = (digitalRead(INT_TREND_SOBE) == HIGH);
    String ts = getTimestamp();
    if (ts.length() > 0 && ts != "null") {
      doc["timestamp"] = ts;
    }

    String statusMsg;
    serializeJson(doc, statusMsg);
    mqttEnqueuePublish(statusTopic, statusMsg, true);
  }
}

#if HAS_BLE
static void beepSeqStart(uint8_t count, uint16_t onMs, uint16_t offMs);

class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    uint32_t now = millis();
    bleClienteConectado = true;
    bleLastConnectAtMs = now;
    bleAdvCooldownUntilMs = 0;
    if (bleAdvertising) {
      bleAdvertising->stop();
    }
    bleAdvertisingAtivo = false;
    blePendingHelloSync = true;
    blePendingHelloSyncAtMs = now;
    blePendingHelloSyncLastSendMs = 0;
    blePendingHelloSyncTries = 0;
    bleSawRxSinceConnect = false;
    bleRxBuf = "";
    bleCmdQueueClear();
    bleTxSuspendUntilMs = 0;
    bleTxLastErrRc = 0;
    bleTxFailCount = 0;
    bleTxLastFailAtMs = 0;
    bleTxLastOkAtMs = now;
    beepSeqStart(4, 100, 120);
    Serial.println("\n====================================");
    Serial.println("  [BLE] DISPOSITIVO CONECTADO!");
    Serial.println("====================================");
    Serial.print("[BLE] t=");
    Serial.println(now);
    Serial.print("Total de clientes conectados: ");
    Serial.println(pServer->getConnectedCount());
    Serial.println("====================================\n");
  };

  void onDisconnect(BLEServer* pServer) {
    uint32_t now = millis();
    bleClienteConectado = false;
    blePendingHelloSync = false;
    blePendingHelloSyncLastSendMs = 0;
    blePendingHelloSyncTries = 0;
    bleSawRxSinceConnect = false;
    bleTxSuspendUntilMs = 0;
    bleRxBuf = "";
    bleCmdQueueClear();
    Serial.println("\n====================================");
    Serial.println("  [BLE] DISPOSITIVO DESCONECTADO");
    Serial.println("====================================");
    Serial.print("[BLE] t=");
    Serial.println(now);
    Serial.println("[BLE] disconnect -> STOP ALL");
    AT_SEG();
    comandoBLE = "";
    ultimoCmdBLE = "";
    ultimoComandoBLE = 0;
    ultimoComandoRemoto = 0;
    ultimoComandoTrendSobe = 0;
    ultimoComandoTrendDesce = 0;
    lastTrendCmdMs = 0;
    trendRemoteActive = false;
    faz_m1 = false;
    vzInicialEmAndamento = false;

    uint32_t aliveMs = (bleLastConnectAtMs > 0) ? (now - bleLastConnectAtMs) : 0;
    if (aliveMs > 0 && aliveMs < 3000) {
      if (bleQuickDiscCount < 255) bleQuickDiscCount++;
    } else {
      bleQuickDiscCount = 0;
    }
    if (bleQuickDiscCount >= 3) {
      bleAdvCooldownUntilMs = now + 2000;
    } else {
      bleAdvCooldownUntilMs = 0;
    }

    if (bleAdvertising) {
      bleAdvertising->stop();
    }
    bleAdvertisingAtivo = false;

    if (bleAdvCooldownUntilMs != 0 && static_cast<int32_t>(now - bleAdvCooldownUntilMs) < 0) {
      Serial.print("[BLE] Cooldown adv ms=");
      Serial.println((uint32_t)(bleAdvCooldownUntilMs - now));
    } else {
      BLEDevice::startAdvertising();
      bleAdvertisingAtivo = true;
      Serial.println("[BLE] Advertising reiniciado");
      Serial.println("Aguardando nova conexao...");
    }
    Serial.println("====================================\n");
  }
};

class MyCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string rxValue = pCharacteristic->getValue();
    
    if (rxValue.length() > 0) {
      bleSawRxSinceConnect = true;
      for (size_t i = 0; i < rxValue.length(); i++) {
        char c = rxValue[i];
        bleRxBuf += c;
        if (bleRxBuf.length() > 160) {
          Serial.println("[BLE] RX overflow");
          bleRxBuf = "";
          bleCmdQueueClear();
          return;
        }
      }

      bool hadDelimiter = false;
      for (;;) {
        int idxN = bleRxBuf.indexOf('\n');
        int idxR = bleRxBuf.indexOf('\r');
        int idx = -1;
        if (idxN >= 0 && idxR >= 0) idx = min(idxN, idxR);
        else if (idxN >= 0) idx = idxN;
        else if (idxR >= 0) idx = idxR;
        if (idx < 0) break;
        hadDelimiter = true;
        String line = bleRxBuf.substring(0, idx);
        bleRxBuf = bleRxBuf.substring(idx + 1);
        line.trim();
        if (line.length() == 0) continue;
        bleCmdEnq(line);
        Serial.print("[BLE] RX: ");
        Serial.println(line);
      }

      if (!hadDelimiter && bleRxBuf.length() > 0 && rxValue.find('\n') == std::string::npos && rxValue.find('\r') == std::string::npos) {
        String line = bleRxBuf;
        bleRxBuf = "";
        line.trim();
        if (line.length() > 0) {
          bleCmdEnq(line);
          Serial.print("[BLE] RX: ");
          Serial.println(line);
        }
      }
    }
  }
};

class MyTxCallbacks: public BLECharacteristicCallbacks {
  void onStatus(BLECharacteristic* pCharacteristic, Status s, uint32_t code) {
    (void)pCharacteristic;
    if (s == BLECharacteristicCallbacks::Status::ERROR_GATT) {
      bleTxLastErrRc = (int)code;
      bleTxFailCount++;
      bleTxLastFailAtMs = millis();
      bleTxSuspendUntilMs = millis() + 2000;
      return;
    }
    if (s == BLECharacteristicCallbacks::Status::SUCCESS_NOTIFY) {
      bleTxLastOkAtMs = millis();
    }
  }
};
#endif

#if HAS_BLE
static void bleAtualizaDisponibilidade() {
  bool permitido = true;
  if (!bleInicializado) {
    Serial.println("\n====================================");
    Serial.println("  INICIALIZANDO BLE");
    Serial.println("====================================");
    Serial.print("Inicializando BLE com nome: ");
    Serial.println(NOME_DISPOSITIVO);

    BLEDevice::init(NOME_DISPOSITIVO.c_str());
    BLEDevice::setPower(ESP_PWR_LVL_P9);
    BLEDevice::setMTU(247);
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    BLEService *pService = pServer->createService(SERVICE_UUID);
    pCharacteristicTX = pService->createCharacteristic(
      CHARACTERISTIC_UUID_TX,
      BLECharacteristic::PROPERTY_NOTIFY
    );
    bleTxCccd = new BLE2902();
    pCharacteristicTX->addDescriptor(bleTxCccd);
    pCharacteristicTX->setCallbacks(new MyTxCallbacks());

    pCharacteristicRX = pService->createCharacteristic(
      CHARACTERISTIC_UUID_RX,
      BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
    );
    pCharacteristicRX->setCallbacks(new MyCallbacks());
    pService->start();

    bleAdvertising = BLEDevice::getAdvertising();
    bleAdvertising->addServiceUUID(SERVICE_UUID);
    bleAdvertising->setScanResponse(true);
    bleAdvertising->setMinInterval(0x00A0);
    bleAdvertising->setMaxInterval(0x0100);
    BLEDevice::startAdvertising();
    bleInicializado = true;
    bleAdvertisingAtivo = true;

    Serial.println("[OK] BLE iniciado e advertising ativo!");
    Serial.print("Nome BLE: ");
    Serial.println(NOME_DISPOSITIVO);
    Serial.println("UUID do Serviço: 6E400001-B5A3-F393-E0A9-E50E24DCCA9E");
    Serial.println("Aguardando conexão de cliente BLE...");
    Serial.println("====================================\n");
    return;
  }
  if (!bleClienteConectado && bleAdvertising && !bleAdvertisingAtivo) {
    uint32_t now = millis();
    if (bleAdvCooldownUntilMs != 0 && static_cast<int32_t>(now - bleAdvCooldownUntilMs) < 0) {
      return;
    }
    BLEDevice::startAdvertising();
    bleAdvertisingAtivo = true;
  }
}
#endif

void enviarBLE(String msg) {
#if HAS_BLE
  if (!bleClienteConectado || pCharacteristicTX == NULL) {
    return;
  }
  uint32_t now = millis();
  if (!bleSawRxSinceConnect && bleLastConnectAtMs != 0 && (now - bleLastConnectAtMs) < 1500) {
    return;
  }
  if (bleTxCccd != NULL && !bleTxCccd->getNotifications()) {
    return;
  }
  if (bleTxSuspendUntilMs != 0 && static_cast<int32_t>(now - bleTxSuspendUntilMs) < 0) {
    return;
  }
  if (pServer != NULL && pServer->getConnectedCount() == 0) {
    return;
  }
  if (!msg.endsWith("\n")) {
    msg += "\n";
  }
  if (msg.length() > 595) {
    msg = msg.substring(0, 594);
    msg += "\n";
  }
  {
    pCharacteristicTX->setValue(msg.c_str());
    pCharacteristicTX->notify();
    if (!msg.startsWith("STATUS:")) {
      Serial.print("[BLE] Enviado: ");
      Serial.println(msg);
    }
  }
#else
  Serial.println(msg);
#endif
}

static void enviarBLEQuiet(String msg) {
#if HAS_BLE
  if (!bleClienteConectado || pCharacteristicTX == NULL) {
    return;
  }
  uint32_t now = millis();
  if (!bleSawRxSinceConnect && bleLastConnectAtMs != 0 && (now - bleLastConnectAtMs) < 1500) {
    return;
  }
  if (bleTxCccd != NULL && !bleTxCccd->getNotifications()) {
    return;
  }
  if (bleTxSuspendUntilMs != 0 && static_cast<int32_t>(now - bleTxSuspendUntilMs) < 0) {
    return;
  }
  if (pServer != NULL && pServer->getConnectedCount() == 0) {
    return;
  }
  if (!msg.endsWith("\n")) {
    msg += "\n";
  }
  if (msg.length() > 595) {
    msg = msg.substring(0, 594);
    msg += "\n";
  }
  {
    pCharacteristicTX->setValue(msg.c_str());
    pCharacteristicTX->notify();
  }
#else
  (void)msg;
#endif
}

// Constantes - Pinos de ENTRADA (botÃµes)
const int M1 = PCF_PIN(0);
const int SE = PCF_PIN(1);
const int PT = PCF_PIN(2);
const int VZ = PCF_PIN(3);
const int DP = PCF_PIN(4);
const int SP = PCF_PIN(5);
const int DE = PCF_PIN(6);
const int DA = PCF_PIN(7);
const int SA = PIN_SA;
const int RF = PIN_RF;
int GAVETA = PIN_GAVETA;
static int gavetaIdleLevel = -1;
static int rfIdleLevel = -1;
const int ENCODER1 = PIN_ENCODER1;
const int ENCODER2 = PIN_ENCODER2;
const int ENCODER3 = PIN_ENCODER3;
const int ENCODER_TREND = PIN_ENCODER_TREND;
const int TREN_INT_DESCE = PIN_TREN_INT_DESCE;
const int INT_TREND_DESCE = PIN_INT_TREND_DESCE;
const int TREN_INT_SOBE = PIN_TREN_INT_SOBE;
const int INT_TREND_SOBE = PIN_INT_TREND_SOBE;

static void loadGavetaPinPreference() {
  Preferences p;
  if (!p.begin("cadeira", false)) {
    GAVETA = PIN_GAVETA;
    return;
  }
  GAVETA = p.getInt("gaveta_pin", PIN_GAVETA);
  p.end();
}

static void saveGavetaPinPreference() {
  Preferences p;
  if (!p.begin("cadeira", false)) {
    return;
  }
  p.putInt("gaveta_pin", GAVETA);
  p.end();
}

static bool isGavetaAberta() {
  if (ignoreGavetaLock) {
    return false;
  }
  if (GAVETA < 0) {
    return false;
  }
  if (gavetaIdleLevel == -1) {
    return digitalRead(GAVETA) == LOW;
  }
  return digitalRead(GAVETA) != gavetaIdleLevel;
}

static bool isGavetaAbertaRaw() {
  if (GAVETA < 0) {
    return false;
  }
  if (gavetaIdleLevel == -1) {
    return digitalRead(GAVETA) == LOW;
  }
  return digitalRead(GAVETA) != gavetaIdleLevel;
}

static uint32_t userButtonMaskRaw() {
  uint32_t mask = 0;
  if (digitalRead(M1) == LOW) mask |= (1U << 0);
  if (digitalRead(SE) == LOW) mask |= (1U << 1);
  if (digitalRead(PT) == LOW) mask |= (1U << 2);
  if (digitalRead(VZ) == LOW) mask |= (1U << 3);
  if (digitalRead(DP) == LOW) mask |= (1U << 4);
  if (digitalRead(SP) == LOW) mask |= (1U << 5);
  if (digitalRead(DE) == LOW) mask |= (1U << 6);
  if (digitalRead(DA) == LOW) mask |= (1U << 7);
  if (digitalRead(SA) == LOW) mask |= (1U << 8);

  if (RF >= 0) {
    bool rfRaw = digitalRead(RF);
    bool rfPressed = (rfIdleLevel >= 0) ? (rfRaw != (rfIdleLevel != 0)) : (rfRaw == LOW);
    if (rfPressed) mask |= (1U << 9);
  }

  if (isGavetaAbertaRaw()) mask |= (1U << 10);
  return mask;
}

static bool isAnyUserButtonActiveRaw() {
  return userButtonMaskRaw() != 0;
}

extern int fim_encosto_encoder;
extern int fim_asento_encoder;
extern int fim_perneira_encoder;

static void loadCalibracaoLimites() {
  Preferences p;
  if (!p.begin("cadeira", false)) {
    return;
  }
  bool done = p.getBool("cal_done", false);
  if (!done) {
    p.end();
    return;
  }
  int enc = p.getInt("encosto_max", fim_encosto_encoder);
  int ass = p.getInt("assento_max", fim_asento_encoder);
  int per = p.getInt("perneira_max", fim_perneira_encoder);
  p.end();
  if (enc > 0) fim_encosto_encoder = enc;
  if (ass > 0) fim_asento_encoder = ass;
  if (per > 0) fim_perneira_encoder = per;
  Serial.print("[CAL] Limites carregados ENC=");
  Serial.print(fim_encosto_encoder);
  Serial.print(" ASS=");
  Serial.print(fim_asento_encoder);
  Serial.print(" PER=");
  Serial.println(fim_perneira_encoder);
}

static void loadMqttPreferences() {
  Preferences p;
  if (!p.begin("mqtt", true)) {
    return;
  }
  if (p.isKey("host")) {
    mqttHost = p.getString("host", mqttHost);
  }
  if (p.isKey("port")) {
    uint32_t port = p.getUInt("port", 0);
    if (port > 0 && port <= 65535) {
      mqttPort = static_cast<uint16_t>(port);
    }
  }
  if (p.isKey("user")) {
    mqttUser = p.getString("user", "");
  }
  if (p.isKey("pass")) {
    mqttPass = p.getString("pass", "");
  }
  if (p.isKey("tls")) {
    mqttUseTls = p.getBool("tls", mqttUseTls);
  }
  if (p.isKey("cid")) {
    mqttClientId = p.getString("cid", "");
  }
  if (p.isKey("clean")) {
    mqttCleanSession = p.getBool("clean", mqttCleanSession);
  }
  p.end();
}

static void saveMqttPreferences() {
  Preferences p;
  if (!p.begin("mqtt", false)) {
    return;
  }
  p.putString("host", mqttHost);
  p.putUInt("port", mqttPort);
  p.putString("user", mqttUser);
  p.putString("pass", mqttPass);
  p.putBool("tls", mqttUseTls);
  p.putString("cid", mqttClientId);
  p.putBool("clean", mqttCleanSession);
  p.end();
}

static void applyMqttRuntimeConfig() {
  bool locked = false;
  if (mqttMutex) {
    locked = mqttLockMs(200);
    if (!locked) {
      return;
    }
  }
  if (mqttUseTls && mqttPort == 1883) {
    mqttPort = 8883;
  } else if (!mqttUseTls && mqttPort == 8883) {
    mqttPort = 1883;
  }
  if (mqttUseTls) {
    mqttSecureClientRef().setInsecure();
    mqttClient.setClient(mqttSecureClientRef());
  } else {
    mqttClient.setClient(mqttPlainClient);
  }
  if (WiFi.status() == WL_CONNECTED) {
    IPAddress ip;
    if (WiFi.hostByName(mqttHost.c_str(), ip) == 1) {
      mqttClient.setServer(ip, mqttPort);
      if (locked) mqttUnlock();
      return;
    }
  }
  mqttClient.setServer(mqttHost.c_str(), mqttPort);
  if (locked) mqttUnlock();
}

// Constantes - Pinos de SAÃDA (relÃ©s e indicadores)
#ifndef PIN_RELE_SA
#define PIN_RELE_SA 33
#endif
#ifndef PIN_RELE_DA
#define PIN_RELE_DA 25
#endif
#ifndef PIN_RELE_SE
#define PIN_RELE_SE 27
#endif
#ifndef PIN_RELE_DE
#define PIN_RELE_DE 26
#endif
#ifndef PIN_RELE_SP
#define PIN_RELE_SP 12
#endif
#ifndef PIN_RELE_DP
#define PIN_RELE_DP 14
#endif
#ifndef PIN_RELE_REFLETOR
#define PIN_RELE_REFLETOR 4
#endif
#ifndef PIN_RELE_TREND_DESCE
#define PIN_RELE_TREND_DESCE -1
#endif
#ifndef PIN_RELE_TREND_SOBE
#define PIN_RELE_TREND_SOBE -1
#endif
#ifndef PIN_LED
#define PIN_LED 2
#endif
#ifndef PIN_BUZZER
#define PIN_BUZZER 32
#endif
#ifndef PIN_BOOT_BUTTON
#define PIN_BOOT_BUTTON 0
#endif

const int Rele_SA = PIN_RELE_SA;
const int Rele_DA = PIN_RELE_DA;
const int Rele_SE = PIN_RELE_SE;
const int Rele_DE = PIN_RELE_DE;
const int Rele_SP = PIN_RELE_SP;
const int Rele_DP = PIN_RELE_DP;
const int LED = PIN_LED;
const int Rele_refletor = PIN_RELE_REFLETOR;
const int Rele_TREND_DESCE = PIN_RELE_TREND_DESCE;
const int Rele_TREND_SOBE = PIN_RELE_TREND_SOBE;
const int BUZZER = PIN_BUZZER;

static void buzzerTestTick() {
  uint32_t now = millis();
  if (!buzzerTestEnabled) {
    if (buzzerTestIsOn) {
      digitalWrite(BUZZER, LOW);
      buzzerTestIsOn = false;
    }
    return;
  }
  if (!buzzerTestIsOn) {
    if (static_cast<int32_t>(now - buzzerTestNextOnMs) >= 0) {
      digitalWrite(BUZZER, HIGH);
      buzzerTestIsOn = true;
      buzzerTestOffAtMs = now + 80;
      buzzerTestNextOnMs = now + 2000;
    }
    return;
  }
  if (static_cast<int32_t>(now - buzzerTestOffAtMs) >= 0) {
    digitalWrite(BUZZER, LOW);
    buzzerTestIsOn = false;
  }
}

static void buzzerPulseStart2s() {
  buzzerTestEnabled = true;
  buzzerTestIsOn = false;
  buzzerTestNextOnMs = millis();
  buzzerTestOffAtMs = 0;
}

static void buzzerPulseStop() {
  buzzerTestEnabled = false;
  digitalWrite(BUZZER, LOW);
  buzzerTestIsOn = false;
}

static void beepSeqStart(uint8_t count, uint16_t onMs, uint16_t offMs) {
  if (count == 0) return;
  beepSeqRestorePulse = buzzerTestEnabled;
  buzzerPulseStop();
  beepSeqActive = true;
  beepSeqRemaining = count;
  beepSeqBuzzerOn = false;
  beepSeqOnMs = onMs;
  beepSeqOffMs = offMs;
  beepSeqNextMs = millis();
}

static void beepSeqTick() {
  if (!beepSeqActive) return;
  uint32_t now = millis();
  if (static_cast<int32_t>(now - beepSeqNextMs) < 0) return;
  if (!beepSeqBuzzerOn) {
    digitalWrite(BUZZER, HIGH);
    beepSeqBuzzerOn = true;
    beepSeqNextMs = now + beepSeqOnMs;
    return;
  }
  digitalWrite(BUZZER, LOW);
  beepSeqBuzzerOn = false;
  if (beepSeqRemaining > 0) {
    beepSeqRemaining--;
  }
  if (beepSeqRemaining == 0) {
    beepSeqActive = false;
    if (beepSeqRestorePulse) {
      buzzerPulseStart2s();
    }
    return;
  }
  beepSeqNextMs = now + beepSeqOffMs;
}

static void alarmTick() {
  if (!audibleErrorBeeps) return;
  if (beepSeqActive) return;
  if (buzzerTestEnabled) return;
  uint32_t now = millis();
  if (bootStartedAtMs == 0) {
    bootStartedAtMs = now;
  }

  bool wifiConnected = (WiFi.status() == WL_CONNECTED);
  bool mqttConnected = mqttClient.connected();

  if (wifiConnected) {
    if (wifiOkAtMs == 0) wifiOkAtMs = now;
    wifiLostAtMs = 0;
  } else if (wifiOkAtMs != 0) {
    if (wifiLostAtMs == 0) wifiLostAtMs = now;
  }

  if (mqttConnected) {
    if (mqttOkAtMs == 0) mqttOkAtMs = now;
    mqttLostAtMs = 0;
  } else if (mqttOkAtMs != 0) {
    if (mqttLostAtMs == 0) mqttLostAtMs = now;
  }

  if (alarmNextMs != 0 && static_cast<int32_t>(now - alarmNextMs) < 0) {
    return;
  }

  if (!wifiConnected) {
    if ((now - bootStartedAtMs) > 45000 || (wifiLostAtMs != 0 && (now - wifiLostAtMs) > 60000)) {
      beepSeqStart(3, 400, 150);
      alarmNextMs = now + 10000;
    }
    return;
  }

  if (!mqttConnected) {
    if ((wifiOkAtMs != 0 && (now - wifiOkAtMs) > 45000) || (mqttLostAtMs != 0 && (now - mqttLostAtMs) > 60000)) {
      beepSeqStart(3, 400, 150);
      alarmNextMs = now + 10000;
    }
    return;
  }

  if (supabaseOkAtMs == 0 && supabaseFailAtMs != 0 && (now - supabaseFailAtMs) > 60000) {
    beepSeqStart(3, 400, 150);
    alarmNextMs = now + 10000;
  }
}

static const char* relayLabelByPin(int pin) {
  if (pin == Rele_SA) return "RELE_SA";
  if (pin == Rele_DA) return "RELE_DA";
  if (pin == Rele_SE) return "RELE_SE";
  if (pin == Rele_DE) return "RELE_DE";
  if (pin == Rele_SP) return "RELE_SP";
  if (pin == Rele_DP) return "RELE_DP";
  if (pin == Rele_TREND_SOBE) return "RELE_TREND_SOBE";
  if (pin == Rele_TREND_DESCE) return "RELE_TREND_DESCE";
  if (pin == Rele_refletor) return "RELE_REFLETOR";
  if (pin == LED) return "LED";
  if (pin == BUZZER) return "BUZZER";
  return "GPIO";
}

#ifndef RELE_DP_ACTIVE_LOW
#define RELE_DP_ACTIVE_LOW 0
#endif
#ifndef RELE_SE_ACTIVE_LOW
#define RELE_SE_ACTIVE_LOW 0
#endif
#ifndef RELE_DE_ACTIVE_LOW
#define RELE_DE_ACTIVE_LOW 0
#endif

static inline bool relayActiveLowForPin(int pin) {
  if (pin == Rele_DP) return RELE_DP_ACTIVE_LOW;
  if (pin == Rele_SE) return RELE_SE_ACTIVE_LOW;
  if (pin == Rele_DE) return RELE_DE_ACTIVE_LOW;
  return false;
}

static inline int relayLevelForWrite(int pin, bool on) {
  if (relayActiveLowForPin(pin)) {
    return on ? LOW : HIGH;
  }
  return on ? HIGH : LOW;
}

static inline bool relayIsOn(int pin) {
  if (relayActiveLowForPin(pin)) {
    return digitalRead(pin) == LOW;
  }
  return digitalRead(pin) == HIGH;
}

static const int kRelayTrackPins[] = {
  Rele_SA,
  Rele_DA,
  Rele_SE,
  Rele_DE,
  Rele_SP,
  Rele_DP,
  Rele_refletor,
  Rele_TREND_SOBE,
  Rele_TREND_DESCE,
};

static bool relayTrackLastOn[sizeof(kRelayTrackPins) / sizeof(kRelayTrackPins[0])] = {};
static uint32_t relayTrackOnSinceMs[sizeof(kRelayTrackPins) / sizeof(kRelayTrackPins[0])] = {};
static char relayTrackLastSrc[sizeof(kRelayTrackPins) / sizeof(kRelayTrackPins[0])][16] = {};

static int relayTrackIndexForPin(int pin) {
  for (size_t i = 0; i < (sizeof(kRelayTrackPins) / sizeof(kRelayTrackPins[0])); i++) {
    if (kRelayTrackPins[i] == pin) return static_cast<int>(i);
  }
  return -1;
}

static void relayTrackNote(int pin, bool on, const char* src) {
  int idx = relayTrackIndexForPin(pin);
  if (idx < 0) return;
  if (kRelayTrackPins[idx] < 0) return;

  if (src && src[0] != '\0') {
    snprintf(relayTrackLastSrc[idx], sizeof(relayTrackLastSrc[idx]), "%s", src);
  }

  bool isOnNow = relayIsOn(pin);
  if (isOnNow && !relayTrackLastOn[idx]) {
    relayTrackOnSinceMs[idx] = millis();
  } else if (!isOnNow) {
    relayTrackOnSinceMs[idx] = 0;
  }
  relayTrackLastOn[idx] = isOnNow;
}

static void relayTrackInitSnapshot() {
  for (size_t i = 0; i < (sizeof(kRelayTrackPins) / sizeof(kRelayTrackPins[0])); i++) {
    int pin = kRelayTrackPins[i];
    if (pin < 0) {
      relayTrackLastOn[i] = false;
      relayTrackOnSinceMs[i] = 0;
      relayTrackLastSrc[i][0] = '\0';
      continue;
    }
    bool onNow = relayIsOn(pin);
    relayTrackLastOn[i] = onNow;
    relayTrackOnSinceMs[i] = onNow ? millis() : 0;
    relayTrackLastSrc[i][0] = '\0';
  }
}

static void relayTrackPrintOnRelays(const char* tag) {
  uint32_t now = millis();
  Serial.print("[RELAY] ");
  if (tag && tag[0] != '\0') {
    Serial.print(tag);
    Serial.print(" ");
  }
  bool any = false;
  for (size_t i = 0; i < (sizeof(kRelayTrackPins) / sizeof(kRelayTrackPins[0])); i++) {
    int pin = kRelayTrackPins[i];
    if (pin < 0) continue;
    bool onNow = relayIsOn(pin);
    if (!onNow) continue;
    uint32_t since = relayTrackOnSinceMs[i];
    uint32_t dur = (since != 0 && now >= since) ? (now - since) : 0;
    Serial.print(any ? " | " : "");
    any = true;
    Serial.print(relayLabelByPin(pin));
    Serial.print("(GPIO");
    Serial.print(pin);
    Serial.print(")");
    Serial.print(" RB=");
    Serial.print(digitalRead(pin) == HIGH ? "1" : "0");
    Serial.print(" ON=1");
    Serial.print(" T=");
    Serial.print(dur);
    Serial.print(" SRC=");
    Serial.print(relayTrackLastSrc[i][0] ? relayTrackLastSrc[i] : "?");
  }
  if (!any) {
    Serial.print("NONE");
  }
  Serial.println();
}

static void setOutputPin(int pin, bool on, const char* src, bool log = true) {
  int newValue = relayLevelForWrite(pin, on);
  int oldValue = digitalRead(pin);
  if (oldValue == newValue) {
    relayTrackNote(pin, on, src);
    return;
  }
  digitalWrite(pin, newValue);
  int readBack = digitalRead(pin);
  relayTrackNote(pin, on, src);
  if (pin == Rele_SA || pin == Rele_DA || pin == Rele_SE || pin == Rele_DE || pin == Rele_SP || pin == Rele_DP || pin == Rele_refletor || pin == Rele_TREND_SOBE || pin == Rele_TREND_DESCE) {
    mqttStatusDirty = true;
  }
  if (!log) {
    return;
  }
  Serial.print("[GPIO_OUT] ");
  Serial.print(relayLabelByPin(pin));
  Serial.print("(GPIO");
  Serial.print(pin);
  Serial.print(")=");
  Serial.print(on ? "1" : "0");
  Serial.print(" RB=");
  Serial.print(readBack == HIGH ? "1" : "0");
  Serial.print(" ON=");
  Serial.print(relayIsOn(pin) ? "1" : "0");
  if (relayActiveLowForPin(pin)) {
    Serial.print(" ALOW=1");
  }
  if (src && src[0] != '\0') {
    Serial.print(" ");
    Serial.print(src);
  }
  Serial.println();
}

#if defined(I2C_SDA) && defined(I2C_SCL)
  #if (I2C_SDA == PIN_RELE_SA) || (I2C_SCL == PIN_RELE_SA)
    #error "Conflito: I2C_SDA/I2C_SCL nao pode ser igual a PIN_RELE_SA"
  #endif
  #if (I2C_SDA == PIN_RELE_DA) || (I2C_SCL == PIN_RELE_DA)
    #error "Conflito: I2C_SDA/I2C_SCL nao pode ser igual a PIN_RELE_DA"
  #endif
  #if (I2C_SDA == PIN_RELE_SE) || (I2C_SCL == PIN_RELE_SE)
    #error "Conflito: I2C_SDA/I2C_SCL nao pode ser igual a PIN_RELE_SE"
  #endif
  #if (I2C_SDA == PIN_RELE_DE) || (I2C_SCL == PIN_RELE_DE)
    #error "Conflito: I2C_SDA/I2C_SCL nao pode ser igual a PIN_RELE_DE"
  #endif
  #if (I2C_SDA == PIN_RELE_SP) || (I2C_SCL == PIN_RELE_SP)
    #error "Conflito: I2C_SDA/I2C_SCL nao pode ser igual a PIN_RELE_SP"
  #endif
  #if (I2C_SDA == PIN_RELE_DP) || (I2C_SCL == PIN_RELE_DP)
    #error "Conflito: I2C_SDA/I2C_SCL nao pode ser igual a PIN_RELE_DP"
  #endif
  #if (I2C_SDA == PIN_RELE_REFLETOR) || (I2C_SCL == PIN_RELE_REFLETOR)
    #error "Conflito: I2C_SDA/I2C_SCL nao pode ser igual a PIN_RELE_REFLETOR"
  #endif
  #if (I2C_SDA == PIN_RELE_TREND_DESCE) || (I2C_SCL == PIN_RELE_TREND_DESCE)
    #error "Conflito: I2C_SDA/I2C_SCL nao pode ser igual a PIN_RELE_TREND_DESCE"
  #endif
  #if (I2C_SDA == PIN_RELE_TREND_SOBE) || (I2C_SCL == PIN_RELE_TREND_SOBE)
    #error "Conflito: I2C_SDA/I2C_SCL nao pode ser igual a PIN_RELE_TREND_SOBE"
  #endif
#endif

static int botaoResetWifiPin = RF;
static bool rfHabilitado = true;
static bool ignoreButtonsUntilRelease = false;

// VariÃ¡veis de estado dos botÃµes
int buttonState = 0;
int buttonState1 = 0;
int buttonState2 = 0;
int buttonState3 = 0;
int buttonState4 = 0;
int buttonState5 = 0;
int buttonState6 = 0;
int buttonState7 = 0;
int buttonState8 = 0;
int buttonState9 = 0;
int buttonState10 = 0;
int buttonState11 = 0;
int buttonState12 = 0;

unsigned long previousMillis = 0;
unsigned long currentMillis;
unsigned long interval = 500;

int ledState = 0;
int cont = 0;
int contador = 0;
int contador1 = 0;
int contador2 = 0;
int cont15 = 0;
int contador21 = 0;

// VariÃ¡veis do encoder virtual
int incoder_virtual_encosto_M1 = 0;
int incoder_virtual_asento_M1 = 0;
int incoder_virtual_perneira_M1 = 0;
int cont_bt_m1 = 0;
int incoder_virtual_encosto_service = 0;
int incoder_virtual_asento_service = 0;
int incoder_virtual_perneira_service = 0;
int cont12 = 0;

// Limites do encoder virtual
int fim_encosto_encoder = 0;
int fim_asento_encoder = 0;
int fim_perneira_encoder = 0;
static bool calibrationInProgress = false;

int cont13 = 0;

bool estado_r = 0;
bool estado_sp = 0;
bool estado_dp = 0;
bool estado_sa = 0;
bool estado_da = 0;
bool estado_se = 0;
bool estado_de = 0;
bool estado_trend_sobe = 0;
bool estado_trend_desce = 0;
bool faz_bt_seg = 0;

static bool reflectorMoveActive = false;
static bool reflectorWasOnAtMoveStart = false;
static uint8_t reflectorMoveKind = 0;

volatile uint32_t pulses_encosto = 0;
volatile uint32_t pulses_assento = 0;
volatile uint32_t pulses_perneira = 0;
volatile uint32_t pulses_trend = 0;
static uint32_t last_pulses_encosto = 0;
static uint32_t last_pulses_assento = 0;
static uint32_t last_pulses_perneira = 0;
static uint32_t last_pulses_trend = 0;
static uint32_t trendLastPulseAtMs = 0;
static uint32_t last_pulses_trend_travel = 0;

static uint64_t motorTravelPulsesEncosto = 0;
static uint64_t motorTravelPulsesAssento = 0;
static uint64_t motorTravelPulsesPerneira = 0;
static uint64_t motorTravelPulsesTrend = 0;
static uint64_t motorTravelUnsavedEncosto = 0;
static uint64_t motorTravelUnsavedAssento = 0;
static uint64_t motorTravelUnsavedPerneira = 0;
static uint64_t motorTravelUnsavedTrend = 0;
static float mmPerPulseEncosto = 0.077f;
static float mmPerPulseAssento = 0.077f;
static float mmPerPulsePerneira = 0.077f;
static float mmPerPulseTrend = 0.077f;
static float maintenanceLimitKm = 5.0f; // Limite para manutencao em Quilometros (Ex: 5km)
static bool maintenanceRequired = false;
static uint32_t motorTravelNextSaveMs = 0;
static uint32_t motorTravelNextSendMs = 0;
static const uint32_t MOTOR_TRAVEL_SAVE_INTERVAL_MS = 10000;
static const uint32_t MOTOR_TRAVEL_SEND_INTERVAL_MS = 900000;
static bool encoderPulseDebug = false;
static bool pulsePrintEnabled = false;
static uint32_t pulsePrintLastTrend = 0;
static uint32_t pulsePrintLastEncosto = 0;
static uint32_t pulsePrintLastAssento = 0;
static uint32_t pulsePrintLastPerneira = 0;
static bool encoderMonEnabled = false;
static uint32_t encoderMonNextMs = 0;
static uint32_t encoderMonLastEnc = 0;
static uint32_t encoderMonLastAss = 0;
static uint32_t encoderMonLastPer = 0;
static uint32_t encoderMonLastTrend = 0;
static uint8_t encRequiredMask = 0;

static inline bool encReqEncosto() { return (encRequiredMask & 0x01) != 0; }
static inline bool encReqAssento() { return (encRequiredMask & 0x02) != 0; }
static inline bool encReqPerneira() { return (encRequiredMask & 0x04) != 0; }
static inline bool encReqTrend() { return (encRequiredMask & 0x08) != 0; }

static int relayPinByToken(String token) {
  token.trim();
  token.toUpperCase();

  if (token == "SA" || token == "RELE_SA") return Rele_SA;
  if (token == "DA" || token == "RELE_DA") return Rele_DA;
  if (token == "SE" || token == "RELE_SE") return Rele_SE;
  if (token == "DE" || token == "RELE_DE") return Rele_DE;
  if (token == "SP" || token == "RELE_SP") return Rele_SP;
  if (token == "DP" || token == "RELE_DP") return Rele_DP;

  if (token == "TS" || token == "TREND_SOBE" || token == "RELE_TREND_SOBE") return Rele_TREND_SOBE;
  if (token == "TD" || token == "TREND_DESCE" || token == "RELE_TREND_DESCE") return Rele_TREND_DESCE;

  if (token == "RF" || token == "REFLETOR" || token == "RELE_REFLETOR") return Rele_refletor;
  if (token == "LED") return LED;
  if (token == "BUZZER" || token == "BZ") return BUZZER;

  return -1;
}

static inline void encoderMonResetCounters() {
  encoderMonLastEnc = pulses_encosto;
  encoderMonLastAss = pulses_assento;
  encoderMonLastPer = pulses_perneira;
  encoderMonLastTrend = pulses_trend;
}

static void encoderMonTick() {
  if (!encoderMonEnabled) return;
  uint32_t now = millis();
  if (static_cast<int32_t>(now - encoderMonNextMs) < 0) return;
  encoderMonNextMs = now + 500;
  uint32_t pe = pulses_encosto;
  uint32_t pa = pulses_assento;
  uint32_t pp = pulses_perneira;
  uint32_t pt = pulses_trend;
  uint32_t de = pe - encoderMonLastEnc;
  uint32_t da = pa - encoderMonLastAss;
  uint32_t dp = pp - encoderMonLastPer;
  uint32_t dt = pt - encoderMonLastTrend;
  encoderMonLastEnc = pe;
  encoderMonLastAss = pa;
  encoderMonLastPer = pp;
  encoderMonLastTrend = pt;

  Serial.print("[ENC_MON] ENC(GPIO"); Serial.print(ENCODER3); Serial.print(") L="); Serial.print(ENCODER3 >= 0 ? (digitalRead(ENCODER3) == HIGH ? "1" : "0") : "NA");
  Serial.print(" P="); Serial.print(pe); Serial.print(" d="); Serial.print(de);
  Serial.print(" | ASS(GPIO"); Serial.print(ENCODER1); Serial.print(") L="); Serial.print(ENCODER1 >= 0 ? (digitalRead(ENCODER1) == HIGH ? "1" : "0") : "NA");
  Serial.print(" P="); Serial.print(pa); Serial.print(" d="); Serial.print(da);
  Serial.print(" | PER(GPIO"); Serial.print(ENCODER2); Serial.print(") L="); Serial.print(ENCODER2 >= 0 ? (digitalRead(ENCODER2) == HIGH ? "1" : "0") : "NA");
  Serial.print(" P="); Serial.print(pp); Serial.print(" d="); Serial.print(dp);
  Serial.print(" | TREND(GPIO"); Serial.print(ENCODER_TREND); Serial.print(") L="); Serial.print(ENCODER_TREND >= 0 ? (digitalRead(ENCODER_TREND) == HIGH ? "1" : "0") : "NA");
  Serial.print(" P="); Serial.print(pt); Serial.print(" d="); Serial.println(dt);
}

// Mapeamento solicitado:
// ENCODER1 -> Assento
// ENCODER2 -> Perneira
// ENCODER3 -> Encosto
static volatile int8_t encoder1IdleLevel = -1;
static volatile int8_t encoder2IdleLevel = -1;
static volatile int8_t encoder3IdleLevel = -1;
static volatile int8_t encoderTrendIdleLevel = -1;

static void IRAM_ATTR isr_encoder1() {
  if (encoder1IdleLevel >= 0 && gpio_get_level(static_cast<gpio_num_t>(ENCODER1)) != encoder1IdleLevel) {
    pulses_assento++;
  }
}

static void IRAM_ATTR isr_encoder2() {
  if (encoder2IdleLevel >= 0 && gpio_get_level(static_cast<gpio_num_t>(ENCODER2)) != encoder2IdleLevel) {
    pulses_perneira++;
  }
}

static void IRAM_ATTR isr_encoder3() {
  if (encoder3IdleLevel >= 0 && gpio_get_level(static_cast<gpio_num_t>(ENCODER3)) != encoder3IdleLevel) {
    pulses_encosto++;
  }
}

static void IRAM_ATTR isr_encoder_trend() {
  if (encoderTrendIdleLevel >= 0 && gpio_get_level(static_cast<gpio_num_t>(ENCODER_TREND)) != encoderTrendIdleLevel) {
    pulses_trend++;
  }
}

static bool gpioIsrServiceReady = false;
static void IRAM_ATTR isr_encoder2_idf(void *arg) {
  (void)arg;
  if (encoder2IdleLevel >= 0 && gpio_get_level(static_cast<gpio_num_t>(ENCODER2)) != encoder2IdleLevel) {
    pulses_perneira++;
  }
}

static inline void ensureGpioIsrService() {
  if (gpioIsrServiceReady) return;
  esp_err_t err = gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
  if (err == ESP_OK || err == ESP_ERR_INVALID_STATE) {
    gpioIsrServiceReady = true;
  }
}

// Debounce para comandos BLE
unsigned long ultimoComandoBLE = 0;
String ultimoCmdBLE = "";
unsigned long ultimoComandoRemoto = 0;

// Timeout para motores (dead man's switch) - desliga se não receber comando
const unsigned long MOTOR_TIMEOUT = 1000; // 1 segundo
unsigned long ultimoComandoSE = 0;
unsigned long ultimoComandoDE = 0;
unsigned long ultimoComandoSA = 0;
unsigned long ultimoComandoDA = 0;
unsigned long ultimoComandoSP = 0;
unsigned long ultimoComandoDP = 0;
unsigned long ultimoComandoTrendSobe = 0;
unsigned long ultimoComandoTrendDesce = 0;
const unsigned long TREND_WATCHDOG_MS = 2500;
unsigned long lastTrendCmdMs = 0;
bool trendRemoteActive = false;
const uint32_t VZ_MAX_TIME_MS = 40000;
const uint32_t VZ_NO_PULSE_FINISH_MS = 700;
const uint32_t VZ_NO_PULSE_START_ABORT_MS = 2000;
const uint32_t M1_HOLD_SAVE_MS = 3000;
const int LIMIT_STOP_MARGIN_PULSES = 30;
const uint32_t M1_STALL_TIMEOUT_MS = 1200;

static int perneiraMaxOutForLocks() {
  int m = fim_perneira_encoder;
  if (!calibrationInProgress && !ignoreLimitLocks) {
    if (m > LIMIT_STOP_MARGIN_PULSES) m -= LIMIT_STOP_MARGIN_PULSES;
  }
  if (m < 0) m = 0;
  return m;
}

static int encostoMaxOutForLocks() {
  int m = fim_encosto_encoder;
  if (!calibrationInProgress && !ignoreLimitLocks) {
    if (m > LIMIT_STOP_MARGIN_PULSES) m -= LIMIT_STOP_MARGIN_PULSES;
  }
  if (m < 0) m = 0;
  return m;
}

static int assentoMaxOutForLocks() {
  int m = fim_asento_encoder;
  if (!calibrationInProgress && !ignoreLimitLocks) {
    if (m > LIMIT_STOP_MARGIN_PULSES) m -= LIMIT_STOP_MARGIN_PULSES;
  }
  if (m < 0) m = 0;
  return m;
}

static inline int effectiveEncostoMax() { return encostoMaxOutForLocks(); }
static inline int effectiveAssentoMax() { return assentoMaxOutForLocks(); }
static inline int effectivePerneiraMax() { return perneiraMaxOutForLocks(); }

static bool perneiraIsAtPt() {
  int m = perneiraMaxOutForLocks();
  if (m <= 0) {
    return true;
  }
  return incoder_virtual_perneira_service >= m;
}

// Flags para controle de fim de curso do encoder
bool en_SA_acabou = false;
bool en_DA_acabou = false;
bool en_SE_acabou = false;
bool en_DE_acabou = false;
bool en_SP_acabou = false;
bool en_DP_acabou = false;

bool faz_m1 = false;

// Travas de botÃµes baseadas no encoder virtual
bool trava_bt_DA = true;
bool trava_bt_SA = false;
bool trava_bt_SE = true;
bool trava_bt_DE = false;
bool trava_bt_DP = true;
bool trava_bt_SP = false;

unsigned long currentMillis20 = 0;
unsigned long previousMillis_amostra20 = 0;
unsigned long ultimoMillisEncoder = 0;
unsigned long ultimoMillisPiscaLed = 0;
bool estadoLedPisca = false;
const unsigned long INTERVALO_ENCODER = 250;
bool vzInicialEmAndamento = false; // Flag para bloquear loop() durante VZ inicial

void monitoraSistema();

// ========== SETUP ==========
void setup() {
  setupBrownoutDetector();
  bootStartedAtMs = millis();
  // WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // Desabilita detector de Brownout - not needed in Arduino
  setCpuFrequencyMhz(80); // Reduz frequÃªncia para 80MHz durante inicializaÃ§Ã£o
  
  Serial.begin(115200);
  delay(2000);

#if ESP_IDF_VERSION_MAJOR >= 5
  esp_task_wdt_deinit();
  esp_task_wdt_config_t twdt_config = {
      .timeout_ms = 120000,
      .idle_core_mask = (1 << 0) | (1 << 1),
      .trigger_panic = true,
  };
  esp_task_wdt_init(&twdt_config);
#else
  esp_task_wdt_deinit();
  esp_task_wdt_init(120, true);
#endif

  checkBootHoldResetTudo();

#if I2C_EARLY_TEST
  #if defined(I2C_SDA) && defined(I2C_SCL)
  pinMode(I2C_SDA, INPUT_PULLUP);
  pinMode(I2C_SCL, INPUT_PULLUP);
  Wire.begin(I2C_SDA, I2C_SCL);
  #elif defined(PCF8575_SDA) && defined(PCF8575_SCL)
  pinMode(PCF8575_SDA, INPUT_PULLUP);
  pinMode(PCF8575_SCL, INPUT_PULLUP);
  Wire.begin(PCF8575_SDA, PCF8575_SCL);
  #else
  Wire.begin();
  #endif
  Wire.setClock(I2C_CLOCK_HZ);
  Serial.println("\n=== I2C EARLY TEST MODE ===");
  Serial.print("[I2C] Clock: ");
  Serial.println(I2C_CLOCK_HZ);
  #if defined(I2C_SDA) && defined(I2C_SCL)
  Serial.print("[I2C] SDA=");
  Serial.print(I2C_SDA);
  Serial.print(" SCL=");
  Serial.println(I2C_SCL);
  Serial.print("[I2C] SDA level=");
  Serial.print(digitalRead(I2C_SDA));
  Serial.print(" SCL level=");
  Serial.println(digitalRead(I2C_SCL));
  #endif
  Serial.println("Digite: I2C_SCAN, PCF8574_PING, PCF8574_TEST");
  Serial.print("[I2C] Scan inicial: ");
  Serial.println(i2cScanSummary());
  while (true) {
    processaComandosSerialDebug();
    delay(10);
  }
#endif
  
  Serial.println("\n--- INICIALIZANDO SISTEMA ---");
  Serial.print("CPU0 Reset Reason: "); Serial.println(esp_reset_reason());
  Serial.print("CPU1 Reset Reason: "); Serial.println(esp_reset_reason());
  
  // Gera nÃºmero de sÃ©rie Ãºnico baseado no MAC ID do ESP32
  Serial.println("Gerando numero de serie...");
  NUMERO_SERIE_CADEIRA = geraNumeroSerieDoMAC();
  Serial.print("Numero de Serie: ");
  Serial.println(NUMERO_SERIE_CADEIRA);
  
  // Define base dos tópicos MQTT
  MQTT_TOPIC_BASE = NUMERO_SERIE_CADEIRA + "/";
  #if !OFFLINE_DEMO
  loadMqttPreferences();
  applyMqttRuntimeConfig();
  #endif
  
  // Gera nome do dispositivo (últimos 4 dígitos do MAC)
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  char nomeTemp[20];
  snprintf(nomeTemp, sizeof(nomeTemp), "CadeiraOdonto-%02X%02X", mac[4], mac[5]);
  NOME_DISPOSITIVO = String(nomeTemp);
  Serial.print("Nome do Dispositivo: ");
  Serial.println(NOME_DISPOSITIVO);
  delay(500);

  IO_begin();
  
  // ConfiguraÃ§Ã£o de pinos primeiro (menor consumo)
  Serial.println("Configurando pinos GPIO...");

  // Inicializa pinos de entrada com pull-up
  loadGavetaPinPreference();
  loadCalibracaoLimites();
  {
    Preferences p;
    if (p.begin("cadeira", true)) {
      bool done = p.getBool("cal_done", false);
      ignoreLimitLocks = !done;
      ignoreGavetaLock = !done;
      p.end();
    }
  }
  pinMode(DE, INPUT_PULLUP);
  pinMode(SA, INPUT_PULLUP);
  pinMode(DA, INPUT_PULLUP);
  pinMode(SE, INPUT_PULLUP);
  pinMode(RF, INPUT_PULLUP);
  pinMode(VZ, INPUT_PULLUP);
  pinMode(PT, INPUT_PULLUP);
  pinMode(SP, INPUT_PULLUP);
  pinMode(DP, INPUT_PULLUP);
  pinMode(M1, INPUT_PULLUP);
  if (TREN_INT_DESCE >= 0) {
    pinMode(TREN_INT_DESCE, TREND_INPUT_MODE);
    Serial.print("[TREND] TREN_INT_DESCE(GPIO");
    Serial.print(TREN_INT_DESCE);
    Serial.print(") INIT=");
    Serial.println(digitalRead(TREN_INT_DESCE) == HIGH ? 1 : 0);
  }
  if (INT_TREND_DESCE >= 0) {
    pinMode(INT_TREND_DESCE, TREND_INPUT_MODE);
    Serial.print("[TREND] INT_TREND_DESCE(GPIO");
    Serial.print(INT_TREND_DESCE);
    Serial.print(") INIT=");
    Serial.println(digitalRead(INT_TREND_DESCE) == HIGH ? 1 : 0);
  }
  if (TREN_INT_SOBE >= 0) {
    pinMode(TREN_INT_SOBE, TREND_INPUT_MODE);
    Serial.print("[TREND] TREN_INT_SOBE(GPIO");
    Serial.print(TREN_INT_SOBE);
    Serial.print(") INIT=");
    Serial.println(digitalRead(TREN_INT_SOBE) == HIGH ? 1 : 0);
  }
  if (INT_TREND_SOBE >= 0) {
    pinMode(INT_TREND_SOBE, TREND_INPUT_MODE);
    Serial.print("[TREND] INT_TREND_SOBE(GPIO");
    Serial.print(INT_TREND_SOBE);
    Serial.print(") INIT=");
    Serial.println(digitalRead(INT_TREND_SOBE) == HIGH ? 1 : 0);
  }
  if (GAVETA >= 0) {
    pinMode(GAVETA, INPUT_PULLUP);
    Serial.print("[GAVETA] GPIO=");
    Serial.println(GAVETA);
    gavetaIdleLevel = digitalRead(GAVETA);
  } else {
    Serial.println("[GAVETA] DESABILITADO");
    gavetaIdleLevel = -1;
  }
  pinMode(ENCODER1, INPUT_PULLUP);
  if (ENCODER2 >= 0) gpio_reset_pin(static_cast<gpio_num_t>(ENCODER2));
  pinMode(ENCODER2, INPUT_PULLUP);
  pinMode(ENCODER3, INPUT_PULLUP);
  if (ENCODER_TREND >= 0) {
    pinMode(ENCODER_TREND, INPUT_PULLUP);
  }

  if (ENCODER1 >= 0) encoder1IdleLevel = static_cast<int8_t>(gpio_get_level(static_cast<gpio_num_t>(ENCODER1)));
  if (ENCODER2 >= 0) encoder2IdleLevel = static_cast<int8_t>(gpio_get_level(static_cast<gpio_num_t>(ENCODER2)));
  if (ENCODER3 >= 0) encoder3IdleLevel = static_cast<int8_t>(gpio_get_level(static_cast<gpio_num_t>(ENCODER3)));
  if (ENCODER_TREND >= 0) encoderTrendIdleLevel = static_cast<int8_t>(gpio_get_level(static_cast<gpio_num_t>(ENCODER_TREND)));
  encRequiredMask = 0;
  if (ENCODER3 >= 0) encRequiredMask |= 0x01;
  if (ENCODER1 >= 0) encRequiredMask |= 0x02;
  if (ENCODER2 >= 0) encRequiredMask |= 0x04;
  if (ENCODER_TREND >= 0) encRequiredMask |= 0x08;

  Serial.println("\n[DIAG_ENC] Verificando niveis logicos iniciais:");
  Serial.print("[DIAG_ENC] ENC1(GPIO"); Serial.print(ENCODER1); Serial.print(") L="); Serial.println(digitalRead(ENCODER1));
  Serial.print("[DIAG_ENC] ENC2(GPIO"); Serial.print(ENCODER2); Serial.print(") L="); Serial.println(digitalRead(ENCODER2));
  Serial.print("[DIAG_ENC] ENC3(GPIO"); Serial.print(ENCODER3); Serial.print(") L="); Serial.println(digitalRead(ENCODER3));
  if (ENCODER_TREND >= 0) {
    Serial.print("[DIAG_ENC] TREND(GPIO"); Serial.print(ENCODER_TREND); Serial.print(") L="); Serial.println(digitalRead(ENCODER_TREND));
  }

  if (TEST_MODE) {
    Serial.println("[ENC] TEST_MODE=1: interrupcoes de encoder desabilitadas");
  } else {
    if (ENCODER1 >= 0 && ENCODER1 != I2C_SDA && ENCODER1 != I2C_SCL && ENCODER1 != Rele_refletor) {
      pinMode(ENCODER1, INPUT_PULLUP);
      attachInterrupt(digitalPinToInterrupt(ENCODER1), isr_encoder1, CHANGE);
    } else {
      Serial.println("[ENC] ENCODER1 desabilitado");
    }
    if (ENCODER2 >= 0 && ENCODER2 != I2C_SDA && ENCODER2 != I2C_SCL && ENCODER2 != Rele_refletor) {
      if (ENCODER2 >= 39) {
        ensureGpioIsrService();
        gpio_set_direction(static_cast<gpio_num_t>(ENCODER2), GPIO_MODE_INPUT);
        gpio_set_pull_mode(static_cast<gpio_num_t>(ENCODER2), GPIO_PULLUP_ONLY);
        gpio_set_intr_type(static_cast<gpio_num_t>(ENCODER2), GPIO_INTR_ANYEDGE);
        gpio_isr_handler_add(static_cast<gpio_num_t>(ENCODER2), isr_encoder2_idf, nullptr);
        gpio_intr_enable(static_cast<gpio_num_t>(ENCODER2));
      } else {
        pinMode(ENCODER2, INPUT_PULLUP);
        attachInterrupt(digitalPinToInterrupt(ENCODER2), isr_encoder2, CHANGE);
      }
    } else {
      Serial.println("[ENC] ENCODER2 desabilitado");
    }
    if (ENCODER3 >= 0 && ENCODER3 != I2C_SDA && ENCODER3 != I2C_SCL && ENCODER3 != Rele_refletor) {
      pinMode(ENCODER3, INPUT_PULLUP);
      attachInterrupt(digitalPinToInterrupt(ENCODER3), isr_encoder3, CHANGE);
    } else {
      Serial.println("[ENC] ENCODER3 desabilitado");
    }
    if (ENCODER_TREND >= 0 && ENCODER_TREND != I2C_SDA && ENCODER_TREND != I2C_SCL && ENCODER_TREND != Rele_refletor) {
      pinMode(ENCODER_TREND, INPUT_PULLUP);
      attachInterrupt(digitalPinToInterrupt(ENCODER_TREND), isr_encoder_trend, CHANGE);
    } else {
      Serial.println("[ENC] ENCODER_TREND desabilitado");
    }
  }

  // Inicializa pinos de saÃ­da
  pinMode(Rele_SA, OUTPUT);
  pinMode(Rele_DA, OUTPUT);
  pinMode(Rele_SE, OUTPUT);
  pinMode(Rele_DE, OUTPUT);
  pinMode(Rele_DP, OUTPUT);
  pinMode(Rele_SP, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, LOW);
  pinMode(LED, OUTPUT);
  pinMode(Rele_refletor, OUTPUT);
  if (Rele_TREND_DESCE >= 0) pinMode(Rele_TREND_DESCE, OUTPUT);
  if (Rele_TREND_SOBE >= 0) pinMode(Rele_TREND_SOBE, OUTPUT);

  // Desliga todos os relÃ©s no inÃ­cio
  setOutputPin(Rele_SA, false, nullptr, false);
  setOutputPin(Rele_DA, false, nullptr, false);
  setOutputPin(Rele_SE, false, nullptr, false);
  setOutputPin(Rele_DE, false, nullptr, false);
  setOutputPin(Rele_DP, false, nullptr, false);
  setOutputPin(Rele_SP, false, nullptr, false);
  setOutputPin(Rele_refletor, false, nullptr, false);
  if (Rele_TREND_DESCE >= 0) setOutputPin(Rele_TREND_DESCE, false, nullptr, false);
  if (Rele_TREND_SOBE >= 0) setOutputPin(Rele_TREND_SOBE, false, nullptr, false);
  relayTrackInitSnapshot();
  Serial.println("Pinos configurados.");
  delay(500);

  if (RF == Rele_SA || RF == Rele_DA || RF == Rele_SE || RF == Rele_DE || RF == Rele_SP || RF == Rele_DP || RF == Rele_refletor || RF == BUZZER || RF == LED) {
    rfHabilitado = false;
    botaoResetWifiPin = -1;
    Serial.println("[ERRO] RF conflita com um pino de saida. RF desabilitado.");
  } else {
    rfHabilitado = true;
    botaoResetWifiPin = RF;
  }

  inicializaEstadosDeBordaBotoes();
  loadMotorTravelPreferences();

#if PORTS_ONLY
  Serial.println("\n=== PORTS ONLY MODE ===");
  Serial.println("Comandos: I2C_SCAN | PCF8574_PING | PCF8574_TEST | PCF8574_BTNS | GPIO_BTNS | PCF8574_INT");
  bip();
  return;
#endif

#if PORTS_VERIFY
  Serial.println("\n=== PORTS VERIFY MODE ===");
  Serial.println("Teste completo: botoes (PCF8574) -> rotinas -> reles (GPIO)");
  Serial.println("Use: PCF8574_BTNS | PCF8574_INT | GPIO_BTNS");
  bip();
  return;
#endif

  Serial.println("Inicio programa cadeira GO - Com WiFiManager + Supabase");

  // Carrega dados salvos (encoder virtual e horÃ­metro)
  Serial.println("Carregando preferencias...");
  carregaPreferencias();
  #if !OFFLINE_DEMO
  otaLoadPreferences();
  #endif
  delay(500);

  setCpuFrequencyMhz(240);
  #if OFFLINE_DEMO
  mqttConnectEnabled = false;
  mqttReadyAtMs = 0;
  otaEnabled = false;
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_OFF);
  Serial.println("[OFFLINE_DEMO] WiFi/MQTT/NTP/Supabase/OTA desabilitados");
  #else
  Serial.println("CPU em 240MHz - Iniciando WiFi...");
  delay(1000);

  configuraWiFiManager();

  if (WiFi.status() == WL_CONNECTED) {
    mqttConnectEnabled = false;
    mqttReadyAtMs = 0;
    Serial.println("\n====================================");
    Serial.println("  INICIALIZANDO MQTT");
    Serial.println("====================================");
    applyMqttRuntimeConfig();
    mqttClient.setCallback(mqttCallback);
    Serial.print("Servidor MQTT: ");
    Serial.println(mqttHost);
    Serial.print("Porta: ");
    Serial.println(mqttPort);
    Serial.print("TLS: ");
    Serial.println(mqttUseTls ? "SIM" : "NAO");
    Serial.print("Usuario: ");
    Serial.println(mqttUser.length() > 0 ? mqttUser : "(vazio)");
    Serial.print("Base dos tópicos: ");
    Serial.println(MQTT_TOPIC_BASE);
    Serial.println("====================================\n");
  }

  if (!mqttMutex) {
    mqttMutex = xSemaphoreCreateRecursiveMutex();
  }
  if (!mqttCommandQueue) {
    mqttCommandQueue = xQueueCreate(10, sizeof(MqttCmdItem));
  }
  if (!mqttTxQueue) {
    mqttTxQueue = xQueueCreate(8, sizeof(MqttTxItem));
  }
  if (!mqttTaskHandle) {
    xTaskCreatePinnedToCore(mqttTaskMain, "mqtt", 24576, NULL, 1, &mqttTaskHandle, 1);
  }
  otaValidateAfterBoot();

  Serial.println("Aguardando estabilizacao da rede...");
  delay(2000);

  if (WiFi.status() == WL_CONNECTED) {
    sincronizaNTP();
  }

  if (WiFi.status() == WL_CONNECTED) {
    uint32_t waitUntil = millis() + 12000;
    while (!mqttClient.connected() && static_cast<int32_t>(millis() - waitUntil) < 0) {
      reconnectMQTT();
      delay(200);
    }
  }

  Serial.println("\n====================================");
  Serial.println("  INICIANDO CONEXAO COM SUPABASE");
  Serial.println("====================================");
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi Status: CONECTADO | IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("Heap livre antes: ");
    Serial.print(ESP.getFreeHeap());
    Serial.println(" bytes");
    
    verificaStatusCadeira();
    
    Serial.print("Heap livre depois: ");
    Serial.print(ESP.getFreeHeap());
    Serial.println(" bytes");
    Serial.println("[OK] Verificacao de status concluida.");
  } else {
    Serial.println("WiFi Status: DESCONECTADO");
    Serial.println("[INFO] Pulando verificacao inicial - modo offline.");
  }

  if (WiFi.status() == WL_CONNECTED) {
    mqttConnectEnabled = true;
    mqttReadyAtMs = millis() + 1000;
  }
  #endif

  bool precisaProgramarPercurso = true;
  {
    Preferences p;
    if (p.begin("cadeira", true)) {
      precisaProgramarPercurso = !p.getBool("cal_done", false);
      p.end();
    }
  }

  if (precisaProgramarPercurso) {
    Serial.println("\n====================================");
    Serial.println("  PROGRAMANDO PERCURSOS (1a VEZ)");
    Serial.println("  SEQUENCIA: VZ -> PT -> SALVAR");
    Serial.println("====================================");
    executa_vz_ini();
    cont13 = 1;
    contador = 0;
    executa_pt();

    Preferences p;
    if (p.begin("cadeira", false)) {
      p.putInt("encosto_max", incoder_virtual_encosto_service);
      p.putInt("assento_max", incoder_virtual_asento_service);
      p.putInt("perneira_max", incoder_virtual_perneira_service);
      p.putBool("cal_done", true);
      p.end();
    }
    fim_encosto_encoder = incoder_virtual_encosto_service;
    fim_asento_encoder = incoder_virtual_asento_service;
    fim_perneira_encoder = incoder_virtual_perneira_service;
    ignoreLimitLocks = false;
    ignoreGavetaLock = false;
    saveMotorTravelPreferences(true);

    Serial.print("[OK] Percursos programados e salvos ENC=");
    Serial.print(fim_encosto_encoder);
    Serial.print(" ASS=");
    Serial.print(fim_asento_encoder);
    Serial.print(" PER=");
    Serial.println(fim_perneira_encoder);
    Serial.println("====================================\n");
  } else {
    Serial.println("\n[INFO] Percursos ja programados (cal_done=1). Pulando VZ/PT automatico.");
  }
  
  Serial.println("\n====================================");
  Serial.println("       SISTEMA PRONTO!");
  Serial.println("====================================");
  Serial.print("Nome: ");
  Serial.println(NOME_DISPOSITIVO);
  Serial.print("Serial: ");
  Serial.println(NUMERO_SERIE_CADEIRA);
  Serial.print("WiFi: ");
  #if OFFLINE_DEMO
  Serial.println("OFFLINE_DEMO");
  #else
  Serial.println(WiFi.status() == WL_CONNECTED ? "ONLINE" : "OFFLINE");
  #endif
  Serial.print("Memoria livre: ");
  Serial.print(ESP.getFreeHeap());
  Serial.println(" bytes");
  Serial.println("====================================");
}

static bool trendInputsConfigured() {
  return TREN_INT_DESCE >= 0 || INT_TREND_DESCE >= 0 || TREN_INT_SOBE >= 0 || INT_TREND_SOBE >= 0;
}

static bool trendReadDebouncedHigh(int pin, bool& initialized, bool& raw, bool& stable, uint32_t& changedAtMs) {
  static const uint32_t TREND_DEBOUNCE_MS = 10;
  if (pin < 0) {
    return false;
  }
  static int8_t idleLevelByPin[49];
  static bool idleInit = false;
  if (!idleInit) {
    for (size_t i = 0; i < sizeof(idleLevelByPin); i++) {
      idleLevelByPin[i] = -1;
    }
    idleInit = true;
  }
  uint32_t now = millis();
  bool cur = digitalRead(pin);
  bool idleLevel = true;
  if (pin >= 0 && pin < static_cast<int>(sizeof(idleLevelByPin))) {
    if (idleLevelByPin[pin] == -1) {
      idleLevelByPin[pin] = cur ? 1 : 0;
    }
    idleLevel = (idleLevelByPin[pin] != 0);
  }
  if (!initialized) {
    initialized = true;
    raw = cur;
    stable = cur;
    changedAtMs = now;
    return stable != idleLevel;
  }
  if (cur != raw) {
    raw = cur;
    changedAtMs = now;
  }
  if ((now - changedAtMs) < TREND_DEBOUNCE_MS) {
    return stable != idleLevel;
  }
  stable = raw;
  return stable != idleLevel;
}

static void trendDebugDump(const char* tag) {
  bool hasUpPins = (TREN_INT_SOBE >= 0 || INT_TREND_SOBE >= 0);
  bool mapUpRaw = false;
  bool mapDownRaw = false;
  if (hasUpPins) {
    if (TREN_INT_SOBE >= 0 && digitalRead(TREN_INT_SOBE) == HIGH) mapUpRaw = true;
    if (INT_TREND_SOBE >= 0 && digitalRead(INT_TREND_SOBE) == HIGH) mapUpRaw = true;
  } else {
    if (INT_TREND_DESCE >= 0 && digitalRead(INT_TREND_DESCE) == HIGH) mapUpRaw = true;
  }
  if (TREN_INT_DESCE >= 0 && digitalRead(TREN_INT_DESCE) == HIGH) mapDownRaw = true;

  String msg = "[TREND_DBG] ";
  msg += (tag ? tag : "DUMP");
  msg += " GPIO_IN{";
  msg += "TREN_D=";
  msg += (TREN_INT_DESCE >= 0 ? String(digitalRead(TREN_INT_DESCE) == HIGH ? 1 : 0) : "NA");
  msg += ",INT_D=";
  msg += (INT_TREND_DESCE >= 0 ? String(digitalRead(INT_TREND_DESCE) == HIGH ? 1 : 0) : "NA");
  msg += ",TREN_U=";
  msg += (TREN_INT_SOBE >= 0 ? String(digitalRead(TREN_INT_SOBE) == HIGH ? 1 : 0) : "NA");
  msg += ",INT_U=";
  msg += (INT_TREND_SOBE >= 0 ? String(digitalRead(INT_TREND_SOBE) == HIGH ? 1 : 0) : "NA");
  msg += "} GPIO_OUT{";
  msg += "D=";
  msg += (Rele_TREND_DESCE >= 0 ? String(digitalRead(Rele_TREND_DESCE) == HIGH ? 1 : 0) : "NA");
  msg += ",U=";
  msg += (Rele_TREND_SOBE >= 0 ? String(digitalRead(Rele_TREND_SOBE) == HIGH ? 1 : 0) : "NA");
  msg += "} STATE{";
  msg += "upOn=";
  msg += (estado_trend_sobe ? "1" : "0");
  msg += ",downOn=";
  msg += (estado_trend_desce ? "1" : "0");
  msg += "} MAP{";
  msg += "U=";
  msg += (mapUpRaw ? "1" : "0");
  msg += ",D=";
  msg += (mapDownRaw ? "1" : "0");
  msg += "}";
  Serial.println(msg);
}

static void trendDebugTick() {
  if (!trendDebugEnabled) {
    return;
  }
  uint32_t now = millis();
  if (now < trendDebugNextMs) {
    return;
  }
  trendDebugNextMs = now + trendDebugIntervalMs;
  trendDebugDump("TICK");
}

static void trendTickInputs() {
  if (!trendInputsConfigured()) {
    return;
  }
  if (Rele_TREND_SOBE < 0 && Rele_TREND_DESCE < 0) {
    return;
  }

  static const uint32_t TREND_GPIO_PRINT_STABLE_MS = 50;
  uint32_t nowDbg = millis();

  static int8_t idleLevelByPin[49];
  static int8_t lastRawByPin[49];
  static uint32_t activeSinceByPin[49];
  static bool printedByPin[49];
  static bool initByPin = false;
  if (!initByPin) {
    for (size_t i = 0; i < sizeof(idleLevelByPin); i++) {
      idleLevelByPin[i] = -1;
      lastRawByPin[i] = -1;
      activeSinceByPin[i] = 0;
      printedByPin[i] = false;
    }
    initByPin = true;
  }

  auto tickAcionado = [&](int pin) {
    if (pin < 0 || pin >= static_cast<int>(sizeof(idleLevelByPin))) {
      return;
    }
    int raw = digitalRead(pin) == HIGH ? 1 : 0;
    if (idleLevelByPin[pin] == -1) {
      idleLevelByPin[pin] = raw;
      lastRawByPin[pin] = raw;
      activeSinceByPin[pin] = 0;
      printedByPin[pin] = false;
      return;
    }
    bool active = (raw != idleLevelByPin[pin]);
    if (active) {
      if (lastRawByPin[pin] == idleLevelByPin[pin]) {
        activeSinceByPin[pin] = nowDbg;
        printedByPin[pin] = false;
      }
      if (!printedByPin[pin] && activeSinceByPin[pin] > 0 && (nowDbg - activeSinceByPin[pin]) >= TREND_GPIO_PRINT_STABLE_MS) {
        Serial.print("GPIO ");
        Serial.print(pin);
        Serial.println(" ACIONADO");
        printedByPin[pin] = true;
      }
    } else {
      activeSinceByPin[pin] = 0;
      printedByPin[pin] = false;
    }
    lastRawByPin[pin] = raw;
  };

  tickAcionado(TREN_INT_DESCE);
  tickAcionado(INT_TREND_DESCE);
  tickAcionado(TREN_INT_SOBE);
  tickAcionado(INT_TREND_SOBE);
  tickAcionado(GAVETA);

  auto isActiveStable = [&](int pin) -> bool {
    if (pin < 0 || pin >= static_cast<int>(sizeof(idleLevelByPin))) {
      return false;
    }
    if (idleLevelByPin[pin] == -1) {
      return false;
    }
    uint32_t since = activeSinceByPin[pin];
    if (since == 0) {
      return false;
    }
    return (nowDbg - since) >= TREND_GPIO_PRINT_STABLE_MS;
  };

  bool hasUpPins = (TREN_INT_SOBE >= 0 || INT_TREND_SOBE >= 0);
  bool up = false;
  if (hasUpPins) {
    up = isActiveStable(TREN_INT_SOBE) || isActiveStable(INT_TREND_SOBE);
  } else {
    up = isActiveStable(INT_TREND_DESCE);
  }

  bool down = isActiveStable(TREN_INT_DESCE);

  static bool lastConflict = false;
  bool conflict = up && down;
  if (conflict) {
    if (Rele_TREND_SOBE >= 0) setOutputPin(Rele_TREND_SOBE, false, "TREND_IN");
    if (Rele_TREND_DESCE >= 0) setOutputPin(Rele_TREND_DESCE, false, "TREND_IN");
    estado_trend_sobe = false;
    estado_trend_desce = false;
    trendRemoteActive = false;
    faz_bt_seg = 0;
    paraTimerMotor();
    if (!lastConflict) {
      enviarBLE("TREND:CONFLICT");
      mqttEnqueuePublish(MQTT_TOPIC_BASE + "tx_cmd", "TREND_CONFLICT", false);
      mqttStatusDirty = true;
    }
    lastConflict = true;
    return;
  }
  lastConflict = false;

  if (up) {
    uint32_t now = millis();
    ultimoComandoTrendSobe = now;
    lastTrendCmdMs = now;
    trendRemoteActive = false;
    trendLastPulseAtMs = now;
    last_pulses_trend = pulses_trend;

    if (!estado_trend_sobe) {
      if (estado_trend_desce) {
        if (Rele_TREND_DESCE >= 0) setOutputPin(Rele_TREND_DESCE, false, "TREND_IN");
        estado_trend_desce = false;
        mqttStatusDirty = true;
        mqttEnqueuePublish(MQTT_TOPIC_BASE + "tx_cmd", "TREND_DOWN_OFF", false);
      }
      estado_trend_sobe = true;
      if (Rele_TREND_SOBE >= 0) setOutputPin(Rele_TREND_SOBE, true, "TREND_IN");
      faz_bt_seg = 1;
      iniciaTimerMotor();
      mqttStatusDirty = true;
      mqttEnqueuePublish(MQTT_TOPIC_BASE + "tx_cmd", "TREND_UP_ON", false);
    }
    return;
  }

  if (down) {
    uint32_t now = millis();
    ultimoComandoTrendDesce = now;
    lastTrendCmdMs = now;
    trendRemoteActive = false;
    trendLastPulseAtMs = now;
    last_pulses_trend = pulses_trend;

    if (!estado_trend_desce) {
      if (estado_trend_sobe) {
        if (Rele_TREND_SOBE >= 0) setOutputPin(Rele_TREND_SOBE, false, "TREND_IN");
        estado_trend_sobe = false;
        mqttStatusDirty = true;
        mqttEnqueuePublish(MQTT_TOPIC_BASE + "tx_cmd", "TREND_UP_OFF", false);
      }
      estado_trend_desce = true;
      if (Rele_TREND_DESCE >= 0) setOutputPin(Rele_TREND_DESCE, true, "TREND_IN");
      faz_bt_seg = 1;
      iniciaTimerMotor();
      mqttStatusDirty = true;
      mqttEnqueuePublish(MQTT_TOPIC_BASE + "tx_cmd", "TREND_DOWN_ON", false);
    }
    return;
  }

  if (estado_trend_sobe || estado_trend_desce) {
    if (trendRemoteActive) {
      return;
    }
    bool wasUp = estado_trend_sobe;
    bool wasDown = estado_trend_desce;
    if (Rele_TREND_SOBE >= 0) setOutputPin(Rele_TREND_SOBE, false, "TREND_IN");
    if (Rele_TREND_DESCE >= 0) setOutputPin(Rele_TREND_DESCE, false, "TREND_IN");
    estado_trend_sobe = false;
    estado_trend_desce = false;
    faz_bt_seg = 0;
    paraTimerMotor();
    mqttStatusDirty = true;
    enviarBLE("TREND:OFF");
    if (wasUp) mqttEnqueuePublish(MQTT_TOPIC_BASE + "tx_cmd", "TREND_UP_OFF", false);
    if (wasDown) mqttEnqueuePublish(MQTT_TOPIC_BASE + "tx_cmd", "TREND_DOWN_OFF", false);
    mqttEnqueuePublish(MQTT_TOPIC_BASE + "tx_cmd", "TREND_OFF", false);
  }
}

// ========== MONITORAMENTO DE TIMEOUT DOS MOTORES (DEAD MAN'S SWITCH) ==========
void verificaTimeoutMotores() {
  unsigned long agora = millis();
  uint32_t curTrend = pulses_trend;
  if (curTrend != last_pulses_trend) {
    last_pulses_trend = curTrend;
    trendLastPulseAtMs = agora;
  }
  
  // SE - Encosto sobe
  if (estado_se && (agora - ultimoComandoSE) > MOTOR_TIMEOUT) {
    estado_se = false;
    setOutputPin(Rele_SE, false, "TIMEOUT");
    faz_bt_seg = 0;
    enviarBLE("SE:TIMEOUT");
    Serial.println("[TIMEOUT] Motor SE desligado por seguranca");
  }
  
  // DE - Encosto desce
  if (estado_de && (agora - ultimoComandoDE) > MOTOR_TIMEOUT) {
    estado_de = false;
    setOutputPin(Rele_DE, false, "TIMEOUT");
    faz_bt_seg = 0;
    enviarBLE("DE:TIMEOUT");
    Serial.println("[TIMEOUT] Motor DE desligado por seguranca");
  }
  
  // SA - Assento sobe
  if (estado_sa && (agora - ultimoComandoSA) > MOTOR_TIMEOUT) {
    estado_sa = false;
    setOutputPin(Rele_SA, false, "TIMEOUT");
    faz_bt_seg = 0;
    enviarBLE("SA:TIMEOUT");
    Serial.println("[TIMEOUT] Motor SA desligado por seguranca");
  }
  
  // DA - Assento desce
  if (estado_da && (agora - ultimoComandoDA) > MOTOR_TIMEOUT) {
    estado_da = false;
    setOutputPin(Rele_DA, false, "TIMEOUT");
    faz_bt_seg = 0;
    enviarBLE("DA:TIMEOUT");
    Serial.println("[TIMEOUT] Motor DA desligado por seguranca");
  }
  
  // SP - Pernas sobem
  if (estado_sp && (agora - ultimoComandoSP) > MOTOR_TIMEOUT) {
    estado_sp = false;
    setOutputPin(Rele_SP, false, "TIMEOUT");
    faz_bt_seg = 0;
    enviarBLE("SP:TIMEOUT");
    Serial.println("[TIMEOUT] Motor SP desligado por seguranca");
  }
  
  // DP - Pernas descem
  if (estado_dp && (agora - ultimoComandoDP) > MOTOR_TIMEOUT) {
    estado_dp = false;
    setOutputPin(Rele_DP, false, "TIMEOUT");
    faz_bt_seg = 0;
    enviarBLE("DP:TIMEOUT");
    Serial.println("[TIMEOUT] Motor DP desligado por seguranca");
  }

  // TS/TD - Trend (watchdog tolerante)
  bool tsInputActive = false;
  if (TREN_INT_SOBE >= 0 || INT_TREND_SOBE >= 0) {
    if (TREN_INT_SOBE >= 0 && digitalRead(TREN_INT_SOBE) == HIGH) tsInputActive = true;
    if (INT_TREND_SOBE >= 0 && digitalRead(INT_TREND_SOBE) == HIGH) tsInputActive = true;
  } else {
    if (INT_TREND_DESCE >= 0 && digitalRead(INT_TREND_DESCE) == HIGH) tsInputActive = true;
  }
  bool tdInputActive = false;
  if (TREN_INT_DESCE >= 0 && digitalRead(TREN_INT_DESCE) == HIGH) tdInputActive = true;
  if ((tsInputActive || tdInputActive) && lastTrendCmdMs == 0) {
    lastTrendCmdMs = agora;
  }
  if ((tsInputActive || tdInputActive) && (agora - lastTrendCmdMs) > 50) {
    lastTrendCmdMs = agora;
  }
  if ((estado_trend_sobe || estado_trend_desce) && !tsInputActive && !tdInputActive && lastTrendCmdMs > 0 && (agora - lastTrendCmdMs) > TREND_WATCHDOG_MS) {
    bool wasUp = estado_trend_sobe;
    bool wasDown = estado_trend_desce;
    if (Rele_TREND_SOBE >= 0) setOutputPin(Rele_TREND_SOBE, false, "TREND_WD");
    if (Rele_TREND_DESCE >= 0) setOutputPin(Rele_TREND_DESCE, false, "TREND_WD");
    estado_trend_sobe = false;
    estado_trend_desce = false;
    trendRemoteActive = false;
    faz_bt_seg = 0;
    mqttStatusDirty = true;
    enviarBLE("TREND:OFF");
    Serial.println("[TREND_WD] Timeout. Trend desligado por seguranca");
    if (wasUp) mqttEnqueuePublish(MQTT_TOPIC_BASE + "tx_cmd", "TREND_UP_OFF", false);
    if (wasDown) mqttEnqueuePublish(MQTT_TOPIC_BASE + "tx_cmd", "TREND_DOWN_OFF", false);
    mqttEnqueuePublish(MQTT_TOPIC_BASE + "tx_cmd", "TREND_OFF", false);
  }

  if ((estado_trend_sobe || estado_trend_desce) && ENCODER_TREND >= 0 && trendLastPulseAtMs > 0 && (agora - trendLastPulseAtMs) > 500) {
    if (Rele_TREND_SOBE >= 0) setOutputPin(Rele_TREND_SOBE, false, "TREND_ENC");
    if (Rele_TREND_DESCE >= 0) setOutputPin(Rele_TREND_DESCE, false, "TREND_ENC");
    estado_trend_sobe = false;
    estado_trend_desce = false;
    faz_bt_seg = 0;
    enviarBLE("TREND:STALL");
    Serial.println("[TREND_ENC] Sem pulsos. Trend desligado por seguranca");
  }
}

static void reflectorAutoTick() {
  uint8_t kindNow = 1;
  if (cont == 1 || vzInicialEmAndamento) {
    kindNow = 3;
  } else if (cont13 == 1) {
    kindNow = 2;
  }

  bool moving =
      (Rele_SE >= 0 && relayIsOn(Rele_SE)) ||
      (Rele_DE >= 0 && relayIsOn(Rele_DE)) ||
      (Rele_SA >= 0 && relayIsOn(Rele_SA)) ||
      (Rele_DA >= 0 && relayIsOn(Rele_DA)) ||
      (Rele_SP >= 0 && relayIsOn(Rele_SP)) ||
      (Rele_DP >= 0 && relayIsOn(Rele_DP)) ||
      (Rele_TREND_SOBE >= 0 && relayIsOn(Rele_TREND_SOBE)) ||
      (Rele_TREND_DESCE >= 0 && relayIsOn(Rele_TREND_DESCE));

  if (!reflectorMoveActive && moving) {
    reflectorMoveActive = true;
    reflectorWasOnAtMoveStart = (Rele_refletor >= 0) ? relayIsOn(Rele_refletor) : false;
    reflectorMoveKind = kindNow;
  }

  if (reflectorMoveActive && moving) {
    if (kindNow == 3) {
      reflectorMoveKind = 3;
    } else if (kindNow == 2 && reflectorMoveKind != 3) {
      reflectorMoveKind = 2;
    }
    if (Rele_refletor >= 0 && relayIsOn(Rele_refletor)) {
      setOutputPin(Rele_refletor, false, "REF_MOVE");
      estado_r = false;
      enviarBLE("RF:AUTO_OFF");
      mqttEnqueuePublish(MQTT_TOPIC_BASE + "tx_cmd", "RF_AUTO_OFF", false);
    }
    return;
  }

  if (reflectorMoveActive && !moving) {
    reflectorMoveActive = false;
    if (reflectorMoveKind == 2) {
      if (Rele_refletor >= 0) {
        setOutputPin(Rele_refletor, true, "REF_PT");
        estado_r = true;
        enviarBLE("RF:PT_ON");
        mqttEnqueuePublish(MQTT_TOPIC_BASE + "tx_cmd", "RF_PT_ON", false);
      }
    } else if (reflectorMoveKind == 3) {
      if (Rele_refletor >= 0) {
        setOutputPin(Rele_refletor, false, "REF_VZ");
        estado_r = false;
        enviarBLE("RF:VZ_OFF");
        mqttEnqueuePublish(MQTT_TOPIC_BASE + "tx_cmd", "RF_VZ_OFF", false);
      }
    } else if (reflectorWasOnAtMoveStart) {
      if (Rele_refletor >= 0) {
        setOutputPin(Rele_refletor, true, "REF_RESTORE");
        estado_r = true;
        enviarBLE("RF:RESTORE_ON");
        mqttEnqueuePublish(MQTT_TOPIC_BASE + "tx_cmd", "RF_RESTORE_ON", false);
      }
    }
    reflectorMoveKind = 0;
    reflectorWasOnAtMoveStart = false;
  }
}

static void reflectorPreMove(uint8_t kind, const char* src) {
  if (!reflectorMoveActive) {
    reflectorMoveActive = true;
    reflectorWasOnAtMoveStart = (Rele_refletor >= 0) ? relayIsOn(Rele_refletor) : false;
    reflectorMoveKind = kind;
  } else {
    if (kind == 3) {
      reflectorMoveKind = 3;
    } else if (kind == 2 && reflectorMoveKind != 3) {
      reflectorMoveKind = 2;
    }
  }

  if (Rele_refletor >= 0 && relayIsOn(Rele_refletor)) {
    setOutputPin(Rele_refletor, false, src ? src : "REF_PRE");
    estado_r = false;
    enviarBLE("RF:AUTO_OFF");
    mqttEnqueuePublish(MQTT_TOPIC_BASE + "tx_cmd", "RF_AUTO_OFF", false);
  }
}

// ========== LOOP PRINCIPAL ==========
void loop() {
  if (powerFailDetected) {
    // Queda de energia detectada! Salva apenas as posições dos encoders imediatamente.
    saveMotorTravelPreferences(true);
    Serial.println("\n[AVISO] QUEDA DE ENERGIA! Posicoes salvas.");
    while(1) { delay(1000); } // Aguarda o desligamento total
  }
#if PORTS_ONLY || PORTS_VERIFY
  if (pcf8574InterruptPending) {
    bool okRead = false;
    uint8_t in = pcf8574ReadByte(static_cast<uint8_t>(PCF8574_ADDRESS), &okRead);
    if (okRead) {
      pcf8574ReportChanges(pcf8574LastIn, in, "[PCF8574_INT]");
      pcf8574LastIn = in;
    }
    pcf8574InterruptPending = false;
  }
  processaComandosBluetooth();
#if PORTS_VERIFY
  Button_geral();
  Button_Seg();
  monitora_tempo_rele();
  contagem_tempo_incoder_virtual();
  encoderMonTick();
#endif
  reflectorAutoTick();
  delay(10);
  return;
#endif
  // Se VZ inicial estiver em andamento, nÃ£o executa nada do loop
  if (vzInicialEmAndamento) {
    reflectorAutoTick();
    delay(100);
    return;
  }
  
  #if !OFFLINE_DEMO
  verificaBotaoResetWifi();
  #endif
  
  // Verifica timeout dos motores (dead man's switch)
  verificaTimeoutMotores();
  
  // Processa comandos Bluetooth do app
  processaComandosBluetooth();
  #if !OFFLINE_DEMO
  if (mqttCommandQueue) {
    MqttCmdItem item;
    while (xQueueReceive(mqttCommandQueue, &item, 0) == pdTRUE) {
      String cmd = String(item.cmd);
      cmd.trim();
      if (cmd.length() > 0) {
        executaComandoBluetooth(cmd, "MQTT");
      }
    }
  }
  #endif
  inputsDebugTick();
  trendTickInputs();
  trendDebugTick();
  reflectorAutoTick();

  // Monitoramento do sistema (Debug)
  monitoraSistema();

  // Atualiza horÃ­metro
  atualizaHorimetro();

  #if !OFFLINE_DEMO
  atualizaSupabase();
  sendMotorTravelToSupabaseIfNeeded();
  verificacaoPeriodicaStatus();
  #endif

  // FunÃ§Ãµes originais de controle
  contagem_tempo_incoder_virtual();
  motorTravelAutosaveTick();
  encoderMonTick();
  Watch_Dog();
  buzzerTestTick();
  beepSeqTick();
  alarmTick();
  Button_Seg();
  Button_geral();
  monitora_tempo_rele();
  #if !OFFLINE_DEMO
  if (mqttStatusDirty && mqttClient.connected() && (millis() - mqttLastStatusPublishMs) > 250) {
    mqttStatusDirty = false;
    mqttLastStatusPublishMs = millis();
    publicaStatusMQTT();
  }
  otaTick();
  #endif

#if HAS_BLE
  bleAtualizaDisponibilidade();
#endif
}

// ========== GERAÃ‡ÃƒO DO NÃšMERO DE SÃ‰RIE BASEADO NO MAC ==========
String geraNumeroSerieDoMAC() {
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  
  // Formato: CADEIRA-XXXXXXXXXXXX (12 caracteres hexadecimais do MAC)
  char serial[25];
  snprintf(serial, sizeof(serial), "CADEIRA-%02X%02X%02X%02X%02X%02X", 
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  
  return String(serial);
}

// ========== CONFIGURAÃ‡ÃƒO WIFI MANAGER ==========
void configuraWiFiManager() {
  Serial.println("[WiFi] Configurando WiFiManager...");
  
  // Configura timeout para o portal de configuração
  wifiManager.setConfigPortalTimeout(TIMEOUT_CONFIG_WIFI);
  
  // Configura título do portal (sem acentuação para evitar problemas de encoding)
  wifiManager.setTitle("VIP Hospitalar - Cadeira Odontologica");
  
  // Adiciona logo VIP Hospitalar no topo do portal
  wifiManager.setCustomHeadElement(LOGO_VIP_HOSPITALAR);
  
  // Adiciona informação da cadeira no menu
  String infoHTML = "<div style='text-align:center; margin:10px; padding:10px; background:#f0f0f0; border-radius:5px;'><b>" + NOME_DISPOSITIVO + "</b><br><small>Serial: " + NUMERO_SERIE_CADEIRA + "</small></div>";
  wifiManager.setCustomMenuHTML(infoHTML.c_str());
  
  Serial.println("[WiFi] Tentando conectar...");
  
  // autoConnect tenta conectar com credenciais salvas
  // Se falhar, abre um Access Point para configuraÃ§Ã£o
  if (!wifiManager.autoConnect(NOME_DISPOSITIVO.c_str(), SENHA_AP)) {
    Serial.println("[WiFi] Falha na conexÃ£o - timeout");
    Serial.println("[WiFi] Continuando em modo offline...");
    wifiLostAtMs = millis();
  } else {
    Serial.println("WiFi conectado!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("SSID: ");
    Serial.println(WiFi.SSID());
    
    bip();
    wifiOkAtMs = millis();
    wifiLostAtMs = 0;
    supabaseLogUsage("WIFI_CONNECTED");
    #if !(PORTS_ONLY) && !(PORTS_VERIFY)
    iniciaCalibracaoSeNecessario();
    #endif
  }
}

// Verifica se botÃ£o de reset WiFi estÃ¡ pressionado por 5 segundos
void verificaBotaoResetWifi() {
  if (botaoResetWifiPin >= 0 && digitalRead(botaoResetWifiPin) == LOW) {
    if (!botaoResetWifiPressionado) {
      tempoPressionadoBotaoResetWifi = millis();
      botaoResetWifiPressionado = true;
    } else {
      // Verifica se passou 5 segundos
      if (millis() - tempoPressionadoBotaoResetWifi >= 5000) {
        Serial.println("Resetando configuraÃ§Ãµes WiFi...");
        resetaConfiguracoesWifi();
        botaoResetWifiPressionado = false;
      }
    }
  } else {
    botaoResetWifiPressionado = false;
  }
}

// Reseta configuraÃ§Ãµes WiFi e reinicia o ESP
void resetaConfiguracoesWifi() {
  // Bips indicando reset
  for (int i = 0; i < 5; i++) {
    bip();
    delay(100);
  }
  
  // Limpa credenciais salvas
  wifiManager.resetSettings();

  Preferences p;
  p.begin("cadeira", false);
  p.remove("cal_done");
  p.remove("encosto_max");
  p.remove("assento_max");
  p.remove("perneira_max");
  p.end();
  
  Serial.println("ConfiguraÃ§Ãµes WiFi apagadas. Reiniciando...");
  delay(1000);
  
  // Reinicia o ESP32
  ESP.restart();
}

void resetCalibracao() {
  Preferences p;
  p.begin("cadeira", false);
  p.remove("cal_done");
  p.remove("encosto_max");
  p.remove("assento_max");
  p.remove("perneira_max");
  p.end();
  fim_encosto_encoder = 0;
  fim_asento_encoder = 0;
  fim_perneira_encoder = 0;
  Serial.println("[CAL] Dados de calibracao apagados (cal_done, *_max).");
}

static void clearPrefsNamespace(const char* ns) {
  Preferences p;
  if (!p.begin(ns, false)) return;
  p.clear();
  p.end();
}

static void checkBootHoldResetTudo() {
  const int bootPin = PIN_BOOT_BUTTON;
  if (bootPin < 0) return;
  pinMode(bootPin, INPUT_PULLUP);
  if (digitalRead(bootPin) != LOW) return;

  if (LED >= 0) {
    pinMode(LED, OUTPUT);
  }

  Serial.println("[RESET] BOOT pressionado. Segure 5s para RESET_TUDO.");
  uint32_t pressedAt = millis();
  uint32_t lastBlinkAt = 0;
  bool ledOn = false;
  while (digitalRead(bootPin) == LOW) {
    uint32_t now = millis();
    if (LED >= 0 && (now - lastBlinkAt) >= 150) {
      lastBlinkAt = now;
      ledOn = !ledOn;
      digitalWrite(LED, ledOn ? HIGH : LOW);
    }
    if ((now - pressedAt) >= 5000) {
      if (LED >= 0) digitalWrite(LED, HIGH);
      Serial.println("[RESET] RESET_TUDO acionado por BOOT.");
      delay(200);
      resetParaPrimeiraVez();
      return;
    }
    delay(10);
  }
  if (LED >= 0) digitalWrite(LED, LOW);
  Serial.println("[RESET] BOOT solto. Cancelado.");
}

static void resetParaPrimeiraVez() {
  Serial.println("[RESET] Apagando dados salvos (NVS)...");
  delay(100);
  clearPrefsNamespace("motor_travel");
  clearPrefsNamespace("cadeira");
  clearPrefsNamespace("encoder_encosto");
  clearPrefsNamespace("encoder_asento");
  clearPrefsNamespace("encoder_pern");
  clearPrefsNamespace("horimetro");
  clearPrefsNamespace("mqtt");
  clearPrefsNamespace("supabase");
  wifiManager.resetSettings();

  motorTravelPulsesEncosto = 0;
  motorTravelPulsesAssento = 0;
  motorTravelPulsesPerneira = 0;
  motorTravelPulsesTrend = 0;
  motorTravelUnsavedEncosto = 0;
  motorTravelUnsavedAssento = 0;
  motorTravelUnsavedPerneira = 0;
  motorTravelUnsavedTrend = 0;
  incoder_virtual_encosto_service = 0;
  incoder_virtual_asento_service = 0;
  incoder_virtual_perneira_service = 0;
  incoder_virtual_encosto_M1 = 0;
  incoder_virtual_asento_M1 = 0;
  incoder_virtual_perneira_M1 = 0;
  fim_encosto_encoder = 0;
  fim_asento_encoder = 0;
  fim_perneira_encoder = 0;
  seatTravelLearned = false;
  supabaseUserId = "";
  supabaseMaintenanceRequestSent = false;
  horimetro = 0.0f;
  totalMillisMotor = 0;

  Serial.println("[RESET] OK. Reiniciando...");
  delay(500);
  ESP.restart();
}

// ========== BLUETOOTH - PROCESSA COMANDOS DO APP ==========
void processaComandosBluetooth() {
#if HAS_BLE
  if (comandoBLE.length() == 0) {
    String nextCmd;
    if (bleCmdDeq(&nextCmd)) {
      comandoBLE = nextCmd;
    }
  }
  if (comandoBLE.length() > 0) {
    String cmd = comandoBLE;
    comandoBLE = "";
    cmd.trim();
    String cmdUpper = cmd;
    cmdUpper.toUpperCase();
    bool isMotorCmd = (cmdUpper == "SE" || cmdUpper == "DE" || cmdUpper == "SA" || 
                       cmdUpper == "DA" || cmdUpper == "SP" || cmdUpper == "DP");
    
    if (!isMotorCmd) {
      unsigned long agora = millis();
      if (cmdUpper == ultimoCmdBLE && (agora - ultimoComandoBLE) < 300) {
        Serial.println("[BLE] Comando duplicado ignorado: " + cmdUpper);
        return;
      }
      ultimoCmdBLE = cmdUpper;
      ultimoComandoBLE = agora;
    }
    
    executaComandoBluetooth(cmd, "BLE");
  }
  processaComandosSerialDebug();
#else
  processaComandosSerialDebug();
#endif
}

void executaComandoBluetooth(String cmd, const char* origin) {
  String cmdRaw = cmd;
  cmdRaw.trim();
  cmd = cmdRaw;
  cmd.toUpperCase();
  static uint32_t lastKeepSEMs = 0;
  static uint32_t lastKeepDEMs = 0;
  static uint32_t lastKeepSAMs = 0;
  static uint32_t lastKeepDAMs = 0;
  static uint32_t lastKeepSPMs = 0;
  static uint32_t lastKeepDPMs = 0;
  static uint32_t lastKeepTSMs = 0;
  static uint32_t lastKeepTDMs = 0;
  auto sendKeep = [&](const char* msg, uint32_t &lastMs) {
    uint32_t now = millis();
    if ((now - lastMs) > BLE_KEEP_MIN_INTERVAL_MS) {
      enviarBLE(msg);
      lastMs = now;
    }
  };
  if (origin && (String(origin) == "BLE" || String(origin) == "MQTT" || String(origin) == "SERIAL")) {
    String cu = cmd;
    bool isMotorCmd = (cu == "SE" || cu == "DE" || cu == "SA" || cu == "DA" || cu == "SP" || cu == "DP");
    if (isMotorCmd || cu == "STOP" || cu == "AT_SEG") {
      ultimoComandoRemoto = millis();
    }
  }
  String logCmd = cmd;
  if (cmd.startsWith("MQTT_PASS=") || cmd.startsWith("MQTT_PASS:")) {
    logCmd = "MQTT_PASS=***";
  }
  if (cmd.startsWith("SUPABASE_KEY=") || cmd.startsWith("SUPABASE_KEY:")) {
    logCmd = "SUPABASE_KEY=***";
  }
  Serial.print("Comando BT recebido: ");
  Serial.println(logCmd);
  mqttPublishTxCmd(cmd, origin);

#if I2C_EARLY_TEST
  if (!(cmd == "I2C_SCAN" || cmd == "PCF8574_PING" || cmd == "PCF8574_TEST")) {
    Serial.println("[I2C_TEST] Comando permitido: I2C_SCAN | PCF8574_PING | PCF8574_TEST");
    return;
  }
#endif

  if (cmd == "I2C_SCAN") {
    String found = i2cScanSummary();
    Serial.print("[I2C] Found: ");
    Serial.println(found);
    enviarBLE("I2C:FOUND:" + found);
    return;
  }

  if (cmd == "I2C_PIN_TEST") {
    Serial.print("[I2C] Pin test SDA=");
    Serial.print(I2C_SDA);
    Serial.print(" SCL=");
    Serial.println(I2C_SCL);
    Wire.end();
    i2cReady = false;
    pcf8574Available = false;

    pinMode(I2C_SDA, OUTPUT);
    pinMode(I2C_SCL, OUTPUT);
    for (int i = 0; i < 10; i++) {
      digitalWrite(I2C_SDA, (i % 2) ? HIGH : LOW);
      digitalWrite(I2C_SCL, (i % 2) ? LOW : HIGH);
      delay(250);
    }
    digitalWrite(I2C_SDA, HIGH);
    digitalWrite(I2C_SCL, HIGH);
    pinMode(I2C_SDA, INPUT_PULLUP);
    pinMode(I2C_SCL, INPUT_PULLUP);

    i2cReady = Wire.begin(I2C_SDA, I2C_SCL);
    if (i2cReady) {
      Wire.setClock(I2C_CLOCK_HZ);
      pcf8574Available = i2cPing(PCF8574_ADDRESS);
    }
    Serial.print("[I2C] Reinit: ");
    Serial.println(i2cReady ? "OK" : "FAIL");
    Serial.print("[I2C] PCF8574: ");
    Serial.println(pcf8574Available ? "OK" : "FAIL");
    enviarBLE(String("I2C:PIN_TEST:") + (i2cReady ? "OK" : "FAIL"));
    return;
  }

  if (cmd == "PCF8574_PING") {
    bool ok = i2cPing(static_cast<uint8_t>(PCF8574_ADDRESS));
    Serial.print("[PCF8574] Ping 0x");
    Serial.print(PCF8574_ADDRESS, HEX);
    Serial.print(": ");
    Serial.println(ok ? "OK" : "FAIL");
    enviarBLE(String("PCF8574:PING:") + (ok ? "OK" : "FAIL"));
    return;
  }

  if (cmd == "PCF8574_TEST") {
    bool ok1 = pcf8574WriteByte(static_cast<uint8_t>(PCF8574_ADDRESS), 0xFF);
    delay(10);
    bool okRead = false;
    uint8_t in = pcf8574ReadByte(static_cast<uint8_t>(PCF8574_ADDRESS), &okRead);
    Serial.print("[PCF8574] Test 0x");
    Serial.print(PCF8574_ADDRESS, HEX);
    Serial.print(": ");
    Serial.println((ok1 && okRead) ? "OK" : "FAIL");
    Serial.print("[PCF8574] IN=0x");
    Serial.println(in, HEX);
    enviarBLE(String("PCF8574:TEST:") + ((ok1 && okRead) ? "OK" : "FAIL"));
    return;
  }

  if (cmd == "PCF8574_BTNS") {
    bool okRead = false;
    uint8_t in = pcf8574ReadByte(static_cast<uint8_t>(PCF8574_ADDRESS), &okRead);
    Serial.print("[PCF8574] IN=0x");
    Serial.println(in, HEX);
    if (!okRead) {
      Serial.println("[PCF8574] READ FAIL");
      return;
    }
    Serial.print("P0(M1)="); Serial.println((in & (1U << 0)) ? "1" : "0");
    Serial.print("P1(SE)="); Serial.println((in & (1U << 1)) ? "1" : "0");
    Serial.print("P2(PT)="); Serial.println((in & (1U << 2)) ? "1" : "0");
    Serial.print("P3(VZ)="); Serial.println((in & (1U << 3)) ? "1" : "0");
    Serial.print("P4(DP)="); Serial.println((in & (1U << 4)) ? "1" : "0");
    Serial.print("P5(SP)="); Serial.println((in & (1U << 5)) ? "1" : "0");
    Serial.print("P6(DE)="); Serial.println((in & (1U << 6)) ? "1" : "0");
    Serial.print("P7(DA)="); Serial.println((in & (1U << 7)) ? "1" : "0");
    return;
  }

  if (cmd == "GPIO_BTNS") {
    Serial.print("SA(GPIO");
    Serial.print(SA);
    Serial.print(")=");
    Serial.println(digitalRead(SA) == HIGH ? "1" : "0");
    Serial.print("GAVETA(GPIO");
    Serial.print(GAVETA);
    Serial.print(")=");
    if (GAVETA >= 0) {
      Serial.println(digitalRead(GAVETA) == HIGH ? "1" : "0");
    } else {
      Serial.println("NA");
    }
    Serial.print("RF(GPIO");
    Serial.print(RF);
    Serial.print(")=");
    Serial.println(digitalRead(RF) == HIGH ? "1" : "0");
    return;
  }

  if (cmd.startsWith("GAVETA_PIN=") || cmd.startsWith("GAVETA_PIN:")) {
    int sep = cmd.indexOf('=');
    if (sep < 0) sep = cmd.indexOf(':');
    int newPin = cmd.substring(sep + 1).toInt();
    GAVETA = newPin;
    if (GAVETA >= 0) {
      pinMode(GAVETA, INPUT_PULLUP);
      lastButtonState_GAVETA = digitalRead(GAVETA);
      gavetaIdleLevel = lastButtonState_GAVETA;
      saveGavetaPinPreference();
      Serial.print("[GAVETA] PIN=");
      Serial.println(GAVETA);
    } else {
      Serial.println("[GAVETA] DESABILITADO");
      gavetaIdleLevel = -1;
    }
    return;
  }

  if (cmd.startsWith("USER_ID=") || cmd.startsWith("USER_ID:")) {
    int sep = cmd.indexOf('=');
    if (sep < 0) sep = cmd.indexOf(':');
    supabaseUserId = cmd.substring(sep + 1);
    supabaseUserId.trim();
    preferences.begin("cadeira", false);
    preferences.putString("user_id", supabaseUserId);
    preferences.end();
    enviarBLE("USER_ID:OK");
    Serial.print("[USER_ID] ");
    Serial.println(supabaseUserId);
    supabaseLogUsage("USER_ID_SET");
    return;
  }
  if (cmd == "USER_ID_STATUS") {
    enviarBLE("USER_ID:" + supabaseUserId);
    return;
  }

  if (cmd.startsWith("SUPABASE_URL=") || cmd.startsWith("SUPABASE_URL:")) {
    int sep = cmdRaw.indexOf('=');
    if (sep < 0) sep = cmdRaw.indexOf(':');
    supabaseUrl = cmdRaw.substring(sep + 1);
    supabaseUrl.trim();
    Preferences p;
    if (p.begin("supabase", false)) {
      p.putString("url", supabaseUrl);
      p.end();
    }
    enviarBLE("SUPABASE_URL:OK");
    return;
  }
  if (cmd.startsWith("SUPABASE_KEY=") || cmd.startsWith("SUPABASE_KEY:")) {
    int sep = cmdRaw.indexOf('=');
    if (sep < 0) sep = cmdRaw.indexOf(':');
    supabaseKey = cmdRaw.substring(sep + 1);
    supabaseKey.trim();
    Preferences p;
    if (p.begin("supabase", false)) {
      p.putString("key", supabaseKey);
      p.end();
    }
    enviarBLE("SUPABASE_KEY:OK");
    return;
  }
  if (cmd == "SUPABASE_STATUS") {
    enviarBLE(String("SUPABASE:URL:") + (supabaseUrl.length() ? supabaseUrl : "NA") + ":KEY_SET:" + (supabaseKey.length() ? "1" : "0"));
    return;
  }

  if (cmd.startsWith("MQTT_HOST=") || cmd.startsWith("MQTT_HOST:")) {
    int sep = cmdRaw.indexOf('=');
    if (sep < 0) sep = cmdRaw.indexOf(':');
    bool locked = mqttLockMs(200);
    if (!locked) {
      enviarBLE("MQTT_BUSY");
      return;
    }
    mqttHost = cmdRaw.substring(sep + 1);
    mqttHost.trim();
    saveMqttPreferences();
    applyMqttRuntimeConfig();
    mqttClient.disconnect();
    mqttUnlock();
    beepNetMqttOkDone = false;
    enviarBLE("MQTT_HOST:OK");
    return;
  }
  if (cmd.startsWith("MQTT_PORT=") || cmd.startsWith("MQTT_PORT:")) {
    int sep = cmdRaw.indexOf('=');
    if (sep < 0) sep = cmdRaw.indexOf(':');
    bool locked = mqttLockMs(200);
    if (!locked) {
      enviarBLE("MQTT_BUSY");
      return;
    }
    mqttPort = static_cast<uint16_t>(cmdRaw.substring(sep + 1).toInt());
    saveMqttPreferences();
    applyMqttRuntimeConfig();
    mqttClient.disconnect();
    mqttUnlock();
    beepNetMqttOkDone = false;
    enviarBLE("MQTT_PORT:OK");
    return;
  }
  if (cmd.startsWith("MQTT_TLS=") || cmd.startsWith("MQTT_TLS:")) {
    int sep = cmdRaw.indexOf('=');
    if (sep < 0) sep = cmdRaw.indexOf(':');
    bool locked = mqttLockMs(200);
    if (!locked) {
      enviarBLE("MQTT_BUSY");
      return;
    }
    bool newTls = (cmdRaw.substring(sep + 1).toInt() != 0);
    if (newTls != mqttUseTls) {
      if (newTls && mqttPort == 1883) mqttPort = 8883;
      if (!newTls && mqttPort == 8883) mqttPort = 1883;
    }
    mqttUseTls = newTls;
    saveMqttPreferences();
    applyMqttRuntimeConfig();
    mqttClient.disconnect();
    mqttUnlock();
    beepNetMqttOkDone = false;
    enviarBLE("MQTT_TLS:OK");
    return;
  }
  if (cmd.startsWith("MQTT_USER=") || cmd.startsWith("MQTT_USER:")) {
    int sep = cmdRaw.indexOf('=');
    if (sep < 0) sep = cmdRaw.indexOf(':');
    bool locked = mqttLockMs(200);
    if (!locked) {
      enviarBLE("MQTT_BUSY");
      return;
    }
    mqttUser = cmdRaw.substring(sep + 1);
    mqttUser.trim();
    saveMqttPreferences();
    mqttClient.disconnect();
    mqttUnlock();
    beepNetMqttOkDone = false;
    enviarBLE("MQTT_USER:OK");
    return;
  }
  if (cmd.startsWith("MQTT_PASS=") || cmd.startsWith("MQTT_PASS:")) {
    int sep = cmdRaw.indexOf('=');
    if (sep < 0) sep = cmdRaw.indexOf(':');
    bool locked = mqttLockMs(200);
    if (!locked) {
      enviarBLE("MQTT_BUSY");
      return;
    }
    mqttPass = cmdRaw.substring(sep + 1);
    mqttPass.trim();
    saveMqttPreferences();
    mqttClient.disconnect();
    mqttUnlock();
    beepNetMqttOkDone = false;
    enviarBLE("MQTT_PASS:OK");
    return;
  }
  if (cmd.startsWith("MQTT_CLIENT_ID=") || cmd.startsWith("MQTT_CLIENT_ID:")) {
    int sep = cmdRaw.indexOf('=');
    if (sep < 0) sep = cmdRaw.indexOf(':');
    bool locked = mqttLockMs(200);
    if (!locked) {
      enviarBLE("MQTT_BUSY");
      return;
    }
    mqttClientId = cmdRaw.substring(sep + 1);
    mqttClientId.trim();
    saveMqttPreferences();
    mqttClient.disconnect();
    mqttUnlock();
    beepNetMqttOkDone = false;
    enviarBLE("MQTT_CLIENT_ID:OK");
    return;
  }
  if (cmd.startsWith("MQTT_CLEAN=") || cmd.startsWith("MQTT_CLEAN:")) {
    int sep = cmdRaw.indexOf('=');
    if (sep < 0) sep = cmdRaw.indexOf(':');
    bool locked = mqttLockMs(200);
    if (!locked) {
      enviarBLE("MQTT_BUSY");
      return;
    }
    mqttCleanSession = (cmdRaw.substring(sep + 1).toInt() != 0);
    saveMqttPreferences();
    mqttClient.disconnect();
    mqttUnlock();
    beepNetMqttOkDone = false;
    enviarBLE("MQTT_CLEAN:OK");
    return;
  }
  if (cmd == "MQTT_STATUS") {
    Serial.print("[MQTT] HOST=");
    Serial.print(mqttHost);
    Serial.print(" PORT=");
    Serial.print(mqttPort);
    Serial.print(" TLS=");
    Serial.print(mqttUseTls ? "1" : "0");
    Serial.print(" CLIENT_ID=");
    Serial.print(mqttClientId.length() > 0 ? mqttClientId : ("ESP32-" + NUMERO_SERIE_CADEIRA));
    Serial.print(" CLEAN=");
    Serial.print(mqttCleanSession ? "1" : "0");
    Serial.print(" USER=");
    Serial.print(mqttUser.length() > 0 ? mqttUser : "(vazio)");
    Serial.print(" PASS=");
    Serial.println(mqttPass.length() > 0 ? "SET" : "(vazio)");
    enviarBLE("MQTT:OK");
    return;
  }
  if (cmd == "MQTT_RESET") {
    bool locked = mqttLockMs(200);
    if (!locked) {
      enviarBLE("MQTT_BUSY");
      return;
    }
    mqttHost = "broker.emqx.io";
    mqttPort = 8883;
    mqttUser = "";
    mqttPass = "";
    mqttUseTls = true;
    mqttClientId = "";
    mqttCleanSession = true;
    Preferences p;
    if (p.begin("mqtt", false)) {
      p.clear();
      p.end();
    }
    applyMqttRuntimeConfig();
    mqttClient.disconnect();
    mqttUnlock();
    beepNetMqttOkDone = false;
    enviarBLE("MQTT_RESET:OK");
    return;
  }

  if (cmd == "TLS_TEST") {
    Serial.println("====================================");
    Serial.println("             TLS_TEST               ");
    Serial.println("====================================");
    bool ok8883 = tlsHandshakeOnce(mqttHost, 8883);
    bool ok8886 = tlsHandshakeOnce(mqttHost, 8886);
    bool okCurrent = tlsHandshakeOnce(mqttHost, mqttPort);
    enviarBLE(String("TLS_TEST:HOST:") + mqttHost + ":8883:" + (ok8883 ? "OK" : "FAIL") + ":8886:" + (ok8886 ? "OK" : "FAIL") + ":CUR:" + String(mqttPort) + ":" + (okCurrent ? "OK" : "FAIL"));
    return;
  }

  if (cmd == "MQTT_RAW_TEST") {
    Serial.println("====================================");
    Serial.println("           MQTT_RAW_TEST            ");
    Serial.println("====================================");
    bool prevEnabled = mqttConnectEnabled;
    mqttConnectEnabled = false;
    if (mqttLockMs(200)) {
      mqttClient.disconnect();
      mqttUnlock();
    }
    delay(200);
    String cid = mqttClientId.length() > 0 ? mqttClientId : ("ESP32-" + NUMERO_SERIE_CADEIRA);
    bool ok8883 = mqttRawConnackOnce(mqttHost, 8883, cid);
    bool ok8886 = mqttRawConnackOnce(mqttHost, 8886, cid);
    bool okCurrent = mqttRawConnackOnce(mqttHost, mqttPort, cid);
    mqttConnectEnabled = prevEnabled;
    enviarBLE(String("MQTT_RAW_TEST:HOST:") + mqttHost + ":8883:" + (ok8883 ? "OK" : "FAIL") + ":8886:" + (ok8886 ? "OK" : "FAIL") + ":CUR:" + String(mqttPort) + ":" + (okCurrent ? "OK" : "FAIL"));
    return;
  }

  if (cmd == "LIMITS_OFF") {
    ignoreLimitLocks = true;
    ignoreGavetaLock = true;
    Serial.println("[LIMIT] LIMITS_OFF");
    enviarBLE("LIMITS:OFF");
    return;
  }

  if (cmd == "LIMITS_ON") {
    ignoreLimitLocks = false;
    ignoreGavetaLock = false;
    Serial.println("[LIMIT] LIMITS_ON");
    enviarBLE("LIMITS:ON");
    return;
  }

  if (cmd == "LIMITS_STATUS") {
    Serial.print("[LIMIT] IGNORE=");
    Serial.println(ignoreLimitLocks ? "1" : "0");
    enviarBLE(String("LIMITS:IGNORE:") + (ignoreLimitLocks ? "1" : "0") + ":GAVETA_IGNORE:" + (ignoreGavetaLock ? "1" : "0"));
    return;
  }

  if (cmd == "OTA_ON") {
    otaEnabled = true;
    otaSavePreferences();
    enviarBLE("OTA:ON");
    mqttEnqueuePublish(MQTT_TOPIC_BASE + "tx_cmd", "OTA_ON", false);
    mqttStatusDirty = true;
    return;
  }
  if (cmd == "OTA_OFF") {
    otaEnabled = false;
    otaSavePreferences();
    enviarBLE("OTA:OFF");
    mqttEnqueuePublish(MQTT_TOPIC_BASE + "tx_cmd", "OTA_OFF", false);
    mqttStatusDirty = true;
    return;
  }
  if (cmd == "OTA_STATUS") {
    enviarBLE(String("OTA:ENABLED:") + (otaEnabled ? "1" : "0") + ":INTERVAL:" + String(otaIntervalSec) + ":PENDING:" + (otaPendingVersion.length() ? otaPendingVersion : "NA") + ":MANIFEST:" + (otaManifestUrl.length() ? otaManifestUrl : "NA"));
    return;
  }
  if (cmd.startsWith("OTA_MANIFEST=") || cmd.startsWith("OTA_MANIFEST:")) {
    int sep = cmdRaw.indexOf('=');
    if (sep < 0) sep = cmdRaw.indexOf(':');
    otaManifestUrl = cmdRaw.substring(sep + 1);
    otaManifestUrl.trim();
    otaSavePreferences();
    enviarBLE("OTA_MANIFEST:OK");
    mqttEnqueuePublish(MQTT_TOPIC_BASE + "tx_cmd", "OTA_MANIFEST_SET", false);
    return;
  }
  if (cmd.startsWith("OTA_INTERVAL=") || cmd.startsWith("OTA_INTERVAL:")) {
    int sep = cmdRaw.indexOf('=');
    if (sep < 0) sep = cmdRaw.indexOf(':');
    uint32_t sec = static_cast<uint32_t>(cmdRaw.substring(sep + 1).toInt());
    if (sec < 60) sec = 60;
    otaIntervalSec = sec;
    otaSavePreferences();
    enviarBLE("OTA_INTERVAL:OK");
    mqttEnqueuePublish(MQTT_TOPIC_BASE + "tx_cmd", "OTA_INTERVAL_SET", false);
    return;
  }
  if (cmd == "OTA_CHECK") {
    OtaInfo info;
    ChairOtaInfo chair;
    bool ok = otaFetchUpdateInfo(info, chair);
    if (!ok) {
      enviarBLE("OTA:CHECK:FAIL");
      return;
    }
    if (chair.found && !chair.enabled) {
      enviarBLE("OTA:DISABLED");
      return;
    }
    if (!info.available) {
      enviarBLE("OTA:NO_UPDATE");
      return;
    }
    enviarBLE("OTA:UPDATE:" + info.version);
    otaDownloadAndUpdate(info);
    return;
  }
  if (cmd.startsWith("OTA_APPLY=") || cmd.startsWith("OTA_APPLY:")) {
    int sep = cmdRaw.indexOf('=');
    if (sep < 0) sep = cmdRaw.indexOf(':');
    String url = cmdRaw.substring(sep + 1);
    url.trim();
    OtaInfo info;
    info.available = true;
    info.mandatory = true;
    info.version = "MANUAL";
    info.url = url;
    info.md5 = "";
    info.size = 0;
    enviarBLE("OTA:APPLY");
    otaDownloadAndUpdate(info);
    return;
  }

  if (cmd == "BUZZER_TEST_ON") {
    buzzerTestEnabled = true;
    Serial.println("[BUZZER] TEST_ON");
    return;
  }
  if (cmd == "BUZZER_TEST_OFF") {
    buzzerTestEnabled = false;
    Serial.println("[BUZZER] TEST_OFF");
    return;
  }
  if (cmd == "BUZZER_TEST_STATUS") {
    Serial.print("[BUZZER] TEST=");
    Serial.println(buzzerTestEnabled ? "ON" : "OFF");
    return;
  }

  if (cmd == "INPUT_DBG_ON") {
    inputsDebugEnabled = true;
    inputsDebugLastPollMs = 0;
    lastGpioSa = -1;
    lastGpioRf = -1;
    lastGpioGaveta = -1;
    lastGpioTrendInDesce = -1;
    lastGpioTrendInSobe = -1;
    lastGpioIntTrendDesce = -1;
    lastGpioIntTrendSobe = -1;
    lastGpioPcfInt = -1;
#if USE_PCF8574
    inputsDebugLastPcf = 0xFF;
    inputsDebugLastIrqCount = pcf8574InterruptCount;
#endif
    Serial.println("[IN_DBG] ON");
    return;
  }
  if (cmd == "INPUT_DBG_OFF") {
    inputsDebugEnabled = false;
    Serial.println("[IN_DBG] OFF");
    return;
  }
  if (cmd == "INPUT_DBG_STATUS") {
    Serial.print("[IN_DBG] ");
    Serial.println(inputsDebugEnabled ? "ON" : "OFF");
    return;
  }

  if (cmd == "RELAY_STATUS") { relayTrackPrintOnRelays("STATUS"); return; }
  if (cmd == "RELAY_LIST") {
    Serial.println("[RELAY] TOKENS: SA DA SE DE SP DP RF TS TD LED BUZZER");
    Serial.print("[RELAY] SA="); Serial.print(Rele_SA);
    Serial.print(" DA="); Serial.print(Rele_DA);
    Serial.print(" SE="); Serial.print(Rele_SE);
    Serial.print(" DE="); Serial.print(Rele_DE);
    Serial.print(" SP="); Serial.print(Rele_SP);
    Serial.print(" DP="); Serial.print(Rele_DP);
    Serial.print(" RF="); Serial.print(Rele_refletor);
    Serial.print(" TS="); Serial.print(Rele_TREND_SOBE);
    Serial.print(" TD="); Serial.print(Rele_TREND_DESCE);
    Serial.print(" LED="); Serial.print(LED);
    Serial.print(" BUZZER="); Serial.println(BUZZER);
    return;
  }
  if (cmd == "RELAY_ALL_OFF") {
    setOutputPin(Rele_SA, false, "RELAY_ALL_OFF");
    setOutputPin(Rele_DA, false, "RELAY_ALL_OFF");
    setOutputPin(Rele_SE, false, "RELAY_ALL_OFF");
    setOutputPin(Rele_DE, false, "RELAY_ALL_OFF");
    setOutputPin(Rele_SP, false, "RELAY_ALL_OFF");
    setOutputPin(Rele_DP, false, "RELAY_ALL_OFF");
    setOutputPin(Rele_refletor, false, "RELAY_ALL_OFF");
    if (Rele_TREND_SOBE >= 0) setOutputPin(Rele_TREND_SOBE, false, "RELAY_ALL_OFF");
    if (Rele_TREND_DESCE >= 0) setOutputPin(Rele_TREND_DESCE, false, "RELAY_ALL_OFF");
    setOutputPin(LED, false, "RELAY_ALL_OFF");
    setOutputPin(BUZZER, false, "RELAY_ALL_OFF");
    Serial.println("[RELAY] ALL_OFF");
    return;
  }
  if (cmd.startsWith("RELAY_ON=") || cmd.startsWith("RELAY_ON:") ||
      cmd.startsWith("RELAY_OFF=") || cmd.startsWith("RELAY_OFF:") ||
      cmd.startsWith("RELAY_TOGGLE=") || cmd.startsWith("RELAY_TOGGLE:")) {
    bool doOn = cmd.startsWith("RELAY_ON=");
    bool doOff = cmd.startsWith("RELAY_OFF=");
    int sep = cmdRaw.indexOf('=');
    if (sep < 0) sep = cmdRaw.indexOf(':');
    String token = cmdRaw.substring(sep + 1);
    int pin = relayPinByToken(token);
    if (pin < 0) {
      Serial.println("[RELAY] INVALID_TOKEN");
      return;
    }
    if ((doOn || doOff) && pin < 0) {
      Serial.println("[RELAY] NA");
      return;
    }
    bool nextOn = doOn ? true : (doOff ? false : !relayIsOn(pin));
    setOutputPin(pin, nextOn, "RELAY_CMD");
    return;
  }
  if (cmd.startsWith("RELAY_PULSE=") || cmd.startsWith("RELAY_PULSE:")) {
    int sep = cmdRaw.indexOf('=');
    if (sep < 0) sep = cmdRaw.indexOf(':');
    String args = cmdRaw.substring(sep + 1);
    args.trim();
    int comma = args.indexOf(',');
    String token = (comma < 0) ? args : args.substring(0, comma);
    uint32_t ms = 500;
    if (comma >= 0) {
      String sMs = args.substring(comma + 1);
      sMs.trim();
      uint32_t parsed = static_cast<uint32_t>(sMs.toInt());
      if (parsed > 0) ms = parsed;
    }
    int pin = relayPinByToken(token);
    if (pin < 0) {
      Serial.println("[RELAY] INVALID_TOKEN");
      return;
    }
    setOutputPin(pin, true, "RELAY_PULSE");
    delay(ms);
    setOutputPin(pin, false, "RELAY_PULSE");
    return;
  }

  if (cmd == "TEST_SA") { ultimoComandoRemoto = millis(); setOutputPin(Rele_SA, true, "TEST"); delay(500); setOutputPin(Rele_SA, false, "TEST"); return; }
  if (cmd == "TEST_DA") { ultimoComandoRemoto = millis(); setOutputPin(Rele_DA, true, "TEST"); delay(500); setOutputPin(Rele_DA, false, "TEST"); return; }
  if (cmd == "TEST_SE") { ultimoComandoRemoto = millis(); setOutputPin(Rele_SE, true, "TEST"); delay(500); setOutputPin(Rele_SE, false, "TEST"); return; }
  if (cmd == "TEST_DE") { ultimoComandoRemoto = millis(); setOutputPin(Rele_DE, true, "TEST"); delay(500); setOutputPin(Rele_DE, false, "TEST"); return; }
  if (cmd == "TEST_SP") { ultimoComandoRemoto = millis(); setOutputPin(Rele_SP, true, "TEST"); delay(500); setOutputPin(Rele_SP, false, "TEST"); return; }
  if (cmd == "TEST_DP") { ultimoComandoRemoto = millis(); setOutputPin(Rele_DP, true, "TEST"); delay(500); setOutputPin(Rele_DP, false, "TEST"); return; }
  if (cmd == "TEST_RF") { ultimoComandoRemoto = millis(); setOutputPin(Rele_refletor, true, "TEST"); delay(500); setOutputPin(Rele_refletor, false, "TEST"); return; }

  if (cmd == "ENC_STATUS") {
    Serial.print("[ENC] ENCODER1(GPIO"); Serial.print(ENCODER1); Serial.print(") L="); Serial.println(ENCODER1 >= 0 ? (digitalRead(ENCODER1) == HIGH ? "1" : "0") : "NA");
    Serial.print("[ENC] ENCODER2(GPIO"); Serial.print(ENCODER2); Serial.print(") L="); Serial.println(ENCODER2 >= 0 ? (digitalRead(ENCODER2) == HIGH ? "1" : "0") : "NA");
    Serial.print("[ENC] ENCODER3(GPIO"); Serial.print(ENCODER3); Serial.print(") L="); Serial.println(ENCODER3 >= 0 ? (digitalRead(ENCODER3) == HIGH ? "1" : "0") : "NA");
    Serial.print("[ENC] TREND(GPIO"); Serial.print(ENCODER_TREND); Serial.print(") L="); Serial.println(ENCODER_TREND >= 0 ? (digitalRead(ENCODER_TREND) == HIGH ? "1" : "0") : "NA");
    Serial.print("[ENC] REQ ENC="); Serial.print(encReqEncosto() ? 1 : 0);
    Serial.print(" ASS="); Serial.print(encReqAssento() ? 1 : 0);
    Serial.print(" PER="); Serial.print(encReqPerneira() ? 1 : 0);
    Serial.print(" TRE="); Serial.println(encReqTrend() ? 1 : 0);
    Serial.print("[ENC] PULSES ENC="); Serial.print(pulses_encosto);
    Serial.print(" ASS="); Serial.print(pulses_assento);
    Serial.print(" PER="); Serial.print(pulses_perneira);
    Serial.print(" TREND="); Serial.println(pulses_trend);
    return;
  }
  if (cmd == "ENC_REQ_STATUS") {
    Serial.print("[ENC_REQ] ENC="); Serial.print(encReqEncosto() ? 1 : 0);
    Serial.print(" ASS="); Serial.print(encReqAssento() ? 1 : 0);
    Serial.print(" PER="); Serial.print(encReqPerneira() ? 1 : 0);
    Serial.print(" TRE="); Serial.println(encReqTrend() ? 1 : 0);
    return;
  }
  if (cmd.startsWith("ENC_REQ=") || cmd.startsWith("ENC_REQ:")) {
    int sep = cmdRaw.indexOf('=');
    if (sep < 0) sep = cmdRaw.indexOf(':');
    String v = cmdRaw.substring(sep + 1);
    v.trim();
    v.toUpperCase();
    uint8_t m = 0;
    if (v.length() == 0 || v == "ALL") {
      m = 0x0F;
    } else if (v == "NONE") {
      m = 0;
    } else {
      int start = 0;
      while (start < v.length()) {
        int comma = v.indexOf(',', start);
        String token = (comma < 0) ? v.substring(start) : v.substring(start, comma);
        token.trim();
        if (token == "ENC" || token == "ENCOSTO") m |= 0x01;
        else if (token == "ASS" || token == "ASSENTO") m |= 0x02;
        else if (token == "PER" || token == "PERNEIRA") m |= 0x04;
        else if (token == "TRE" || token == "TREND") m |= 0x08;
        if (comma < 0) break;
        start = comma + 1;
      }
    }
    encRequiredMask = m;
    Serial.print("[ENC_REQ] SET ENC="); Serial.print(encReqEncosto() ? 1 : 0);
    Serial.print(" ASS="); Serial.print(encReqAssento() ? 1 : 0);
    Serial.print(" PER="); Serial.print(encReqPerneira() ? 1 : 0);
    Serial.print(" TRE="); Serial.println(encReqTrend() ? 1 : 0);
    return;
  }
  if (cmd == "ENC_RESET") {
    noInterrupts();
    pulses_encosto = 0;
    pulses_assento = 0;
    pulses_perneira = 0;
    pulses_trend = 0;
    last_pulses_encosto = 0;
    last_pulses_assento = 0;
    last_pulses_perneira = 0;
    last_pulses_trend = 0;
    last_pulses_trend_travel = 0;
    interrupts();
    encoderMonResetCounters();
    Serial.println("[ENC] RESET");
    return;
  }

  if (cmd.startsWith("SET_ENC_VIRTUAL=")) {
    // Formato: SET_ENC_VIRTUAL=ENC,ASS,PER (ex: SET_ENC_VIRTUAL=100,200,300)
    String vals = cmd.substring(16);
    int firstComma = vals.indexOf(',');
    int lastComma = vals.lastIndexOf(',');
    if (firstComma > 0 && lastComma > firstComma) {
      incoder_virtual_encosto_service = vals.substring(0, firstComma).toInt();
      incoder_virtual_asento_service = vals.substring(firstComma + 1, lastComma).toInt();
      incoder_virtual_perneira_service = vals.substring(lastComma + 1).toInt();
      Serial.println("[ENC] Posicoes virtuais alteradas manualmente.");
      Serial.print("ENC="); Serial.print(incoder_virtual_encosto_service);
      Serial.print(" ASS="); Serial.print(incoder_virtual_asento_service);
      Serial.print(" PER="); Serial.println(incoder_virtual_perneira_service);
    }
    return;
  }

  if (cmd == "SAVE_ENC") {
    saveMotorTravelPreferences(true);
    Serial.println("[ENC] Salvamento manual realizado (force=true).");
    return;
  }

  if (cmd == "ENC_MON_ON") {
    encoderMonEnabled = true;
    encoderMonNextMs = 0;
    encoderMonResetCounters();
    Serial.println("[ENC_MON] ON");
    return;
  }
  if (cmd == "ENC_MON_OFF") {
    encoderMonEnabled = false;
    Serial.println("[ENC_MON] OFF");
    return;
  }

  if (cmd.startsWith("SET_MAINT_KM=")) {
    maintenanceLimitKm = cmd.substring(13).toFloat();
    Serial.print("[MAINT] Limite alterado para: ");
    Serial.print(maintenanceLimitKm);
    Serial.println(" Km");
    return;
  }

  if (cmd == "MAINT_RESET") {
    maintenanceRequired = false;
    Serial.println("[MAINT] Status de manutencao resetado.");
    return;
  }

  if (cmd == "PCF8574_INT") {
    Serial.print("[PCF8574] INT_PIN=");
    Serial.print(PCF8574_INT_PIN);
    Serial.print(" LEVEL=");
    if (PCF8574_INT_PIN >= 0) {
      Serial.println(digitalRead(static_cast<uint8_t>(PCF8574_INT_PIN)) == HIGH ? "1" : "0");
    } else {
      Serial.println("NA");
    }
    Serial.print("[PCF8574] INT_COUNT=");
    Serial.println(static_cast<uint32_t>(pcf8574InterruptCount));
    Serial.print("[PCF8574] INT_PENDING=");
    Serial.println(pcf8574InterruptPending ? "1" : "0");
    bool okRead = false;
    uint8_t in = pcf8574ReadByte(static_cast<uint8_t>(PCF8574_ADDRESS), &okRead);
    Serial.print("[PCF8574] IN=0x");
    Serial.println(in, HEX);
    if (pcf8574InterruptPending) {
      pcf8574InterruptPending = false;
    }
    return;
  }

  // ===== COMANDOS WIFI (funcionam mesmo com cadeira bloqueada) =====
  if (cmd == "WIFI_RESET") {
    enviarBLE("WIFI:RESETTING");
    resetaConfiguracoesWifi();
    return;
  }
  
  if (cmd == "WIFI_STATUS") {
    if (WiFi.status() == WL_CONNECTED) {
      enviarBLE("WIFI:CONNECTED:" + WiFi.SSID());
    } else {
      enviarBLE("WIFI:DISCONNECTED");
    }
    return;
  }
  
  if (cmd == "WIFI_IP") {
    if (WiFi.status() == WL_CONNECTED) {
      enviarBLE("WIFI_IP:" + WiFi.localIP().toString());
    } else {
      enviarBLE("WIFI_IP:NONE");
    }
    return;
  }

  if (cmd == "TREND_TEST") {
    if (Rele_TREND_SOBE < 0 && Rele_TREND_DESCE < 0) {
      enviarBLE("TREND_TEST:NA");
      return;
    }
    enviarBLE("TREND_TEST:START");
    mqttEnqueuePublish(MQTT_TOPIC_BASE + "tx_cmd", "TREND_TEST", false);

    if (Rele_TREND_SOBE >= 0) setOutputPin(Rele_TREND_SOBE, false, "TREND_TEST");
    if (Rele_TREND_DESCE >= 0) setOutputPin(Rele_TREND_DESCE, false, "TREND_TEST");
    estado_trend_sobe = false;
    estado_trend_desce = false;
    mqttStatusDirty = true;
    delay(200);

    if (Rele_TREND_SOBE >= 0) {
      estado_trend_sobe = true;
      ultimoComandoTrendSobe = millis();
      setOutputPin(Rele_TREND_SOBE, true, "TREND_TEST");
      mqttStatusDirty = true;
      delay(800);
      setOutputPin(Rele_TREND_SOBE, false, "TREND_TEST");
      estado_trend_sobe = false;
      mqttStatusDirty = true;
      delay(200);
    }

    if (Rele_TREND_DESCE >= 0) {
      estado_trend_desce = true;
      ultimoComandoTrendDesce = millis();
      setOutputPin(Rele_TREND_DESCE, true, "TREND_TEST");
      mqttStatusDirty = true;
      delay(800);
      setOutputPin(Rele_TREND_DESCE, false, "TREND_TEST");
      estado_trend_desce = false;
      mqttStatusDirty = true;
      delay(200);
    }

    enviarBLE("TREND_TEST:DONE");
    mqttEnqueuePublish(MQTT_TOPIC_BASE + "tx_cmd", "TREND_TEST_DONE", false);
    return;
  }

  if (cmd == "TREND_DUMP") {
    trendDebugDump("CMD");
    enviarBLE("TREND_DUMP:OK");
    return;
  }

  if (cmd.startsWith("TREND_DEBUG=") || cmd.startsWith("TREND_DEBUG:")) {
    int sep = cmd.indexOf('=');
    if (sep < 0) sep = cmd.indexOf(':');
    String v = cmd.substring(sep + 1);
    v.trim();
    v.toUpperCase();
    if (v == "1" || v == "ON" || v == "TRUE") {
      trendDebugEnabled = true;
    } else if (v == "0" || v == "OFF" || v == "FALSE") {
      trendDebugEnabled = false;
    }
    trendDebugNextMs = 0;
    enviarBLE(String("TREND_DEBUG:") + (trendDebugEnabled ? "ON" : "OFF"));
    trendDebugDump("DEBUG");
    return;
  }
  
  if (cmd == "WIFI_CONFIG") {
    // Inicia portal de configuraÃ§Ã£o WiFi
    enviarBLE("WIFI:CONFIG_MODE");
    Serial.println("Iniciando portal de configuraÃ§Ã£o WiFi...");
    
    // Desconecta do WiFi atual
    WiFi.disconnect();
    
    // Inicia portal de configuraÃ§Ã£o (bloqueia por atÃ© 3 minutos)
    wifiManager.setConfigPortalTimeout(TIMEOUT_CONFIG_WIFI);
    if (wifiManager.startConfigPortal(NOME_DISPOSITIVO.c_str(), SENHA_AP)) {
      enviarBLE("WIFI:CONFIG_OK");
    } else {
      enviarBLE("WIFI:CONFIG_TIMEOUT");
    }
    return;
  }

  // ===== VERIFICA SE CADEIRA ESTÃ BLOQUEADA =====
  if (!cadeiraHabilitada && cmd != "STATUS" && cmd != "AT_SEG") {
    enviarBLE("ERRO:BLOQUEADA");
    Serial.println("Cadeira bloqueada - comando ignorado");
    if (origin && String(origin) == "MQTT") {
      mqttEnqueuePublish(MQTT_TOPIC_BASE + "tx_cmd", "BLOCKED", false);
    }
    return;
  }

  // ===== COMANDOS DE CONTROLE DA CADEIRA =====
  if (cmd == "RF") {
    // Liga/desliga refletor
    estado_r = !estado_r;
    setOutputPin(Rele_refletor, estado_r, "RF");
    enviarBLE(estado_r ? "RF:ON" : "RF:OFF");
    Serial.println("Refletor alternado via BLE");
  }
  else if (cmd == "SE") {
    // Encosto sobe (sentar) - Dead man's switch
    if (!calibrationInProgress && !ignoreLimitLocks && incoder_virtual_encosto_service <= 0) {
      enviarBLE("SE:LIMIT");
      Serial.println("[BLOCK] SE bloqueado (posicao minima)");
      if (origin && String(origin) == "MQTT") {
        mqttEnqueuePublish(MQTT_TOPIC_BASE + "tx_cmd", "SE:LIMIT", false);
      }
      return;
    }
    if (ignoreLimitLocks || !trava_bt_SE) {
      ultimoComandoSE = millis();
      if (!estado_se) {
        reflectorPreMove(1, "SE");
        estado_se = true;
        setOutputPin(Rele_SE, true, "SE");
        faz_bt_seg = 1;
        enviarBLE("SE:ON");
        iniciaTimerMotor();
      } else {
        sendKeep("SE:KEEP", lastKeepSEMs);
      }
    } else {
      enviarBLE("SE:LIMIT");
      Serial.println("[BLOCK] SE bloqueado (limite)");
      if (origin && String(origin) == "MQTT") {
        mqttEnqueuePublish(MQTT_TOPIC_BASE + "tx_cmd", "SE:LIMIT", false);
      }
    }
  }
  else if (cmd == "DE") {
    // Encosto desce (deitar) - Dead man's switch
    if (!calibrationInProgress && !ignoreLimitLocks) {
      int encMax = effectiveEncostoMax();
      if (encMax > 0 && incoder_virtual_encosto_service >= encMax) {
        enviarBLE("DE:LIMIT");
        Serial.println("[BLOCK] DE bloqueado (posicao maxima)");
        if (origin && String(origin) == "MQTT") {
          mqttEnqueuePublish(MQTT_TOPIC_BASE + "tx_cmd", "DE:LIMIT", false);
        }
        return;
      }
    }
    if (ignoreLimitLocks || !trava_bt_DE) {
      ultimoComandoDE = millis();
      if (!estado_de) {
        reflectorPreMove(1, "DE");
        estado_de = true;
        setOutputPin(Rele_DE, true, "DE");
        faz_bt_seg = 1;
        enviarBLE("DE:ON");
        iniciaTimerMotor();
      } else {
        sendKeep("DE:KEEP", lastKeepDEMs);
      }
    } else {
      enviarBLE("DE:LIMIT");
      Serial.println("[BLOCK] DE bloqueado (limite)");
      if (origin && String(origin) == "MQTT") {
        mqttEnqueuePublish(MQTT_TOPIC_BASE + "tx_cmd", "DE:LIMIT", false);
      }
    }
  }
  else if (cmd == "SA") {
    // Assento sobe - Dead man's switch
    if (!calibrationInProgress && !ignoreLimitLocks) {
      int assMax = effectiveAssentoMax();
      if (assMax > 0 && incoder_virtual_asento_service >= assMax) {
        enviarBLE("SA:LIMIT");
        Serial.println("[BLOCK] SA bloqueado (posicao maxima)");
        if (origin && String(origin) == "MQTT") {
          mqttEnqueuePublish(MQTT_TOPIC_BASE + "tx_cmd", "SA:LIMIT", false);
        }
        return;
      }
    }
    if (ignoreLimitLocks || !trava_bt_SA) {
      ultimoComandoSA = millis();
      if (!estado_sa) {
        reflectorPreMove(1, "SA");
        estado_sa = true;
        setOutputPin(Rele_SA, true, "SA");
        faz_bt_seg = 1;
        enviarBLE("SA:ON");
        iniciaTimerMotor();
      } else {
        sendKeep("SA:KEEP", lastKeepSAMs);
      }
    } else {
      enviarBLE("SA:LIMIT");
      Serial.println("[BLOCK] SA bloqueado (limite)");
      if (origin && String(origin) == "MQTT") {
        mqttEnqueuePublish(MQTT_TOPIC_BASE + "tx_cmd", "SA:LIMIT", false);
      }
    }
  }
  else if (cmd == "DA") {
    // Assento desce - Dead man's switch
    if (!calibrationInProgress && !ignoreLimitLocks && incoder_virtual_asento_service <= 0) {
      enviarBLE("DA:LIMIT");
      Serial.println("[BLOCK] DA bloqueado (posicao minima)");
      if (origin && String(origin) == "MQTT") {
        mqttEnqueuePublish(MQTT_TOPIC_BASE + "tx_cmd", "DA:LIMIT", false);
      }
      return;
    }
    if (ignoreLimitLocks || !trava_bt_DA) {
      ultimoComandoDA = millis();
      if (!estado_da) {
        reflectorPreMove(1, "DA");
        estado_da = true;
        setOutputPin(Rele_DA, true, "DA");
        faz_bt_seg = 1;
        enviarBLE("DA:ON");
        iniciaTimerMotor();
      } else {
        sendKeep("DA:KEEP", lastKeepDAMs);
      }
    } else {
      enviarBLE("DA:LIMIT");
      Serial.println("[BLOCK] DA bloqueado (limite)");
      if (origin && String(origin) == "MQTT") {
        mqttEnqueuePublish(MQTT_TOPIC_BASE + "tx_cmd", "DA:LIMIT", false);
      }
    }
  }
  else if (cmd == "SP") {
    // Pernas sobem - Dead man's switch
    if (isGavetaAberta()) {
      Serial.println("[GAVETA] Aberta - bloqueando SP");
      enviarBLE("GAVETA:OPEN");
      enviarBLE("SP:GAVETA");
      mqttEnqueuePublish(MQTT_TOPIC_BASE + "tx_cmd", "GAVETA_OPEN", false);
      if (origin && String(origin) == "MQTT") {
        mqttEnqueuePublish(MQTT_TOPIC_BASE + "tx_cmd", "SP:GAVETA", false);
      }
      mqttStatusDirty = true;
      return;
    }
    if (!calibrationInProgress && !ignoreLimitLocks) {
      int perMax = effectivePerneiraMax();
      if (perMax > 0 && incoder_virtual_perneira_service >= perMax) {
        enviarBLE("SP:LIMIT");
        Serial.println("[BLOCK] SP bloqueado (posicao maxima)");
        if (origin && String(origin) == "MQTT") {
          mqttEnqueuePublish(MQTT_TOPIC_BASE + "tx_cmd", "SP:LIMIT", false);
        }
        return;
      }
    }
    if (ignoreLimitLocks || !trava_bt_SP) {
      ultimoComandoSP = millis();
      if (!estado_sp) {
        reflectorPreMove(1, "SP");
        estado_sp = true;
        setOutputPin(Rele_SP, true, "SP");
        faz_bt_seg = 1;
        enviarBLE("SP:ON");
        iniciaTimerMotor();
      } else {
        sendKeep("SP:KEEP", lastKeepSPMs);
      }
    } else {
      enviarBLE("SP:LIMIT");
      Serial.println("[BLOCK] SP bloqueado (limite)");
      if (origin && String(origin) == "MQTT") {
        mqttEnqueuePublish(MQTT_TOPIC_BASE + "tx_cmd", "SP:LIMIT", false);
      }
    }
  }
  else if (cmd == "DP") {
    // Pernas descem - Dead man's switch
    if (isGavetaAberta()) {
      Serial.println("[GAVETA] Aberta - bloqueando DP");
      enviarBLE("GAVETA:OPEN");
      enviarBLE("DP:GAVETA");
      mqttEnqueuePublish(MQTT_TOPIC_BASE + "tx_cmd", "GAVETA_OPEN", false);
      if (origin && String(origin) == "MQTT") {
        mqttEnqueuePublish(MQTT_TOPIC_BASE + "tx_cmd", "DP:GAVETA", false);
      }
      mqttStatusDirty = true;
      return;
    }
    if (!calibrationInProgress && !ignoreLimitLocks && incoder_virtual_perneira_service <= 0) {
      enviarBLE("DP:LIMIT");
      Serial.println("[BLOCK] DP bloqueado (posicao minima)");
      if (origin && String(origin) == "MQTT") {
        mqttEnqueuePublish(MQTT_TOPIC_BASE + "tx_cmd", "DP:LIMIT", false);
      }
      return;
    }
    if (ignoreLimitLocks || !trava_bt_DP) {
      ultimoComandoDP = millis();
      if (!estado_dp) {
        reflectorPreMove(1, "DP");
        estado_dp = true;
        setOutputPin(Rele_DP, true, "DP");
        faz_bt_seg = 1;
        enviarBLE("DP:ON");
        iniciaTimerMotor();
      } else {
        sendKeep("DP:KEEP", lastKeepDPMs);
      }
    } else {
      enviarBLE("DP:LIMIT");
      Serial.println("[BLOCK] DP bloqueado (limite)");
      if (origin && String(origin) == "MQTT") {
        mqttEnqueuePublish(MQTT_TOPIC_BASE + "tx_cmd", "DP:LIMIT", false);
      }
    }
  }
  else if (cmd == "TS") {
    if (Rele_TREND_SOBE < 0) {
      enviarBLE("TS:NA");
      return;
    }
    lastTrendCmdMs = millis();
    ultimoComandoTrendSobe = lastTrendCmdMs;
    trendRemoteActive = true;
    if (!estado_trend_sobe) {
      reflectorPreMove(1, "TS");
      estado_trend_sobe = true;
      estado_trend_desce = false;
      if (Rele_TREND_DESCE >= 0) setOutputPin(Rele_TREND_DESCE, false, "TS");
      setOutputPin(Rele_TREND_SOBE, true, "TS");
      faz_bt_seg = 1;
      enviarBLE("TS:ON");
      iniciaTimerMotor();
    } else {
      sendKeep("TS:KEEP", lastKeepTSMs);
    }
  }
  else if (cmd == "TD") {
    if (Rele_TREND_DESCE < 0) {
      enviarBLE("TD:NA");
      return;
    }
    lastTrendCmdMs = millis();
    ultimoComandoTrendDesce = lastTrendCmdMs;
    trendRemoteActive = true;
    if (!estado_trend_desce) {
      reflectorPreMove(1, "TD");
      estado_trend_desce = true;
      estado_trend_sobe = false;
      if (Rele_TREND_SOBE >= 0) setOutputPin(Rele_TREND_SOBE, false, "TD");
      setOutputPin(Rele_TREND_DESCE, true, "TD");
      faz_bt_seg = 1;
      enviarBLE("TD:ON");
      iniciaTimerMotor();
    } else {
      sendKeep("TD:KEEP", lastKeepTDMs);
    }
  }
  else if (cmd == "T0") {
    if (Rele_TREND_SOBE >= 0) setOutputPin(Rele_TREND_SOBE, false, "T0");
    if (Rele_TREND_DESCE >= 0) setOutputPin(Rele_TREND_DESCE, false, "T0");
    estado_trend_sobe = false;
    estado_trend_desce = false;
    trendRemoteActive = false;
    faz_bt_seg = 0;
    mqttStatusDirty = true;
    enviarBLE("T0:OK");
  }
  else if (cmd == "VZ") {
    // PosiÃ§Ã£o ginecolÃ³gica
    enviarBLE("VZ:EXEC");
    cont = 1;
    executa_vz();
    enviarBLE("VZ:DONE");
  }
  else if (cmd == "PT") {
    // PosiÃ§Ã£o de parto
    enviarBLE("PT:EXEC");
    cont13 = 1;
    executa_pt();
    enviarBLE("PT:DONE");
  }
  else if (cmd == "M1:GET") {
    String pos = String("M1:POS:") +
                 String(incoder_virtual_encosto_M1) + "," +
                 String(incoder_virtual_asento_M1) + "," +
                 String(incoder_virtual_perneira_M1);
    enviarBLE(pos);
    if (origin && String(origin) == "MQTT") {
      mqttEnqueuePublish(MQTT_TOPIC_BASE + "tx_cmd", pos, false);
    }
  }
  else if (cmd.startsWith("M1:SET:") || cmd.startsWith("M1:SET=")) {
    int start = cmd.indexOf(':', 3);
    if (start >= 0) {
      start++;
    } else {
      start = cmd.indexOf('=');
      if (start >= 0) start++;
    }
    if (start < 0 || start >= cmd.length()) {
      enviarBLE("M1:SET:ERR");
      return;
    }
    String payload = cmd.substring(start);
    int c1 = payload.indexOf(',');
    int c2 = (c1 >= 0) ? payload.indexOf(',', c1 + 1) : -1;
    if (c1 < 0 || c2 < 0) {
      enviarBLE("M1:SET:ERR");
      return;
    }
    int back = payload.substring(0, c1).toInt();
    int seat = payload.substring(c1 + 1, c2).toInt();
    int leg = payload.substring(c2 + 1).toInt();

    incoder_virtual_encosto_M1 = back;
    incoder_virtual_asento_M1 = seat;
    incoder_virtual_perneira_M1 = leg;

    preferences.begin("encoder_encosto", false);
    preferences.putInt("encoder_encosto", incoder_virtual_encosto_M1);
    preferences.end();

    preferences.begin("encoder_asento", false);
    preferences.putInt("encoder_asento", incoder_virtual_asento_M1);
    preferences.end();

    preferences.begin("encoder_pern", false);
    preferences.putInt("encoder_perneira", incoder_virtual_perneira_M1);
    preferences.end();

    mqttStatusDirty = true;
    enviarBLE("M1:SET:OK");

    String pos = String("M1:POS:") +
                 String(incoder_virtual_encosto_M1) + "," +
                 String(incoder_virtual_asento_M1) + "," +
                 String(incoder_virtual_perneira_M1);
    enviarBLE(pos);
    if (origin && String(origin) == "MQTT") {
      mqttEnqueuePublish(MQTT_TOPIC_BASE + "tx_cmd", "M1:SET:OK", false);
      mqttEnqueuePublish(MQTT_TOPIC_BASE + "tx_cmd", pos, false);
    }
  }
  else if (cmd == "M1") {
    // PosiÃ§Ã£o de memÃ³ria 1
    enviarBLE("M1:EXEC");
    executa_M1_bluetooth();
    enviarBLE("M1:DONE");
  }
  else if (cmd == "AT_SEG" || cmd == "STOP") {
    // Parada de emergÃªncia
    AT_SEG();
    paraTimerMotor();
    enviarBLE("AT_SEG:OK");
  }
  else if (cmd == "STATUS") {
    // Retorna status completo
    enviaStatusBluetooth();
    bleSendAllSnapshotExtras();
  }
  else if (cmd == "ENC_SNAPSHOT" || cmd == "ENC_INFO") {
    int encMaxOut = fim_encosto_encoder;
    int assMaxOut = fim_asento_encoder;
    int perMaxOut = fim_perneira_encoder;
    if (!calibrationInProgress && !ignoreLimitLocks) {
      if (encMaxOut > LIMIT_STOP_MARGIN_PULSES) encMaxOut -= LIMIT_STOP_MARGIN_PULSES;
      if (assMaxOut > LIMIT_STOP_MARGIN_PULSES) assMaxOut -= LIMIT_STOP_MARGIN_PULSES;
      if (perMaxOut > LIMIT_STOP_MARGIN_PULSES) perMaxOut -= LIMIT_STOP_MARGIN_PULSES;
    }
    if (encMaxOut < 0) encMaxOut = 0;
    if (assMaxOut < 0) assMaxOut = 0;
    if (perMaxOut < 0) perMaxOut = 0;

    String msg = String("ENC_SNAPSHOT:POS:") +
                 String(incoder_virtual_encosto_service) + "," +
                 String(incoder_virtual_asento_service) + "," +
                 String(incoder_virtual_perneira_service) +
                 ":MIN:0,0,0:MAX:" +
                 String(encMaxOut) + "," + String(assMaxOut) + "," + String(perMaxOut) +
                 ":PULSE:" +
                 String(static_cast<uint32_t>(pulses_encosto)) + "," +
                 String(static_cast<uint32_t>(pulses_assento)) + "," +
                 String(static_cast<uint32_t>(pulses_perneira)) +
                 ":M1:" +
                 String(incoder_virtual_encosto_M1) + "," +
                 String(incoder_virtual_asento_M1) + "," +
                 String(incoder_virtual_perneira_M1);

    Serial.println(msg);
    if (bleClienteConectado) {
      enviarBLE(msg);
    }
    if (origin && String(origin) == "MQTT") {
      mqttEnqueuePublish(MQTT_TOPIC_BASE + "tx_cmd", msg, false);
    }
  }
  else if (cmd == "HORIMETRO") {
    // Retorna horÃ­metro
    enviarBLE("HORIMETRO:" + String(horimetro, 2));
  }
  else if (cmd == "PING") {
    // Teste de conexÃ£o
    enviarBLE("PONG");
  }
  else if (cmd == "CAL_RESET") {
    resetCalibracao();
    enviarBLE("CAL:RESET");
    supabaseLogUsage("CAL_RESET");
  }
  else if (cmd == "CAL_START") {
    supabaseLogUsage("CAL_START");
    executaCalibracao();
    enviarBLE("CAL:DONE");
    supabaseLogUsage("CAL_DONE");
  }
  else if (cmd == "M1_LOAD_FROM_DB") {
    if (supabaseLoadMemoryPositionFromDb(1)) {
      enviarBLE("M1:DB_LOADED");
    }
  }
  else if (cmd.startsWith("M_LOAD_FROM_DB=") || cmd.startsWith("M_LOAD_FROM_DB:")) {
    int sep = cmd.indexOf('=');
    if (sep < 0) sep = cmd.indexOf(':');
    int slot = cmd.substring(sep + 1).toInt();
    supabaseLoadMemoryPositionFromDb(slot);
  }
  else if (cmd == "TRAVEL_STATUS") {
    double enc_m = (static_cast<double>(motorTravelPulsesEncosto) * mmPerPulseEncosto) / 1000.0;
    double ass_m = (static_cast<double>(motorTravelPulsesAssento) * mmPerPulseAssento) / 1000.0;
    double per_m = (static_cast<double>(motorTravelPulsesPerneira) * mmPerPulsePerneira) / 1000.0;
    double tre_m = (static_cast<double>(motorTravelPulsesTrend) * mmPerPulseTrend) / 1000.0;
    Serial.print("[TRAVEL] PULSOS ENC=");
    Serial.print(static_cast<uint32_t>(motorTravelPulsesEncosto));
    Serial.print(" ASS=");
    Serial.print(static_cast<uint32_t>(motorTravelPulsesAssento));
    Serial.print(" PER=");
    Serial.println(static_cast<uint32_t>(motorTravelPulsesPerneira));
    Serial.print("[TRAVEL] PULSOS TREND=");
    Serial.println(static_cast<uint32_t>(motorTravelPulsesTrend));
    Serial.print("[TRAVEL] METROS ENC=");
    Serial.print(enc_m, 3);
    Serial.print(" ASS=");
    Serial.print(ass_m, 3);
    Serial.print(" PER=");
    Serial.print(per_m, 3);
    Serial.print(" TRE=");
    Serial.println(tre_m, 3);
    enviarBLE("TRAVEL:OK");
  }
  else if (cmd == "TRAVEL_RESET") {
    motorTravelPulsesEncosto = 0;
    motorTravelPulsesAssento = 0;
    motorTravelPulsesPerneira = 0;
    motorTravelPulsesTrend = 0;
    motorTravelUnsavedEncosto = 0;
    motorTravelUnsavedAssento = 0;
    motorTravelUnsavedPerneira = 0;
    motorTravelUnsavedTrend = 0;
    saveMotorTravelPreferences(true);
    supabaseLogUsage("TRAVEL_RESET");
    enviarBLE("TRAVEL:RESET");
  }
  else if (cmd == "RESET_TUDO" || cmd == "FACTORY_RESET" || cmd == "RESET_PRIMEIRA_VEZ") {
    resetParaPrimeiraVez();
  }
  else if (cmd == "TRAVEL_SEND") {
    saveMotorTravelPreferences(true);
    if (supabaseUpsertMotorTravel()) {
      supabaseLogUsage("MOTOR_TRAVEL_SENT");
      enviarBLE("TRAVEL:SENT");
    } else {
      enviarBLE("TRAVEL:SENT_FAIL");
    }
  }
  else if (cmd.startsWith("MM_PER_PULSE_ENC=") || cmd.startsWith("MM_PER_PULSE_ENC:")) {
    int sep = cmd.indexOf('=');
    if (sep < 0) sep = cmd.indexOf(':');
    mmPerPulseEncosto = cmd.substring(sep + 1).toFloat();
    saveMotorTravelPreferences(true);
    enviarBLE("MM_PER_PULSE_ENC:OK");
  }
  else if (cmd.startsWith("MM_PER_PULSE_ASS=") || cmd.startsWith("MM_PER_PULSE_ASS:")) {
    int sep = cmd.indexOf('=');
    if (sep < 0) sep = cmd.indexOf(':');
    mmPerPulseAssento = cmd.substring(sep + 1).toFloat();
    saveMotorTravelPreferences(true);
    enviarBLE("MM_PER_PULSE_ASS:OK");
  }
  else if (cmd.startsWith("MM_PER_PULSE_PER=") || cmd.startsWith("MM_PER_PULSE_PER:")) {
    int sep = cmd.indexOf('=');
    if (sep < 0) sep = cmd.indexOf(':');
    mmPerPulsePerneira = cmd.substring(sep + 1).toFloat();
    saveMotorTravelPreferences(true);
    enviarBLE("MM_PER_PULSE_PER:OK");
  }
  else if (cmd.startsWith("MM_PER_PULSE_TREND=") || cmd.startsWith("MM_PER_PULSE_TREND:")) {
    int sep = cmd.indexOf('=');
    if (sep < 0) sep = cmd.indexOf(':');
    mmPerPulseTrend = cmd.substring(sep + 1).toFloat();
    saveMotorTravelPreferences(true);
    enviarBLE("MM_PER_PULSE_TREND:OK");
  }
  else if (cmd == "ENC_DEBUG_ON") {
    encoderPulseDebug = true;
    enviarBLE("ENC_DEBUG:ON");
  }
  else if (cmd == "ENC_DEBUG_OFF") {
    encoderPulseDebug = false;
    enviarBLE("ENC_DEBUG:OFF");
  }
  else if (cmd == "TREND_PULSE_ON") {
    pulsePrintEnabled = true;
    pulsePrintLastTrend = static_cast<uint32_t>(pulses_trend);
    pulsePrintLastEncosto = static_cast<uint32_t>(pulses_encosto);
    pulsePrintLastAssento = static_cast<uint32_t>(pulses_assento);
    pulsePrintLastPerneira = static_cast<uint32_t>(pulses_perneira);
    Serial.println("[PULSE] ON");
    enviarBLE("TREND_PULSE:ON");
  }
  else if (cmd == "TREND_PULSE_OFF") {
    pulsePrintEnabled = false;
    Serial.println("[PULSE] OFF");
    enviarBLE("TREND_PULSE:OFF");
  }
  else if (cmd == "TREND_PULSE_STATUS") {
    Serial.print("[PULSE] ");
    Serial.println(pulsePrintEnabled ? "ON" : "OFF");
    enviarBLE(pulsePrintEnabled ? "TREND_PULSE:ON" : "TREND_PULSE:OFF");
  }
  else if (cmd == "PULSE_ON") {
    pulsePrintEnabled = true;
    pulsePrintLastTrend = static_cast<uint32_t>(pulses_trend);
    pulsePrintLastEncosto = static_cast<uint32_t>(pulses_encosto);
    pulsePrintLastAssento = static_cast<uint32_t>(pulses_assento);
    pulsePrintLastPerneira = static_cast<uint32_t>(pulses_perneira);
    Serial.println("[PULSE] ON");
    enviarBLE("PULSE:ON");
  }
  else if (cmd == "PULSE_OFF") {
    pulsePrintEnabled = false;
    Serial.println("[PULSE] OFF");
    enviarBLE("PULSE:OFF");
  }
  else if (cmd == "PULSE_STATUS") {
    Serial.print("[PULSE] ");
    Serial.print(pulsePrintEnabled ? "ON" : "OFF");
    Serial.print(" | M1(TREND)=");
    Serial.print(static_cast<uint32_t>(pulses_trend));
    Serial.print(" M2(ENC)=");
    Serial.print(static_cast<uint32_t>(pulses_encosto));
    Serial.print(" M3(ASS)=");
    Serial.print(static_cast<uint32_t>(pulses_assento));
    Serial.print(" M4(PER)=");
    Serial.println(static_cast<uint32_t>(pulses_perneira));
    enviarBLE(pulsePrintEnabled ? "PULSE:ON" : "PULSE:OFF");
  }
  else {
    enviarBLE("ERRO:CMD_INVALIDO");
  }
}

// Envia status completo via BLE em formato JSON
void enviaStatusBluetooth() {
  StaticJsonDocument<512> doc;
  const size_t statusJsonMaxLen = 180;

  doc["enabled"] = cadeiraHabilitada;
  doc["manutencao"] = manutencaoNecessaria;
  doc["hourMeter"] = horimetro;
  doc["wifi"] = (WiFi.status() == WL_CONNECTED);

  doc["refletor"] = (Rele_refletor >= 0) ? relayIsOn(Rele_refletor) : false;

  bool backUpOn = (Rele_SE >= 0) ? relayIsOn(Rele_SE) : false;
  bool backDownOn = (Rele_DE >= 0) ? relayIsOn(Rele_DE) : false;
  bool seatUpOn = (Rele_SA >= 0) ? relayIsOn(Rele_SA) : false;
  bool seatDownOn = (Rele_DA >= 0) ? relayIsOn(Rele_DA) : false;
  bool upperLegsOn = (Rele_SP >= 0) ? relayIsOn(Rele_SP) : false;
  bool lowerLegsOn = (Rele_DP >= 0) ? relayIsOn(Rele_DP) : false;
  if (backUpOn) doc["backUpOn"] = true;
  if (backDownOn) doc["backDownOn"] = true;
  if (seatUpOn) doc["seatUpOn"] = true;
  if (seatDownOn) doc["seatDownOn"] = true;
  if (upperLegsOn) doc["upperLegsOn"] = true;
  if (lowerLegsOn) doc["lowerLegsOn"] = true;

  doc["encosto_pos"] = incoder_virtual_encosto_service;
  doc["assento_pos"] = incoder_virtual_asento_service;
  doc["perneira_pos"] = incoder_virtual_perneira_service;
  int encMaxOut = fim_encosto_encoder;
  int assMaxOut = fim_asento_encoder;
  int perMaxOut = fim_perneira_encoder;
  if (!calibrationInProgress && !ignoreLimitLocks) {
    if (encMaxOut > LIMIT_STOP_MARGIN_PULSES) encMaxOut -= LIMIT_STOP_MARGIN_PULSES;
    if (assMaxOut > LIMIT_STOP_MARGIN_PULSES) assMaxOut -= LIMIT_STOP_MARGIN_PULSES;
    if (perMaxOut > LIMIT_STOP_MARGIN_PULSES) perMaxOut -= LIMIT_STOP_MARGIN_PULSES;
  }
  if (encMaxOut < 0) encMaxOut = 0;
  if (assMaxOut < 0) assMaxOut = 0;
  if (perMaxOut < 0) perMaxOut = 0;
  doc["encosto_max"] = encMaxOut;
  doc["assento_max"] = assMaxOut;
  doc["perneira_max"] = perMaxOut;
  if (isGavetaAbertaRaw()) doc["gavetaOpen"] = true;
  if (ignoreGavetaLock) doc["gavetaLockIgnored"] = true;
  if ((cont == 1) || vzInicialEmAndamento) doc["isMovingToGineco"] = true;
  if (cont13 == 1) doc["isMovingToParto"] = true;

  if (trava_bt_SE) doc["se_limit"] = true;
  if (trava_bt_DE) doc["de_limit"] = true;
  if (trava_bt_SA) doc["sa_limit"] = true;
  if (trava_bt_DA) doc["da_limit"] = true;
  if (trava_bt_SP) doc["sp_limit"] = true;
  if (trava_bt_DP) doc["dp_limit"] = true;

  String output;
  serializeJson(doc, output);
  if (output.length() > statusJsonMaxLen) {
    StaticJsonDocument<256> d2;
    d2["enabled"] = cadeiraHabilitada;
    d2["manutencao"] = manutencaoNecessaria;
    d2["hourMeter"] = horimetro;
    d2["wifi"] = (WiFi.status() == WL_CONNECTED);
    d2["refletor"] = (Rele_refletor >= 0) ? relayIsOn(Rele_refletor) : false;
    d2["encosto_pos"] = incoder_virtual_encosto_service;
    d2["assento_pos"] = incoder_virtual_asento_service;
    d2["perneira_pos"] = incoder_virtual_perneira_service;
    d2["encosto_max"] = encMaxOut;
    d2["assento_max"] = assMaxOut;
    d2["perneira_max"] = perMaxOut;
    output = "";
    serializeJson(d2, output);
  }
  enviarBLE("STATUS:" + output);
}

static void bleSendAllSnapshotExtras() {
#if HAS_BLE
  if (!bleClienteConectado || pCharacteristicTX == NULL) {
    return;
  }
  String m1pos = String("M1:POS:") +
                 String(incoder_virtual_encosto_M1) + "," +
                 String(incoder_virtual_asento_M1) + "," +
                 String(incoder_virtual_perneira_M1);
  enviarBLE(m1pos);

  int encMaxOut = fim_encosto_encoder;
  int assMaxOut = fim_asento_encoder;
  int perMaxOut = fim_perneira_encoder;
  if (!calibrationInProgress && !ignoreLimitLocks) {
    if (encMaxOut > LIMIT_STOP_MARGIN_PULSES) encMaxOut -= LIMIT_STOP_MARGIN_PULSES;
    if (assMaxOut > LIMIT_STOP_MARGIN_PULSES) assMaxOut -= LIMIT_STOP_MARGIN_PULSES;
    if (perMaxOut > LIMIT_STOP_MARGIN_PULSES) perMaxOut -= LIMIT_STOP_MARGIN_PULSES;
  }
  if (encMaxOut < 0) encMaxOut = 0;
  if (assMaxOut < 0) assMaxOut = 0;
  if (perMaxOut < 0) perMaxOut = 0;

  String limitsMsg = String("ENC_LIMITS:MIN:0,0,0:MAX:") +
                     String(encMaxOut) + "," + String(assMaxOut) + "," + String(perMaxOut);
  enviarBLE(limitsMsg);
#endif
}

static void bleStatusStreamTick() {
#if HAS_BLE
  if (!bleClienteConectado || pCharacteristicTX == NULL) {
    return;
  }

  uint32_t now = millis();
  static uint32_t lastPrintedFailCount = 0;
  static uint32_t lastFailPrintAtMs = 0;
  if (bleTxFailCount != lastPrintedFailCount) {
    if (lastFailPrintAtMs == 0 || (now - lastFailPrintAtMs) > 300) {
      Serial.print("[BLE_TX] FAIL rc=");
      Serial.print(bleTxLastErrRc);
      Serial.print(" count=");
      Serial.println(bleTxFailCount);
      lastPrintedFailCount = bleTxFailCount;
      lastFailPrintAtMs = now;
    }
  }
  if (blePendingHelloSync) {
    bool ready = bleSawRxSinceConnect || ((now - blePendingHelloSyncAtMs) > 1500);
    if (ready) {
      bool canSend = (blePendingHelloSyncLastSendMs == 0) || ((now - blePendingHelloSyncLastSendMs) > 1000);
      if (canSend) {
        enviaStatusBluetooth();
        blePendingHelloSyncLastSendMs = now;
        blePendingHelloSyncTries++;
        if (bleSawRxSinceConnect || blePendingHelloSyncTries >= 5) {
          blePendingHelloSync = false;
        }
      }
    }
  }
  static uint32_t lastSendMs = 0;
  static int lastBackPos = -1;
  static int lastSeatPos = -1;
  static int lastLegPos = -1;
  static bool lastBackUpOn = false;
  static bool lastBackDownOn = false;
  static bool lastSeatUpOn = false;
  static bool lastSeatDownOn = false;
  static bool lastLegUpOn = false;
  static bool lastLegDownOn = false;
  static bool lastTrendUpOn = false;
  static bool lastTrendDownOn = false;
  static bool lastMoving = false;

  bool backUpOn = (Rele_SE >= 0) ? relayIsOn(Rele_SE) : false;
  bool backDownOn = (Rele_DE >= 0) ? relayIsOn(Rele_DE) : false;
  bool seatUpOn = (Rele_SA >= 0) ? relayIsOn(Rele_SA) : false;
  bool seatDownOn = (Rele_DA >= 0) ? relayIsOn(Rele_DA) : false;
  bool legUpOn = (Rele_SP >= 0) ? relayIsOn(Rele_SP) : false;
  bool legDownOn = (Rele_DP >= 0) ? relayIsOn(Rele_DP) : false;
  bool trendUpOn = (Rele_TREND_SOBE >= 0) ? relayIsOn(Rele_TREND_SOBE) : false;
  bool trendDownOn = (Rele_TREND_DESCE >= 0) ? relayIsOn(Rele_TREND_DESCE) : false;

  bool moving = backUpOn || backDownOn || seatUpOn || seatDownOn || legUpOn || legDownOn || trendUpOn || trendDownOn ||
                (cont == 1) || (cont13 == 1) || vzInicialEmAndamento || faz_m1;

  uint32_t intervalMs = moving ? 400 : 1200;
  if ((now - lastSendMs) < intervalMs) {
    return;
  }

  int backPos = incoder_virtual_encosto_service;
  int seatPos = incoder_virtual_asento_service;
  int legPos = incoder_virtual_perneira_service;
  int encMaxOut = fim_encosto_encoder;
  int assMaxOut = fim_asento_encoder;
  int perMaxOut = fim_perneira_encoder;
  if (!calibrationInProgress && !ignoreLimitLocks) {
    if (encMaxOut > LIMIT_STOP_MARGIN_PULSES) encMaxOut -= LIMIT_STOP_MARGIN_PULSES;
    if (assMaxOut > LIMIT_STOP_MARGIN_PULSES) assMaxOut -= LIMIT_STOP_MARGIN_PULSES;
    if (perMaxOut > LIMIT_STOP_MARGIN_PULSES) perMaxOut -= LIMIT_STOP_MARGIN_PULSES;
  }
  if (encMaxOut < 0) encMaxOut = 0;
  if (assMaxOut < 0) assMaxOut = 0;
  if (perMaxOut < 0) perMaxOut = 0;

  bool changed = (backPos != lastBackPos) || (seatPos != lastSeatPos) || (legPos != lastLegPos) ||
                 (backUpOn != lastBackUpOn) || (backDownOn != lastBackDownOn) ||
                 (seatUpOn != lastSeatUpOn) || (seatDownOn != lastSeatDownOn) ||
                 (legUpOn != lastLegUpOn) || (legDownOn != lastLegDownOn) ||
                 (trendUpOn != lastTrendUpOn) || (trendDownOn != lastTrendDownOn) ||
                 (moving != lastMoving);
  if (!changed && (now - lastSendMs) < 5000) {
    return;
  }

  lastSendMs = now;
  lastBackPos = backPos;
  lastSeatPos = seatPos;
  lastLegPos = legPos;
  lastBackUpOn = backUpOn;
  lastBackDownOn = backDownOn;
  lastSeatUpOn = seatUpOn;
  lastSeatDownOn = seatDownOn;
  lastLegUpOn = legUpOn;
  lastLegDownOn = legDownOn;
  lastTrendUpOn = trendUpOn;
  lastTrendDownOn = trendDownOn;
  lastMoving = moving;

  StaticJsonDocument<384> doc;
  const size_t statusJsonMaxLen = 180;
  doc["refletor"] = (Rele_refletor >= 0) ? relayIsOn(Rele_refletor) : false;
  doc["encosto_pos"] = backPos;
  doc["assento_pos"] = seatPos;
  doc["perneira_pos"] = legPos;
  doc["encosto_max"] = encMaxOut;
  doc["assento_max"] = assMaxOut;
  doc["perneira_max"] = perMaxOut;

  if (backUpOn) doc["backUpOn"] = true;
  if (backDownOn) doc["backDownOn"] = true;
  if (seatUpOn) doc["seatUpOn"] = true;
  if (seatDownOn) doc["seatDownOn"] = true;
  if (legUpOn) doc["upperLegsOn"] = true;
  if (legDownOn) doc["lowerLegsOn"] = true;
  if (trendUpOn) doc["trendUpOn"] = true;
  if (trendDownOn) doc["trendDownOn"] = true;

  if (isGavetaAbertaRaw()) doc["gavetaOpen"] = true;
  if ((cont == 1) || vzInicialEmAndamento) doc["isMovingToGineco"] = true;
  if (cont13 == 1) doc["isMovingToParto"] = true;

  if (trava_bt_SE) doc["se_limit"] = true;
  if (trava_bt_DE) doc["de_limit"] = true;
  if (trava_bt_SA) doc["sa_limit"] = true;
  if (trava_bt_DA) doc["da_limit"] = true;
  if (trava_bt_SP) doc["sp_limit"] = true;
  if (trava_bt_DP) doc["dp_limit"] = true;

  String output;
  serializeJson(doc, output);
  if (output.length() > statusJsonMaxLen) {
    StaticJsonDocument<256> d2;
    d2["refletor"] = (Rele_refletor >= 0) ? relayIsOn(Rele_refletor) : false;
    d2["encosto_pos"] = backPos;
    d2["assento_pos"] = seatPos;
    d2["perneira_pos"] = legPos;
    d2["encosto_max"] = encMaxOut;
    d2["assento_max"] = assMaxOut;
    d2["perneira_max"] = perMaxOut;
    output = "";
    serializeJson(d2, output);
  }
  enviarBLEQuiet("STATUS:" + output);
#endif
}

// ========== HORÃMETRO ==========
void iniciaTimerMotor() {
  if (!motorLigado) {
    tempoInicioMotor = millis();
    motorLigado = true;
  }
}

void paraTimerMotor() {
  if (motorLigado) {
    totalMillisMotor += (millis() - tempoInicioMotor);
    motorLigado = false;
    
    // Converte para horas e salva
    horimetro = totalMillisMotor / 3600000.0;
    salvaHorimetro();
  }
}

void atualizaHorimetro() {
  // Verifica se algum motor está ligado (HIGH = ligado)
  bool algumMotorLigado = (digitalRead(Rele_SA) == HIGH) || (digitalRead(Rele_DA) == HIGH) ||
                          (digitalRead(Rele_SE) == HIGH) || (digitalRead(Rele_DE) == HIGH) ||
                          (digitalRead(Rele_SP) == HIGH) || relayIsOn(Rele_DP) ||
                          (Rele_TREND_SOBE >= 0 && digitalRead(Rele_TREND_SOBE) == HIGH) ||
                          (Rele_TREND_DESCE >= 0 && digitalRead(Rele_TREND_DESCE) == HIGH);
  
  if (algumMotorLigado && !motorLigado) {
    iniciaTimerMotor();
  } else if (!algumMotorLigado && motorLigado) {
    paraTimerMotor();
  }
}

void salvaHorimetro() {
  preferences.begin("horimetro", false);
  preferences.putFloat("hours", horimetro);
  preferences.putULong("millis", totalMillisMotor);
  preferences.end();
}

void carregaHorimetro() {
  preferences.begin("horimetro", false);
  horimetro = preferences.getFloat("hours", 0);
  totalMillisMotor = preferences.getULong("millis", 0);
  preferences.end();
  
  Serial.print("Horimetro carregado: ");
  Serial.print(horimetro);
  Serial.println(" horas");
}

// ========== SUPABASE ==========
void atualizaSupabase() {
  if (millis() - ultimaAtualizacaoSupabase < INTERVALO_ATUALIZACAO_SUPABASE) return;
  if (WiFi.status() != WL_CONNECTED) return;
  
  ultimaAtualizacaoSupabase = millis();
  enviaHorimetroParaSupabase();
}

void verificacaoPeriodicaStatus() {
  if (millis() - ultimaVerificacaoStatus < INTERVALO_VERIFICACAO_STATUS) return;
  ultimaVerificacaoStatus = millis();
  
  // Pula a primeira verificação de status (não na primeira conexão do dia)
  if (primeiraVerificacaoStatus) {
    primeiraVerificacaoStatus = false;
    return;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    verificaStatusCadeira();
  }
}

void enviaHorimetroParaSupabase() {
  if (WiFi.status() != WL_CONNECTED) return;
  
  // Verifica se hÃ¡ memÃ³ria suficiente antes de tentar SSL
  if (ESP.getFreeHeap() < 30000) {
    Serial.println("[AVISO] Memoria insuficiente para SSL, pulando envio.");
    return;
  }
  
  Serial.println("Iniciando envio de horimetro...");
  
  Serial.print("[DEBUG] Heap livre: ");
  Serial.print(ESP.getFreeHeap());
  Serial.println(" bytes");
  WiFiClientSecure client;
  client.setInsecure();
  
  HTTPClient http;
  String url = supabaseUrl + "/rest/v1/chairs?serial_number=eq." + NUMERO_SERIE_CADEIRA;
  
  if (http.begin(client, url)) {
    http.setTimeout(10000); // 10s timeout
    http.addHeader("Content-Type", "application/json");
    if (supabaseKey.length() > 0) {
      http.addHeader("apikey", supabaseKey);
      http.addHeader("Authorization", String("Bearer ") + supabaseKey);
    }
    http.addHeader("Prefer", "return=minimal");
    
    StaticJsonDocument<256> doc;
    doc["hour_meter"] = horimetro;
    doc["last_update"] = getTimestamp();
    
    String payload;
    serializeJson(doc, payload);
    
    Serial.println("[DEBUG] Payload PATCH: " + payload);
    
    int httpCode = http.PATCH(payload);
    
    Serial.print("[DEBUG] HTTP Code: ");
    Serial.println(httpCode);
    
    if (httpCode == 200 || httpCode == 204) {
      Serial.println("[OK] Horimetro enviado ao Supabase");
    } else if (httpCode == 404) {
      Serial.println("Cadeira nao encontrada (404), registrando...");
      registraCadeiraNoSupabase();
    } else if (httpCode > 0) {
      Serial.print("[ERRO] Supabase (PATCH): ");
      Serial.print(httpCode);
      Serial.print(" - Resposta: ");
      Serial.println(http.getString());
    } else {
      Serial.print("[ERRO] Conexao (PATCH): ");
      Serial.print(httpCode);
      Serial.print(" - ");
      Serial.println(http.errorToString(httpCode).c_str());
    }
    http.end();
  } else {
    Serial.println("Falha ao iniciar conexao HTTP para PATCH");
  }
}

void registraCadeiraNoSupabase() {
  if (WiFi.status() != WL_CONNECTED) return;
  
  // Verifica se hÃ¡ memÃ³ria suficiente antes de tentar SSL
  if (ESP.getFreeHeap() < 30000) {
    Serial.println("[AVISO] Memoria insuficiente para SSL, pulando registro.");
    return;
  }
  
  Serial.println("Registrando cadeira no Supabase...");
  WiFiClientSecure client;
  client.setInsecure();
  
  HTTPClient http;
  String url = supabaseUrl + "/rest/v1/chairs";
  
  if (http.begin(client, url)) {
    http.setTimeout(10000); // 10s timeout
    http.addHeader("Content-Type", "application/json");
    if (supabaseKey.length() > 0) {
      http.addHeader("apikey", supabaseKey);
      http.addHeader("Authorization", String("Bearer ") + supabaseKey);
    }
    http.addHeader("Prefer", "return=minimal");
    
    StaticJsonDocument<512> doc;
    doc["serial_number"] = NUMERO_SERIE_CADEIRA;
    doc["hour_meter"] = horimetro;
    doc["enabled"] = true;
    doc["maintenance_required"] = false;
    doc["maintenance_hours"] = 500;
    
    String timestamp = getTimestamp();
    if (timestamp != "null") {
      doc["created_at"] = timestamp;
      doc["last_update"] = timestamp;
    }
    
    String payload;
    serializeJson(doc, payload);
    Serial.println("[REGISTRO] Payload: " + payload);
    Serial.println("[REGISTRO] Enviando POST...");
    
    int httpCode = http.POST(payload);
    
    Serial.print("[REGISTRO] HTTP Code: ");
    Serial.println(httpCode);
    
    if (httpCode == 201) {
      Serial.println("[SUCESSO] Cadeira registrada!");
      supabaseLogUsage("CHAIR_REGISTERED");
    } else if (httpCode == 409) {
      Serial.println("[INFO] Cadeira ja registrada (409)");
    } else if (httpCode > 0) {
      Serial.print("[ERRO] POST falhou: ");
      Serial.print(httpCode);
      Serial.print(" - Resposta: ");
      Serial.println(http.getString());
    } else {
      Serial.print("[ERRO] Conexao falhou: ");
      Serial.println(http.errorToString(httpCode).c_str());
    }
    http.end();
  } else {
    Serial.println("Falha ao iniciar conexao HTTP para POST");
  }
}

static bool supabasePostJson(const String& restPath, const String& payload) {
  if (WiFi.status() != WL_CONNECTED) return false;
  if (ESP.getFreeHeap() < 30000) return false;

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  String url = supabaseUrl + restPath;
  if (!http.begin(client, url)) {
    return false;
  }

  http.setTimeout(10000);
  http.addHeader("Content-Type", "application/json");
  if (supabaseKey.length() > 0) {
    http.addHeader("apikey", supabaseKey);
    http.addHeader("Authorization", String("Bearer ") + supabaseKey);
  }
  http.addHeader("Prefer", "return=minimal");

  int httpCode = http.POST(payload);
  bool ok = (httpCode == 201 || httpCode == 204);
  if (!ok && httpCode > 0) {
    Serial.print("[ERRO] Supabase POST ");
    Serial.print(restPath);
    Serial.print(" ");
    Serial.print(httpCode);
    Serial.print(" ");
    Serial.println(http.getString());
  }
  http.end();
  return ok;
}

static bool supabaseGetJson(const String& restPath, String& responseOut) {
  if (WiFi.status() != WL_CONNECTED) return false;
  if (ESP.getFreeHeap() < 30000) return false;

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  String url = supabaseUrl + restPath;
  if (!http.begin(client, url)) {
    return false;
  }

  http.setTimeout(10000);
  if (supabaseKey.length() > 0) {
    http.addHeader("apikey", supabaseKey);
    http.addHeader("Authorization", String("Bearer ") + supabaseKey);
  }

  int httpCode = http.GET();
  if (httpCode == 200) {
    responseOut = http.getString();
    http.end();
    return true;
  }

  if (httpCode > 0) {
    Serial.print("[ERRO] Supabase GET ");
    Serial.print(restPath);
    Serial.print(" ");
    Serial.print(httpCode);
    Serial.print(" ");
    Serial.println(http.getString());
  }
  http.end();
  return false;
}

static void loadMotorTravelPreferences() {
  Preferences p;
  if (!p.begin("motor_travel", false)) {
    return;
  }
  motorTravelPulsesEncosto = p.isKey("p_enc") ? p.getULong64("p_enc", 0) : 0;
  motorTravelPulsesAssento = p.isKey("p_ass") ? p.getULong64("p_ass", 0) : 0;
  motorTravelPulsesPerneira = p.isKey("p_per") ? p.getULong64("p_per", 0) : 0;
  motorTravelPulsesTrend = p.isKey("p_trend") ? p.getULong64("p_trend", 0) : 0;
  
  // Recupera posições virtuais (para manter após queda de energia)
  incoder_virtual_encosto_service = p.getInt("v_enc", 0);
  incoder_virtual_asento_service = p.getInt("v_ass", 0);
  incoder_virtual_perneira_service = p.getInt("v_per", 0);

  mmPerPulseEncosto = p.isKey("mm_enc") ? p.getFloat("mm_enc", mmPerPulseEncosto) : mmPerPulseEncosto;
  mmPerPulseAssento = p.isKey("mm_ass") ? p.getFloat("mm_ass", mmPerPulseAssento) : mmPerPulseAssento;
  mmPerPulsePerneira = p.isKey("mm_per") ? p.getFloat("mm_per", mmPerPulsePerneira) : mmPerPulsePerneira;
  mmPerPulseTrend = p.isKey("mm_trend") ? p.getFloat("mm_trend", mmPerPulseTrend) : mmPerPulseTrend;
  p.end();
  Serial.println("\n[TRAVEL] --- RECUPERACAO DE DADOS ---");
  Serial.print("[TRAVEL] PULSOS ENC=");
  Serial.print(static_cast<uint32_t>(motorTravelPulsesEncosto));
  Serial.print(" ASS=");
  Serial.print(static_cast<uint32_t>(motorTravelPulsesAssento));
  Serial.print(" PER=");
  Serial.print(static_cast<uint32_t>(motorTravelPulsesPerneira));
  Serial.print(" TREND=");
  Serial.println(static_cast<uint32_t>(motorTravelPulsesTrend));
  
  Serial.print("[TRAVEL] POS_VIRTUAL ENC=");
  Serial.print(incoder_virtual_encosto_service);
  Serial.print(" ASS=");
  Serial.print(incoder_virtual_asento_service);
  Serial.print(" PER=");
  Serial.println(incoder_virtual_perneira_service);
  Serial.println("[TRAVEL] ----------------------------\n");
}

static void saveMotorTravelPreferences(bool force) {
  uint32_t now = millis();
  static int last_v_enc = -1, last_v_ass = -1, last_v_per = -1;
  
  bool posChanged = (incoder_virtual_encosto_service != last_v_enc || 
                     incoder_virtual_asento_service != last_v_ass || 
                     incoder_virtual_perneira_service != last_v_per);

  if (!force) {
    if (motorTravelNextSaveMs != 0 && static_cast<int32_t>(now - motorTravelNextSaveMs) < 0) {
      return;
    }
    bool hasPending = (motorTravelUnsavedEncosto != 0 || motorTravelUnsavedAssento != 0 || motorTravelUnsavedPerneira != 0 || motorTravelUnsavedTrend != 0);
    if (!hasPending && !posChanged) {
      return;
    }
  }

  Preferences p;
  if (!p.begin("motor_travel", false)) {
    motorTravelNextSaveMs = now + 60000; // Tenta de novo em 1 minuto se falhar
    return;
  }
  p.putULong64("p_enc", motorTravelPulsesEncosto);
  p.putULong64("p_ass", motorTravelPulsesAssento);
  p.putULong64("p_per", motorTravelPulsesPerneira);
  p.putULong64("p_trend", motorTravelPulsesTrend);
  
  // Salva posições virtuais
  p.putInt("v_enc", incoder_virtual_encosto_service);
  p.putInt("v_ass", incoder_virtual_asento_service);
  p.putInt("v_per", incoder_virtual_perneira_service);
  
  p.putFloat("mm_enc", mmPerPulseEncosto);
  p.putFloat("mm_ass", mmPerPulseAssento);
  p.putFloat("mm_per", mmPerPulsePerneira);
  p.putFloat("mm_trend", mmPerPulseTrend);
  p.end();

  last_v_enc = incoder_virtual_encosto_service;
  last_v_ass = incoder_virtual_asento_service;
  last_v_per = incoder_virtual_perneira_service;

  motorTravelUnsavedEncosto = 0;
  motorTravelUnsavedAssento = 0;
  motorTravelUnsavedPerneira = 0;
  motorTravelUnsavedTrend = 0;
  motorTravelNextSaveMs = now + 30000;
}

static void motorTravelAutosaveTick() {
  uint32_t now = millis();
  if (motorTravelNextSaveMs != 0 && static_cast<int32_t>(now - motorTravelNextSaveMs) < 0) {
    return;
  }
  bool hasPending = (motorTravelUnsavedEncosto != 0 || motorTravelUnsavedAssento != 0 || motorTravelUnsavedPerneira != 0 || motorTravelUnsavedTrend != 0);
  if (!hasPending) {
    return;
  }
  bool anyMotorOn = relayIsOn(Rele_SA) || relayIsOn(Rele_DA) || relayIsOn(Rele_SE) || relayIsOn(Rele_DE) || relayIsOn(Rele_SP) || relayIsOn(Rele_DP) ||
                    (Rele_TREND_SOBE >= 0 && relayIsOn(Rele_TREND_SOBE)) || (Rele_TREND_DESCE >= 0 && relayIsOn(Rele_TREND_DESCE));
  if (anyMotorOn) {
    motorTravelNextSaveMs = now + 30000;
    return;
  }
  saveMotorTravelPreferences(true);
}

static bool supabaseUpsertMotorTravel() {
  if (WiFi.status() != WL_CONNECTED) return false;
  if (ESP.getFreeHeap() < 30000) return false;

  double enc_m = (static_cast<double>(motorTravelPulsesEncosto) * mmPerPulseEncosto) / 1000.0;
  double ass_m = (static_cast<double>(motorTravelPulsesAssento) * mmPerPulseAssento) / 1000.0;
  double per_m = (static_cast<double>(motorTravelPulsesPerneira) * mmPerPulsePerneira) / 1000.0;
  double tre_m = (static_cast<double>(motorTravelPulsesTrend) * mmPerPulseTrend) / 1000.0;
  
  // Distancia individual em Km
  double enc_km = enc_m / 1000.0;
  double ass_km = ass_m / 1000.0;
  double per_km = per_m / 1000.0;
  double tre_km = tre_m / 1000.0;

  // Verifica se QUALQUER motor ultrapassou o limite de manutencao individual
  if (enc_km >= maintenanceLimitKm || ass_km >= maintenanceLimitKm || 
      per_km >= maintenanceLimitKm || tre_km >= maintenanceLimitKm) {
    maintenanceRequired = true;
  }

  StaticJsonDocument<768> doc;
  doc["chair_serial"] = NUMERO_SERIE_CADEIRA;
  doc["encosto_pulses"] = static_cast<double>(motorTravelPulsesEncosto);
  doc["assento_pulses"] = static_cast<double>(motorTravelPulsesAssento);
  doc["perneira_pulses"] = static_cast<double>(motorTravelPulsesPerneira);
  doc["trend_pulses"] = static_cast<double>(motorTravelPulsesTrend);
  
  doc["encosto_km"] = enc_km;
  doc["assento_km"] = ass_km;
  doc["perneira_km"] = per_km;
  doc["trend_km"] = tre_km;
  
  doc["maintenance_limit_km"] = maintenanceLimitKm;
  doc["maintenance_required"] = maintenanceRequired;
  doc["updated_at"] = getTimestamp();

  String payload;
  serializeJson(doc, payload);

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  String url = supabaseUrl + "/rest/v1/chair_motor_travel?on_conflict=chair_serial";
  if (!http.begin(client, url)) {
    return false;
  }
  http.setTimeout(10000);
  http.addHeader("Content-Type", "application/json");
  if (supabaseKey.length() > 0) {
    http.addHeader("apikey", supabaseKey);
    http.addHeader("Authorization", String("Bearer ") + supabaseKey);
  }
  http.addHeader("Prefer", "resolution=merge-duplicates,return=minimal");
  int httpCode = http.POST(payload);
  bool ok = (httpCode == 200 || httpCode == 201 || httpCode == 204);
  if (!ok && httpCode > 0) {
    Serial.print("[ERRO] MotorTravel ");
    Serial.print(httpCode);
    Serial.print(" ");
    Serial.println(http.getString());
  }
  http.end();
  return ok;
}

static void sendMotorTravelToSupabaseIfNeeded() {
  uint32_t now = millis();
  if (static_cast<int32_t>(now - motorTravelNextSendMs) < 0) {
    return;
  }
  bool hasPending = (motorTravelUnsavedEncosto != 0 || motorTravelUnsavedAssento != 0 || motorTravelUnsavedPerneira != 0 || motorTravelUnsavedTrend != 0);
  motorTravelNextSendMs = now + MOTOR_TRAVEL_SEND_INTERVAL_MS;
  if (!hasPending) {
    return;
  }
  saveMotorTravelPreferences(true);
  if (supabaseUpsertMotorTravel()) {
    supabaseLogUsage("MOTOR_TRAVEL_SENT");
    bip();
  } else {
    bipLong();
  }
}

static bool supabaseLoadMemoryPositionFromDb(int slot) {
  if (slot < 1 || slot > 10) {
    return false;
  }
  String user = supabaseUserId.length() > 0 ? supabaseUserId : NUMERO_SERIE_CADEIRA;
  String path = "/rest/v1/memory_positions?user_id=eq." + user +
                "&slot=eq." + String(slot) +
                "&select=back_angle,seat_height,leg_rest_angle,updated_at" +
                "&order=updated_at.desc&limit=1";

  String response;
  if (!supabaseGetJson(path, response)) {
    enviarBLE("M:DB_LOAD:ERR");
    return false;
  }
  if (response == "[]") {
    Serial.println("[M] DB vazio para esse user/slot");
    enviarBLE("M:DB_LOAD:EMPTY");
    return false;
  }

  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, response);
  if (error || !doc.is<JsonArray>() || doc.size() == 0) {
    Serial.println("[M] ERRO JSON memory_positions");
    enviarBLE("M:DB_LOAD:JSON_ERR");
    return false;
  }

  float back = doc[0]["back_angle"] | 0.0f;
  float seat = doc[0]["seat_height"] | 0.0f;
  float leg = doc[0]["leg_rest_angle"] | 0.0f;

  int prevEnc = incoder_virtual_encosto_M1;
  int prevAss = incoder_virtual_asento_M1;
  int prevPer = incoder_virtual_perneira_M1;

  incoder_virtual_encosto_M1 = static_cast<int>(back);
  incoder_virtual_asento_M1 = static_cast<int>(seat);
  incoder_virtual_perneira_M1 = static_cast<int>(leg);

  preferences.begin("encoder_encosto", false);
  preferences.putInt("encoder_encosto", incoder_virtual_encosto_M1);
  preferences.end();

  preferences.begin("encoder_asento", false);
  preferences.putInt("encoder_asento", incoder_virtual_asento_M1);
  preferences.end();

  preferences.begin("encoder_pern", false);
  preferences.putInt("encoder_perneira", incoder_virtual_perneira_M1);
  preferences.end();

  Serial.print("[M] DB->M");
  Serial.print(slot);
  Serial.print(" ENC ");
  Serial.print(prevEnc);
  Serial.print("->");
  Serial.print(incoder_virtual_encosto_M1);
  Serial.print(" ASS ");
  Serial.print(prevAss);
  Serial.print("->");
  Serial.print(incoder_virtual_asento_M1);
  Serial.print(" PER ");
  Serial.print(prevPer);
  Serial.print("->");
  Serial.println(incoder_virtual_perneira_M1);

  enviarBLE(String("M:DB_LOADED:") + slot);
  supabaseLogUsage("MEMORY_POSITION_LOADED");
  return true;
}

static void supabaseLogUsage(const char* action) {
  StaticJsonDocument<512> doc;
  doc["action"] = action;
  doc["device_id"] = NUMERO_SERIE_CADEIRA;
  String ts = getTimestamp();
  if (ts.length() > 0 && ts != "null") {
    doc["created_at"] = ts;
  }
  if (supabaseUserId.length() > 0) {
    doc["user_id"] = supabaseUserId;
  }
  JsonObject meta = doc.createNestedObject("metadata");
  meta["heap"] = ESP.getFreeHeap();
  meta["rssi"] = WiFi.isConnected() ? WiFi.RSSI() : 0;
  String payload;
  serializeJson(doc, payload);
  supabasePostJson("/rest/v1/chair_usage_logs", payload);
}

static void supabaseCreateMaintenanceRequestIfNeeded() {
  if (!manutencaoNecessaria) return;
  if (supabaseMaintenanceRequestSent) return;

  StaticJsonDocument<384> doc;
  doc["chair_serial"] = NUMERO_SERIE_CADEIRA;
  doc["description"] = "Manutencao requerida";
  doc["hours_at_request"] = horimetro;
  String ts = getTimestamp();
  if (ts.length() > 0 && ts != "null") {
    doc["created_at"] = ts;
  }

  String payload;
  serializeJson(doc, payload);
  if (supabasePostJson("/rest/v1/maintenance_requests", payload)) {
    supabaseMaintenanceRequestSent = true;
    preferences.begin("cadeira", false);
    preferences.putBool("mnt_sent", true);
    preferences.end();
    supabaseLogUsage("MAINTENANCE_REQUEST_CREATED");
  }
}

static void supabaseSaveMemoryPosition(int slot) {
  if (slot < 1 || slot > 10) return;
  StaticJsonDocument<384> doc;
  String user = supabaseUserId.length() > 0 ? supabaseUserId : NUMERO_SERIE_CADEIRA;
  doc["user_id"] = user;
  doc["slot"] = slot;
  doc["back_angle"] = incoder_virtual_encosto_service;
  doc["seat_height"] = incoder_virtual_asento_service;
  doc["leg_rest_angle"] = incoder_virtual_perneira_service;
  String ts = getTimestamp();
  if (ts.length() > 0 && ts != "null") {
    doc["created_at"] = ts;
    doc["updated_at"] = ts;
  }

  String payload;
  serializeJson(doc, payload);
  if (supabasePostJson("/rest/v1/memory_positions", payload)) {
    supabaseLogUsage("MEMORY_POSITION_SAVED");
  }
}

void verificaStatusCadeira() {
  if (WiFi.status() != WL_CONNECTED) return;
  
  // Verifica se hÃ¡ memÃ³ria suficiente antes de tentar SSL
  size_t freeHeap = ESP.getFreeHeap();
  Serial.print("[DEBUG] Heap disponivel: ");
  Serial.print(freeHeap);
  Serial.println(" bytes");
  
  if (freeHeap < 30000) {
    Serial.println("[AVISO] Memoria insuficiente para SSL (< 30KB), pulando verificacao.");
    Serial.println("[INFO] Cadeira operara em modo offline ate proxima verificacao.");
    return;
  }
  
  Serial.println("Verificando status da cadeira no Supabase...");
  WiFiClientSecure client;
  client.setInsecure();
  
  HTTPClient http;
  String url = supabaseUrl + "/rest/v1/chairs?serial_number=eq." + NUMERO_SERIE_CADEIRA + "&select=enabled,maintenance_required,maintenance_hours";
  
  if (http.begin(client, url)) {
    http.setTimeout(10000); // 10s timeout
    if (supabaseKey.length() > 0) {
      http.addHeader("apikey", supabaseKey);
      http.addHeader("Authorization", String("Bearer ") + supabaseKey);
    }
    
    int httpCode = http.GET();
    Serial.print("HTTP Code: "); Serial.println(httpCode);
    
    if (httpCode == 200) {
      if (!beepSupabaseOkDone) {
        bip(); delay(120);
        bip();
        beepSupabaseOkDone = true;
      }
      supabaseOkAtMs = millis();
      supabaseFailAtMs = 0;
      String response = http.getString();
      Serial.print("Resposta: "); Serial.println(response);
      
      if (response == "[]") {
        Serial.println("Cadeira nao encontrada (lista vazia), registrando...");
        registraCadeiraNoSupabase();
      } else {
        // Usa StaticJsonDocument para evitar fragmentaÃ§Ã£o de heap
        StaticJsonDocument<768> doc;
        DeserializationError error = deserializeJson(doc, response);
        
        if (!error) {
          if (doc.is<JsonArray>() && doc.size() > 0) {
            cadeiraHabilitada = doc[0]["enabled"] | true;
            manutencaoNecessaria = doc[0]["maintenance_required"] | false;
            float horasManutencao = doc[0]["maintenance_hours"] | 500;
            
            if (horimetro >= horasManutencao) {
              manutencaoNecessaria = true;
            }
            
            Serial.print("Status: habilitada=");
            Serial.print(cadeiraHabilitada ? "SIM" : "NAO");
            Serial.print(", manutencao=");
            Serial.println(manutencaoNecessaria ? "SIM" : "NAO");

            if (!manutencaoNecessaria && supabaseMaintenanceRequestSent) {
              supabaseMaintenanceRequestSent = false;
              preferences.begin("cadeira", false);
              preferences.putBool("mnt_sent", false);
              preferences.end();
            }
            supabaseCreateMaintenanceRequestIfNeeded();
            
            if (!cadeiraHabilitada) {
              AT_SEG();
              Serial.println("!!! CADEIRA BLOQUEADA REMOTAMENTE !!!");
              bipBloqueio();
            }
          } else {
            Serial.println("Erro: Resposta do Supabase nao e um array valido ou esta vazia");
          }
        } else {
          Serial.print("Erro ao processar JSON: ");
          Serial.println(error.c_str());
        }
      }
    } else {
      Serial.print("Erro HTTP: "); Serial.println(httpCode);
      if (supabaseFailAtMs == 0) supabaseFailAtMs = millis();
    }
    http.end();
  } else {
    Serial.println("Falha ao iniciar conexao HTTP");
    if (supabaseFailAtMs == 0) supabaseFailAtMs = millis();
  }
}

void sincronizaNTP() {
  Serial.println("[NTP] Sincronizando horario com servidor NTP...");
  // Configura NTP para horário de Brasília (GMT-3)
  configTime(-3 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  
  Serial.print("[NTP] Aguardando sincronizacao");
  int tentativas = 0;
  while (time(nullptr) < 100000 && tentativas < 20) {
    Serial.print(".");
    delay(500);
    tentativas++;
  }
  
  if (time(nullptr) < 100000) {
    Serial.println("\n[ERRO] Falha ao sincronizar NTP");
  } else {
    time_t now = time(nullptr);
    Serial.println("\n[OK] Horario sincronizado!");
    Serial.print("[NTP] Data/Hora atual: ");
    Serial.println(ctime(&now));
  }
}

String getTimestamp() {
  // Retorna timestamp ISO 8601 (formato aceito pelo Supabase)
  time_t now = time(nullptr);
  struct tm timeinfo;
  
  if (!getLocalTime(&timeinfo)) {
    // Se NTP falhou, retorna NULL para Supabase usar horário do servidor
    return "null";
  }
  
  char buffer[30];
  strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S-03:00", &timeinfo);
  return String(buffer);
}

void bipBloqueio() {
  for (int i = 0; i < 5; i++) {
    bip();
    delay(200);
  }
}

// ========== CARREGAMENTO DE PREFERÃŠNCIAS ==========
void carregaPreferencias() {
  preferences.begin("cadeira", false);
  supabaseUserId = preferences.isKey("user_id") ? preferences.getString("user_id", "") : "";
  supabaseMaintenanceRequestSent = preferences.isKey("mnt_sent") ? preferences.getBool("mnt_sent", false) : false;
  seatTravelLearned = preferences.isKey("ass_travel_done") ? preferences.getBool("ass_travel_done", false) : false;
  preferences.end();

  preferences.begin("supabase", false);
  supabaseUrl = preferences.isKey("url") ? preferences.getString("url", SUPABASE_URL) : String(SUPABASE_URL);
  supabaseKey = preferences.isKey("key") ? preferences.getString("key", SUPABASE_KEY) : String(SUPABASE_KEY);
  if (supabaseKey.length() == 0) {
    supabaseKey = SUPABASE_KEY;
  }
  preferences.end();

  // Carrega posiÃ§Ãµes do encoder virtual M1
  preferences.begin("encoder_encosto", false);
  incoder_virtual_encosto_M1 = preferences.getInt("encoder_encosto", 0);
  preferences.end();
  
  preferences.begin("encoder_asento", false);
  incoder_virtual_asento_M1 = preferences.getInt("encoder_asento", 0);
  preferences.end();
  
  preferences.begin("encoder_pern", false);
  incoder_virtual_perneira_M1 = preferences.getInt("encoder_perneira", 0);
  preferences.end();

  // Carrega horÃ­metro
  carregaHorimetro();

  Serial.print("Encoder encosto M1: ");
  Serial.println(incoder_virtual_encosto_M1);
  Serial.print("Encoder assento M1: ");
  Serial.println(incoder_virtual_asento_M1);
  Serial.print("Encoder perneira M1: ");
  Serial.println(incoder_virtual_perneira_M1);
}

// ========== EXECUÃ‡ÃƒO M1 VIA BLUETOOTH (sem esperar botÃ£o fÃ­sico) ==========
void executa_M1_bluetooth() {
  bip();
  
  Serial.println("Executando Memoria 1 via Bluetooth");
  faz_bt_seg = 1;
  int targetEnc = incoder_virtual_encosto_M1;
  int targetAss = incoder_virtual_asento_M1;
  int targetPer = incoder_virtual_perneira_M1;
  if (!calibrationInProgress && !ignoreLimitLocks) {
    int stopEncMax = fim_encosto_encoder;
    int stopAssMax = fim_asento_encoder;
    int stopPerMax = fim_perneira_encoder;
    if (stopEncMax > LIMIT_STOP_MARGIN_PULSES) stopEncMax -= LIMIT_STOP_MARGIN_PULSES;
    if (stopAssMax > LIMIT_STOP_MARGIN_PULSES) stopAssMax -= LIMIT_STOP_MARGIN_PULSES;
    if (stopPerMax > LIMIT_STOP_MARGIN_PULSES) stopPerMax -= LIMIT_STOP_MARGIN_PULSES;
    if (stopEncMax > 0 && targetEnc > stopEncMax) targetEnc = stopEncMax;
    if (stopAssMax > 0 && targetAss > stopAssMax) targetAss = stopAssMax;
    if (stopPerMax > 0 && targetPer > stopPerMax) targetPer = stopPerMax;
  }

  Serial.print("[M1] ATUAL ENC=");
  Serial.print(incoder_virtual_encosto_service);
  Serial.print(" ASS=");
  Serial.print(incoder_virtual_asento_service);
  Serial.print(" PER=");
  Serial.println(incoder_virtual_perneira_service);
  Serial.print("[M1] ALVO  ENC=");
  Serial.print(incoder_virtual_encosto_M1);
  Serial.print(" ASS=");
  Serial.print(incoder_virtual_asento_M1);
  Serial.print(" PER=");
  Serial.println(incoder_virtual_perneira_M1);
  Serial.print("[M1] ALVO_EFETIVO ENC=");
  Serial.print(targetEnc);
  Serial.print(" ASS=");
  Serial.print(targetAss);
  Serial.print(" PER=");
  Serial.println(targetPer);

  bool alreadyAtTarget = (incoder_virtual_encosto_service == targetEnc &&
                          incoder_virtual_asento_service == targetAss &&
                          incoder_virtual_perneira_service == targetPer);
  if (!calibrationInProgress && !ignoreLimitLocks && alreadyAtTarget) {
    Serial.println("[M1] JA ESTA NA POSICAO");
    faz_bt_seg = 0;
    return;
  }

  reflectorPreMove(2, "M1");

  uint32_t prevPulseEnc = pulses_encosto;
  uint32_t prevPulseAss = pulses_assento;
  uint32_t prevPulsePer = pulses_perneira;
  uint32_t lastPulseEncAt = millis();
  uint32_t lastPulseAssAt = lastPulseEncAt;
  uint32_t lastPulsePerAt = lastPulseEncAt;
  
  while (true) {
    Watch_Dog();
    buzzerTestTick();
    contagem_tempo_incoder_virtual();
    reflectorAutoTick();
    monitora_tempo_rele();

    uint32_t now = millis();
    uint32_t curPe = pulses_encosto;
    uint32_t curPa = pulses_assento;
    uint32_t curPp = pulses_perneira;
    if (curPe != prevPulseEnc) { prevPulseEnc = curPe; lastPulseEncAt = now; }
    if (curPa != prevPulseAss) { prevPulseAss = curPa; lastPulseAssAt = now; }
    if (curPp != prevPulsePer) { prevPulsePer = curPp; lastPulsePerAt = now; }

    bool encMotorOn = relayIsOn(Rele_DE) || relayIsOn(Rele_SE);
    bool assMotorOn = relayIsOn(Rele_SA) || relayIsOn(Rele_DA);
    bool perMotorOn = relayIsOn(Rele_SP) || relayIsOn(Rele_DP);
    if (encMotorOn && ENCODER3 >= 0 && (now - lastPulseEncAt) > M1_STALL_TIMEOUT_MS) {
      Serial.println("[M1] STALL ENCOSTO - sem pulsos");
      enviarBLE("M1:STALL_ENC");
      AT_SEG();
      bipLong();
      return;
    }
    if (assMotorOn && ENCODER1 >= 0 && (now - lastPulseAssAt) > M1_STALL_TIMEOUT_MS) {
      Serial.println("[M1] STALL ASSENTO - sem pulsos");
      enviarBLE("M1:STALL_ASS");
      AT_SEG();
      bipLong();
      return;
    }
    if (perMotorOn && ENCODER2 >= 0 && (now - lastPulsePerAt) > M1_STALL_TIMEOUT_MS) {
      Serial.println("[M1] STALL PERNEIRA - sem pulsos");
      enviarBLE("M1:STALL_PER");
      AT_SEG();
      bipLong();
      return;
    }
    
    // Verifica parada de emergÃªncia via BLE
    if (comandoBLE.length() > 0) {
      String cmd = comandoBLE;
      comandoBLE = "";
      cmd.trim();
      cmd.toUpperCase();
      if (cmd == "AT_SEG" || cmd == "STOP") {
        AT_SEG();
        return;
      }
    }
    
    if (ENCODER3 < 0) {
      setOutputPin(Rele_DE, false, "M1");
      setOutputPin(Rele_SE, false, "M1");
      en_DE_acabou = true;
      en_SE_acabou = true;
    } else {
      if (incoder_virtual_encosto_service < targetEnc) {
        setOutputPin(Rele_DE, true, "M1");
        setOutputPin(Rele_SE, false, "M1");
        en_DE_acabou = false;
        en_SE_acabou = false;
      } else if (incoder_virtual_encosto_service > targetEnc) {
        setOutputPin(Rele_SE, true, "M1");
        setOutputPin(Rele_DE, false, "M1");
        en_DE_acabou = false;
        en_SE_acabou = false;
      } else {
        setOutputPin(Rele_DE, false, "M1");
        setOutputPin(Rele_SE, false, "M1");
        en_DE_acabou = true;
        en_SE_acabou = true;
      }
    }

    if (ENCODER1 < 0) {
      setOutputPin(Rele_SA, false, "M1");
      setOutputPin(Rele_DA, false, "M1");
      en_SA_acabou = true;
      en_DA_acabou = true;
    } else {
      if (incoder_virtual_asento_service < targetAss) {
        setOutputPin(Rele_SA, true, "M1");
        setOutputPin(Rele_DA, false, "M1");
        en_SA_acabou = false;
        en_DA_acabou = false;
      } else if (incoder_virtual_asento_service > targetAss) {
        setOutputPin(Rele_DA, true, "M1");
        setOutputPin(Rele_SA, false, "M1");
        en_SA_acabou = false;
        en_DA_acabou = false;
      } else {
        setOutputPin(Rele_SA, false, "M1");
        setOutputPin(Rele_DA, false, "M1");
        en_SA_acabou = true;
        en_DA_acabou = true;
      }
    }

    if (ENCODER2 < 0) {
      setOutputPin(Rele_SP, false, "M1");
      setOutputPin(Rele_DP, false, "M1");
      en_SP_acabou = true;
      en_DP_acabou = true;
    } else {
      if (incoder_virtual_perneira_service < targetPer) {
        setOutputPin(Rele_SP, true, "M1");
        setOutputPin(Rele_DP, false, "M1");
        en_SP_acabou = false;
        en_DP_acabou = false;
      } else if (incoder_virtual_perneira_service > targetPer) {
        setOutputPin(Rele_DP, true, "M1");
        setOutputPin(Rele_SP, false, "M1");
        en_SP_acabou = false;
        en_DP_acabou = false;
      } else {
        setOutputPin(Rele_SP, false, "M1");
        setOutputPin(Rele_DP, false, "M1");
        en_SP_acabou = true;
        en_DP_acabou = true;
      }
    }

    // Verifica se todos os movimentos terminaram
    if (en_SE_acabou && en_DE_acabou && en_SA_acabou && en_DA_acabou && en_SP_acabou && en_DP_acabou) {
      en_SP_acabou = false;
      en_DP_acabou = false;
      en_SE_acabou = false;
      en_DE_acabou = false;
      en_SA_acabou = false;
      en_DA_acabou = false;
      
      faz_bt_seg = 0;
      bip();
      delay(300);
      bip();
      
      Serial.println("Memoria 1 concluida");
      reflectorAutoTick();
      return;
    }
    delay(5);
  }
}

// ========== FUNÃ‡Ã•ES DO ENCODER VIRTUAL ==========
void contagem_tempo_incoder_virtual() {
  bool dir_encosto_up = (Rele_DE >= 0) ? relayIsOn(Rele_DE) : false;
  bool dir_encosto_down = (Rele_SE >= 0) ? relayIsOn(Rele_SE) : false;
  bool dir_asento_up = (Rele_SA >= 0) ? relayIsOn(Rele_SA) : false;
  bool dir_asento_down = (Rele_DA >= 0) ? relayIsOn(Rele_DA) : false;
  bool dir_perneira_up = (Rele_SP >= 0) ? relayIsOn(Rele_SP) : false;
  bool dir_perneira_down = (Rele_DP >= 0) ? relayIsOn(Rele_DP) : false;

  int stopEncMax = fim_encosto_encoder;
  int stopAssMax = fim_asento_encoder;
  int stopPerMax = fim_perneira_encoder;
  if (!calibrationInProgress && !ignoreLimitLocks) {
    if (stopEncMax > LIMIT_STOP_MARGIN_PULSES) stopEncMax = stopEncMax - LIMIT_STOP_MARGIN_PULSES;
    if (stopAssMax > LIMIT_STOP_MARGIN_PULSES) stopAssMax = stopAssMax - LIMIT_STOP_MARGIN_PULSES;
    if (stopPerMax > LIMIT_STOP_MARGIN_PULSES) stopPerMax = stopPerMax - LIMIT_STOP_MARGIN_PULSES;
  }

  uint32_t d_encosto = pulses_encosto - last_pulses_encosto;
  uint32_t d_asento = pulses_assento - last_pulses_assento;
  uint32_t d_perneira = pulses_perneira - last_pulses_perneira;
  uint32_t d_trend = pulses_trend - last_pulses_trend_travel;
  last_pulses_encosto = pulses_encosto;
  last_pulses_assento = pulses_assento;
  last_pulses_perneira = pulses_perneira;
  last_pulses_trend_travel = pulses_trend;

  if (pulsePrintEnabled) {
    uint32_t curTrend = static_cast<uint32_t>(pulses_trend);
    uint32_t curEnc = static_cast<uint32_t>(pulses_encosto);
    uint32_t curAss = static_cast<uint32_t>(pulses_assento);
    uint32_t curPer = static_cast<uint32_t>(pulses_perneira);

    if (curTrend < pulsePrintLastTrend) pulsePrintLastTrend = curTrend;
    if (curEnc < pulsePrintLastEncosto) pulsePrintLastEncosto = curEnc;
    if (curAss < pulsePrintLastAssento) pulsePrintLastAssento = curAss;
    if (curPer < pulsePrintLastPerneira) pulsePrintLastPerneira = curPer;

    while (pulsePrintLastTrend < curTrend) {
      pulsePrintLastTrend++;
      Serial.print("PULSO NO TREND MOTOR 1 = ");
      Serial.println(pulsePrintLastTrend);
    }
    while (pulsePrintLastEncosto < curEnc) {
      pulsePrintLastEncosto++;
      Serial.print("PULSO NO TREND MOTOR 2 = ");
      Serial.println(pulsePrintLastEncosto);
    }
    while (pulsePrintLastAssento < curAss) {
      pulsePrintLastAssento++;
      Serial.print("PULSO NO TREND MOTOR 3 = ");
      Serial.println(pulsePrintLastAssento);
    }
    while (pulsePrintLastPerneira < curPer) {
      pulsePrintLastPerneira++;
      Serial.print("PULSO NO TREND MOTOR 4 = ");
      Serial.println(pulsePrintLastPerneira);
    }
  }

  if (d_encosto) {
    motorTravelPulsesEncosto += d_encosto;
    motorTravelUnsavedEncosto += d_encosto;
  }
  if (d_asento) {
    motorTravelPulsesAssento += d_asento;
    motorTravelUnsavedAssento += d_asento;
  }
  if (d_perneira) {
    motorTravelPulsesPerneira += d_perneira;
    motorTravelUnsavedPerneira += d_perneira;
  }
  if (d_trend) {
    motorTravelPulsesTrend += d_trend;
    motorTravelUnsavedTrend += d_trend;
  }
  if (encoderPulseDebug && (d_encosto || d_asento || d_perneira || d_trend)) {
    Serial.print("[ENC_PULSE] dENC=");
    Serial.print(d_encosto);
    Serial.print(" dASS=");
    Serial.print(d_asento);
    Serial.print(" dPER=");
    Serial.print(d_perneira);
    Serial.print(" dTRE=");
    Serial.print(d_trend);
    Serial.print(" | PENC=");
    Serial.print(static_cast<uint32_t>(pulses_encosto));
    Serial.print(" PASS=");
    Serial.print(static_cast<uint32_t>(pulses_assento));
    Serial.print(" PPER=");
    Serial.print(static_cast<uint32_t>(pulses_perneira));
    Serial.print(" PTRE=");
    Serial.println(static_cast<uint32_t>(pulses_trend));
  }
  saveMotorTravelPreferences(false);

  if (d_encosto) {
    if (dir_encosto_up) {
      if (calibrationInProgress || ignoreLimitLocks || incoder_virtual_encosto_service + (int)d_encosto <= stopEncMax) {
        incoder_virtual_encosto_service += (int)d_encosto;
        trava_bt_SE = false;
      } else {
        incoder_virtual_encosto_service = stopEncMax;
        setOutputPin(Rele_DE, false, "LIMIT");
        trava_bt_DE = true;
      }
    } else if (dir_encosto_down) {
      if (ignoreLimitLocks || incoder_virtual_encosto_service - (int)d_encosto >= 0) {
        incoder_virtual_encosto_service -= (int)d_encosto;
        trava_bt_DE = false;
      } else {
        incoder_virtual_encosto_service = 0;
        setOutputPin(Rele_SE, false, "LIMIT");
        trava_bt_SE = true;
      }
    }
  }

  if (d_asento) {
    if (dir_asento_up) {
      if (calibrationInProgress || ignoreLimitLocks || incoder_virtual_asento_service + (int)d_asento <= stopAssMax) {
        incoder_virtual_asento_service += (int)d_asento;
        trava_bt_DA = false; trava_bt_SA = false;
      } else {
        incoder_virtual_asento_service = stopAssMax;
        setOutputPin(Rele_SA, false, "LIMIT");
        trava_bt_SA = true;
      }
    } else if (dir_asento_down) {
      if (ignoreLimitLocks || incoder_virtual_asento_service - (int)d_asento >= 0) {
        incoder_virtual_asento_service -= (int)d_asento;
        trava_bt_SA = false; trava_bt_DA = false;
      } else {
        incoder_virtual_asento_service = 0;
        setOutputPin(Rele_DA, false, "LIMIT");
        trava_bt_DA = true;
      }
    }
  }

  if (d_perneira) {
    if (dir_perneira_up) {
      if (calibrationInProgress || ignoreLimitLocks || incoder_virtual_perneira_service + (int)d_perneira <= stopPerMax) {
        incoder_virtual_perneira_service += (int)d_perneira;
        trava_bt_DP = false; trava_bt_SP = false;
      } else {
        incoder_virtual_perneira_service = stopPerMax;
        setOutputPin(Rele_SP, false, "LIMIT");
        trava_bt_SP = true;
      }
    } else if (dir_perneira_down) {
      if (ignoreLimitLocks || incoder_virtual_perneira_service - (int)d_perneira >= 0) {
        incoder_virtual_perneira_service -= (int)d_perneira;
        trava_bt_SP = false; trava_bt_DP = false;
      } else {
        incoder_virtual_perneira_service = 0;
        setOutputPin(Rele_DP, false, "LIMIT");
        trava_bt_DP = true;
      }
    }
  }

  bleStatusStreamTick();
}

// ========== WATCH DOG (LED heartbeat) ==========
void Watch_Dog() {
  unsigned long agora = millis();
  if (agora - ultimoMillisPiscaLed > 500) {
    ultimoMillisPiscaLed = agora;
    estadoLedPisca = !estadoLedPisca;
    digitalWrite(LED, estadoLedPisca ? HIGH : LOW);
  }
}

unsigned long inicioAtivacaoRele = 0;
const unsigned long TIMEOUT_RELE = 30000; // 30 segundos

// ========== MONITORAMENTO DE TEMPO DOS RELÃ‰S (seguranÃ§a) ==========
void monitora_tempo_rele() {
  if (isGavetaAberta()) {
    bool spOn = relayIsOn(Rele_SP);
    bool dpOn = relayIsOn(Rele_DP);
    if (spOn || dpOn) {
      Serial.println("[GAVETA] Aberta - desligando perneira");
      setOutputPin(Rele_SP, false, "GAVETA");
      setOutputPin(Rele_DP, false, "GAVETA");
      estado_sp = false;
      estado_dp = false;
      enviarBLE("GAVETA:OPEN");
      enviarBLE("SP:GAVETA");
      enviarBLE("DP:GAVETA");
      mqttEnqueuePublish(MQTT_TOPIC_BASE + "tx_cmd", "GAVETA_OPEN", false);
    }
  }

  bool seOn = relayIsOn(Rele_SE);
  bool saOn = relayIsOn(Rele_SA);
  bool daOn = relayIsOn(Rele_DA);
  bool spOn = relayIsOn(Rele_SP);
  bool dpOn = relayIsOn(Rele_DP);
  bool deOn = relayIsOn(Rele_DE);
  bool trendSobeOn = (Rele_TREND_SOBE >= 0 && relayIsOn(Rele_TREND_SOBE));
  bool trendDesceOn = (Rele_TREND_DESCE >= 0 && relayIsOn(Rele_TREND_DESCE));
  bool algumReleAtivo = (seOn || saOn || daOn || spOn || dpOn || deOn || trendSobeOn || trendDesceOn);

  if (algumReleAtivo) {
    if (inicioAtivacaoRele == 0) {
      inicioAtivacaoRele = millis();
    } else if (millis() - inicioAtivacaoRele >= TIMEOUT_RELE) {
      relayTrackPrintOnRelays("TIMEOUT");
      Serial.println("!!! TIMEOUT DE SEGURANÃ‡A - RELÃ‰ ATIVO POR 30s !!!");
      AT_SEG();
      inicioAtivacaoRele = 0;
    }
  } else {
    inicioAtivacaoRele = 0;
  }
}

// ========== BOTÃƒO DE SEGURANÃ‡A (parada de emergÃªncia) ==========
void Button_Seg() {
  if (cont == 0 && cont13 == 0 && !faz_m1 && !vzInicialEmAndamento) {
    return;
  }
  buttonState = digitalRead(DE);
  if (buttonState == LOW) { AT_SEG(); }

  buttonState = digitalRead(SE);
  if (buttonState == LOW) { AT_SEG(); }

  buttonState = digitalRead(DA);
  if (buttonState == LOW) { AT_SEG(); }

  buttonState = digitalRead(SA);
  if (buttonState == LOW) { AT_SEG(); }

  buttonState = digitalRead(VZ);
  if (buttonState == LOW) { AT_SEG(); }

  // Nota: RF nÃ£o chama AT_SEG pois Ã© usado para reset WiFi
  // buttonState = digitalRead(RF);
  // if (buttonState == LOW) { AT_SEG(); }

  buttonState = digitalRead(SP);
  if (buttonState == LOW) { AT_SEG(); }

  buttonState = digitalRead(DP);
  if (buttonState == LOW) { AT_SEG(); }

  buttonState = digitalRead(PT);
  if (buttonState == LOW) { AT_SEG(); }

  buttonState = digitalRead(M1);
  if (buttonState == LOW) { AT_SEG(); }
}

// VariÃ¡veis para detecÃ§Ã£o de borda de subida nos botÃµes
static const uint32_t BUTTON_DEBOUNCE_MS = 40;
bool lastButtonState_RF = HIGH;
bool lastButtonState_SP = HIGH;
bool lastButtonState_DP = HIGH;
bool lastButtonState_DE = HIGH;
bool lastButtonState_SE = HIGH;
bool lastButtonState_SA = HIGH;
bool lastButtonState_DA = HIGH;
bool lastButtonState_GAVETA = HIGH;
bool lastButtonState_VZ = HIGH;
bool lastButtonState_M1 = HIGH;
bool lastButtonState_PT = HIGH;

bool rawButtonState_RF = HIGH;
bool rawButtonState_SP = HIGH;
bool rawButtonState_DP = HIGH;
bool rawButtonState_DE = HIGH;
bool rawButtonState_SE = HIGH;
bool rawButtonState_SA = HIGH;
bool rawButtonState_DA = HIGH;
bool rawButtonState_GAVETA = HIGH;
bool rawButtonState_VZ = HIGH;
bool rawButtonState_M1 = HIGH;
bool rawButtonState_PT = HIGH;

uint32_t rawButtonChangedAt_RF = 0;
uint32_t rawButtonChangedAt_SP = 0;
uint32_t rawButtonChangedAt_DP = 0;
uint32_t rawButtonChangedAt_DE = 0;
uint32_t rawButtonChangedAt_SE = 0;
uint32_t rawButtonChangedAt_SA = 0;
uint32_t rawButtonChangedAt_DA = 0;
uint32_t rawButtonChangedAt_GAVETA = 0;
uint32_t rawButtonChangedAt_VZ = 0;
uint32_t rawButtonChangedAt_M1 = 0;
uint32_t rawButtonChangedAt_PT = 0;

static void inicializaEstadosDeBordaBotoes() {
  uint32_t now = millis();
  lastButtonState_RF = digitalRead(RF);
  rfIdleLevel = lastButtonState_RF;
  lastButtonState_SP = digitalRead(SP);
  lastButtonState_DP = digitalRead(DP);
  lastButtonState_DE = digitalRead(DE);
  lastButtonState_SE = digitalRead(SE);
  lastButtonState_SA = digitalRead(SA);
  lastButtonState_DA = digitalRead(DA);
  lastButtonState_GAVETA = digitalRead(GAVETA);
  lastButtonState_VZ = digitalRead(VZ);
  lastButtonState_M1 = digitalRead(M1);
  lastButtonState_PT = digitalRead(PT);

  rawButtonState_RF = lastButtonState_RF; rawButtonChangedAt_RF = now;
  rawButtonState_SP = lastButtonState_SP; rawButtonChangedAt_SP = now;
  rawButtonState_DP = lastButtonState_DP; rawButtonChangedAt_DP = now;
  rawButtonState_DE = lastButtonState_DE; rawButtonChangedAt_DE = now;
  rawButtonState_SE = lastButtonState_SE; rawButtonChangedAt_SE = now;
  rawButtonState_SA = lastButtonState_SA; rawButtonChangedAt_SA = now;
  rawButtonState_DA = lastButtonState_DA; rawButtonChangedAt_DA = now;
  rawButtonState_GAVETA = lastButtonState_GAVETA; rawButtonChangedAt_GAVETA = now;
  rawButtonState_VZ = lastButtonState_VZ; rawButtonChangedAt_VZ = now;
  rawButtonState_M1 = lastButtonState_M1; rawButtonChangedAt_M1 = now;
  rawButtonState_PT = lastButtonState_PT; rawButtonChangedAt_PT = now;
}

// ========== LEITURA DOS BOTÃ•ES FÃSICOS (NÃ£o bloqueante) ==========
void Button_geral() {
  if ((millis() - ultimoComandoRemoto) < 1500) return;
  if (faz_bt_seg != 0) return;
  if (ignoreButtonsUntilRelease) {
    if (isAnyUserButtonActiveRaw()) {
      return;
    }
    ignoreButtonsUntilRelease = false;
    inicializaEstadosDeBordaBotoes();
  }

  // Para motores: comportamento "dead-man switch"
  // - Pressionou (LOW): liga e fica ligado enquanto estiver pressionado
  // - Soltou (HIGH): desliga
  // - Enquanto pressionado, renova o timeout para nao cair em TIMEOUT
  auto readMotorButton = [](int pin, bool &stableState, bool &rawState, uint32_t &rawChangedAtMs, const char* msg, bool &estadoMotor, int relayPin, bool locked, const char* lockedReason, unsigned long &ultimoCmdMs) {
    uint32_t now = millis();
    bool currentRaw = digitalRead(pin);
    if (currentRaw != rawState) {
      rawState = currentRaw;
      rawChangedAtMs = now;
    }
    if ((now - rawChangedAtMs) < BUTTON_DEBOUNCE_MS) {
      return;
    }
    bool currentState = rawState;

    if (currentState == LOW && stableState == HIGH) {
      Serial.print("[GPIO_INT] Botao "); Serial.print(msg); Serial.print(" (Pino "); Serial.print(pin); Serial.println(") = 0 (PRESSIONADO)");
      if (locked) {
        Serial.print("Comando "); Serial.print(msg); Serial.print(" bloqueado ("); Serial.print(lockedReason); Serial.println(")");
        if (lockedReason && String(lockedReason) == "limite") {
          Serial.print("[LIMIT] IGNORE="); Serial.print(ignoreLimitLocks ? "1" : "0");
          Serial.print(" ENC="); Serial.print(incoder_virtual_encosto_service);
          Serial.print(" ASS="); Serial.print(incoder_virtual_asento_service);
          Serial.print(" PER="); Serial.println(incoder_virtual_perneira_service);
        } else if (lockedReason && String(lockedReason) == "gaveta") {
          Serial.println("[GAVETA] Aberta - perneira bloqueada");
        }
      }
      if (!locked) {
        mqttEnqueuePublish(MQTT_TOPIC_BASE + "tx_cmd", msg, false);
      }
    }

    if (currentState == HIGH && stableState == LOW) {
      Serial.print("[GPIO_INT] Botao "); Serial.print(msg); Serial.print(" (Pino "); Serial.print(pin); Serial.println(") = 1 (SOLTO)");
      mqttEnqueuePublish(MQTT_TOPIC_BASE + "tx_cmd", "STOP", false);
    }

    if (currentState == LOW && !locked) {
      ultimoCmdMs = millis();
      if (!estadoMotor) {
        estadoMotor = true;
        setOutputPin(relayPin, true, msg);
        Serial.print("Comando "); Serial.print(msg); Serial.println(" acionado");
      }
    } else {
      if (estadoMotor) {
        estadoMotor = false;
        setOutputPin(relayPin, false, msg);
      }
    }

    stableState = currentState;
  };

  // Para botoes de toggle (ex: refletor)
  auto readToggleButton = [](int pin, bool &stableState, bool &rawState, uint32_t &rawChangedAtMs, const char* msg, bool &toggleVar, int relayPin) {
    uint32_t now = millis();
    bool currentRaw = digitalRead(pin);
    if (currentRaw != rawState) {
      rawState = currentRaw;
      rawChangedAtMs = now;
    }
    if ((now - rawChangedAtMs) < BUTTON_DEBOUNCE_MS) {
      return;
    }
    bool currentState = rawState;

    if (currentState == LOW && stableState == HIGH) {
      Serial.print("[GPIO_INT] Botao "); Serial.print(msg); Serial.print(" (Pino "); Serial.print(pin); Serial.println(") = 0 (PRESSIONADO)");
      toggleVar = !toggleVar;
      setOutputPin(relayPin, toggleVar, msg);
      Serial.print("Comando "); Serial.print(msg); Serial.println(" acionado");
      mqttEnqueuePublish(MQTT_TOPIC_BASE + "tx_cmd", msg, false);
    } else if (currentState == HIGH && stableState == LOW) {
      Serial.print("[GPIO_INT] Botao "); Serial.print(msg); Serial.print(" (Pino "); Serial.print(pin); Serial.println(") = 1 (SOLTO)");
    }
    stableState = currentState;
  };

  auto readInputOnly = [](int pin, bool &stableState, bool &rawState, uint32_t &rawChangedAtMs, const char* msg) {
    if (pin < 0) {
      return;
    }
    uint32_t now = millis();
    bool currentRaw = digitalRead(pin);
    if (currentRaw != rawState) {
      rawState = currentRaw;
      rawChangedAtMs = now;
    }
    if ((now - rawChangedAtMs) < BUTTON_DEBOUNCE_MS) {
      return;
    }
    bool currentState = rawState;
    if (currentState == LOW && stableState == HIGH) {
      Serial.print("[GPIO_INT] Botao "); Serial.print(msg); Serial.print(" (Pino "); Serial.print(pin); Serial.println(") = 0 (PRESSIONADO)");
    } else if (currentState == HIGH && stableState == LOW) {
      Serial.print("[GPIO_INT] Botao "); Serial.print(msg); Serial.print(" (Pino "); Serial.print(pin); Serial.println(") = 1 (SOLTO)");
    }
    stableState = currentState;
  };

  if (rfHabilitado) {
    uint32_t now = millis();
    bool currentRaw = digitalRead(RF);
    if (currentRaw != rawButtonState_RF) {
      rawButtonState_RF = currentRaw;
      rawButtonChangedAt_RF = now;
    }
    if ((now - rawButtonChangedAt_RF) >= BUTTON_DEBOUNCE_MS) {
      bool currentState = rawButtonState_RF;
      if (rfIdleLevel < 0) {
        rfIdleLevel = currentState ? HIGH : LOW;
      }
      bool pressed = (currentState != (rfIdleLevel != 0));
      bool wasPressed = (lastButtonState_RF != (rfIdleLevel != 0));

      if (pressed && !wasPressed) {
        Serial.print("[GPIO_INT] Botao RF (Pino "); Serial.print(RF); Serial.println(") = PRESSED");
        estado_r = !estado_r;
        setOutputPin(Rele_refletor, estado_r, "RF");
        Serial.println("Comando RF acionado");
        mqttEnqueuePublish(MQTT_TOPIC_BASE + "tx_cmd", "RF", false);
        mqttStatusDirty = true;
      } else if (!pressed && wasPressed) {
        Serial.print("[GPIO_INT] Botao RF (Pino "); Serial.print(RF); Serial.println(") = RELEASED");
      }
      lastButtonState_RF = currentState;
    }
  }
  readInputOnly(GAVETA, lastButtonState_GAVETA, rawButtonState_GAVETA, rawButtonChangedAt_GAVETA, "GAVETA");
  bool gavetaAberta = isGavetaAberta();
  {
    static bool lastGavetaAberta = false;
    if (gavetaAberta != lastGavetaAberta) {
      lastGavetaAberta = gavetaAberta;
      if (gavetaAberta) {
        enviarBLE("GAVETA:OPEN");
        mqttEnqueuePublish(MQTT_TOPIC_BASE + "tx_cmd", "GAVETA_OPEN", false);
        if (!perneiraIsAtPt()) {
          enviarBLE("GAVETA:NOT_ALLOWED");
          mqttEnqueuePublish(MQTT_TOPIC_BASE + "tx_cmd", "GAVETA_NOT_ALLOWED", false);
          AT_SEG();
        }
      } else {
        enviarBLE("GAVETA:CLOSED");
        mqttEnqueuePublish(MQTT_TOPIC_BASE + "tx_cmd", "GAVETA_CLOSED", false);
      }
      mqttStatusDirty = true;
    }
  }
  readMotorButton(SP, lastButtonState_SP, rawButtonState_SP, rawButtonChangedAt_SP, "SP", estado_sp, Rele_SP, (!ignoreLimitLocks && trava_bt_SP) || gavetaAberta, gavetaAberta ? "gaveta" : "limite", ultimoComandoSP);
  readMotorButton(DP, lastButtonState_DP, rawButtonState_DP, rawButtonChangedAt_DP, "DP", estado_dp, Rele_DP, (!ignoreLimitLocks && trava_bt_DP) || gavetaAberta, gavetaAberta ? "gaveta" : "limite", ultimoComandoDP);
  readMotorButton(DE, lastButtonState_DE, rawButtonState_DE, rawButtonChangedAt_DE, "DE", estado_de, Rele_DE, !ignoreLimitLocks && trava_bt_DE, "limite", ultimoComandoDE);
  readMotorButton(SE, lastButtonState_SE, rawButtonState_SE, rawButtonChangedAt_SE, "SE", estado_se, Rele_SE, !ignoreLimitLocks && trava_bt_SE, "limite", ultimoComandoSE);
  readMotorButton(SA, lastButtonState_SA, rawButtonState_SA, rawButtonChangedAt_SA, "SA", estado_sa, Rele_SA, !ignoreLimitLocks && trava_bt_SA, "limite", ultimoComandoSA);
  readMotorButton(DA, lastButtonState_DA, rawButtonState_DA, rawButtonChangedAt_DA, "DA", estado_da, Rele_DA, !ignoreLimitLocks && trava_bt_DA, "limite", ultimoComandoDA);

  // Botões de funções especiais (VZ, M1, PT)
  {
    uint32_t now = millis();
    bool currRaw = digitalRead(VZ);
    if (currRaw != rawButtonState_VZ) {
      rawButtonState_VZ = currRaw;
      rawButtonChangedAt_VZ = now;
    }
    if ((now - rawButtonChangedAt_VZ) >= BUTTON_DEBOUNCE_MS) {
      bool curr = rawButtonState_VZ;
      if (curr == LOW && lastButtonState_VZ == HIGH) {
        Serial.print("[GPIO_INT] Botao VZ (Pino "); Serial.print(VZ); Serial.println(") = 0 (PRESSIONADO)");
        Serial.println("Comando VZ acionado");
        cont = 1;
        executa_vz();
      } else if (curr == HIGH && lastButtonState_VZ == LOW) {
        Serial.print("[GPIO_INT] Botao VZ (Pino "); Serial.print(VZ); Serial.println(") = 1 (SOLTO)");
      }
      lastButtonState_VZ = curr;
    }
  }

  {
    uint32_t now = millis();
    static uint32_t m1PressedAtMs = 0;
    static bool m1Pressed = false;
    bool currRaw = digitalRead(M1);
    if (currRaw != rawButtonState_M1) {
      rawButtonState_M1 = currRaw;
      rawButtonChangedAt_M1 = now;
    }
    if ((now - rawButtonChangedAt_M1) >= BUTTON_DEBOUNCE_MS) {
      bool curr = rawButtonState_M1;
      if (curr == LOW && lastButtonState_M1 == HIGH) {
        Serial.print("[GPIO_INT] Botao M1 (Pino "); Serial.print(M1); Serial.println(") = 0 (PRESSIONADO)");
        m1PressedAtMs = now;
        m1Pressed = true;
      } else if (curr == HIGH && lastButtonState_M1 == LOW) {
        Serial.print("[GPIO_INT] Botao M1 (Pino "); Serial.print(M1); Serial.println(") = 1 (SOLTO)");
        if (m1Pressed) {
          m1Pressed = false;
          uint32_t dur = now - m1PressedAtMs;
          if (dur >= M1_HOLD_SAVE_MS) {
            incoder_virtual_encosto_M1 = incoder_virtual_encosto_service;
            incoder_virtual_asento_M1 = incoder_virtual_asento_service;
            incoder_virtual_perneira_M1 = incoder_virtual_perneira_service;

            preferences.begin("encoder_encosto", false);
            preferences.putInt("encoder_encosto", incoder_virtual_encosto_M1);
            preferences.end();
            
            preferences.begin("encoder_asento", false);
            preferences.putInt("encoder_asento", incoder_virtual_asento_M1);
            preferences.end();
            
            preferences.begin("encoder_pern", false);
            preferences.putInt("encoder_perneira", incoder_virtual_perneira_M1);
            preferences.end();

            Serial.println("[M1] Posicoes gravadas");
            Serial.print("[M1] ENC="); Serial.print(incoder_virtual_encosto_M1);
            Serial.print(" ASS="); Serial.print(incoder_virtual_asento_M1);
            Serial.print(" PER="); Serial.println(incoder_virtual_perneira_M1);
            enviarBLE("M1:SAVED");
            supabaseSaveMemoryPosition(1);
            bip();
            delay(200);
            bip();
          } else {
            Serial.println("[M1] Executando memoria");
            enviarBLE("M1:EXEC");
            executa_M1_bluetooth();
            enviarBLE("M1:DONE");
          }
        }
      }
      lastButtonState_M1 = curr;
    }
  }

  {
    uint32_t now = millis();
    bool currRaw = digitalRead(PT);
    if (currRaw != rawButtonState_PT) {
      rawButtonState_PT = currRaw;
      rawButtonChangedAt_PT = now;
    }
    if ((now - rawButtonChangedAt_PT) >= BUTTON_DEBOUNCE_MS) {
      bool curr = rawButtonState_PT;
      if (curr == LOW && lastButtonState_PT == HIGH) {
        Serial.print("[GPIO_INT] Botao PT (Pino "); Serial.print(PT); Serial.println(") = 0 (PRESSIONADO)");
        Serial.println("Comando PT acionado");
        cont13 = 1;
        executa_pt();
      } else if (curr == HIGH && lastButtonState_PT == LOW) {
        Serial.print("[GPIO_INT] Botao PT (Pino "); Serial.print(PT); Serial.println(") = 1 (SOLTO)");
      }
      lastButtonState_PT = curr;
    }
  }
}

void iniciaCalibracaoSeNecessario() {
  Preferences p;
  p.begin("cadeira", false);
  bool done = p.getBool("cal_done", false);
  p.end();
  if (!done) {
    executaCalibracao();
  }
}

static const uint32_t CAL_NO_PULSE_TIMEOUT_MS = 6000;

void executaCalibracao() {
  Serial.println("\n=== CALIBRACAO AUTOMATICA (primeira vez) ===");
  Serial.println("Indo para zero (VZ) por 15 segundos...");
  calibrationInProgress = true;
  incoder_virtual_encosto_service = 0;
  incoder_virtual_asento_service = 0;
  incoder_virtual_perneira_service = 0;
  pulses_encosto = pulses_assento = pulses_perneira = 0;
  pulses_trend = 0;
  last_pulses_encosto = last_pulses_assento = last_pulses_perneira = 0;
  last_pulses_trend = 0;
  last_pulses_trend_travel = 0;
  contador2 = 0;
  cont = 1;
  executa_vz();
  setOutputPin(Rele_SE, false, "CAL");
  setOutputPin(Rele_DE, false, "CAL");
  setOutputPin(Rele_DA, false, "CAL");
  setOutputPin(Rele_SA, false, "CAL");
  setOutputPin(Rele_DP, false, "CAL");
  setOutputPin(Rele_SP, false, "CAL");
  if (Rele_TREND_SOBE >= 0) setOutputPin(Rele_TREND_SOBE, false, "CAL");
  if (Rele_TREND_DESCE >= 0) setOutputPin(Rele_TREND_DESCE, false, "CAL");
  delay(200);
  Serial.println("Subindo todos os eixos ate limite (aprendizado)...");
  unsigned long start = millis();
  bool doneEnc = (!encReqEncosto() || ENCODER3 < 0);
  bool doneAss = (!encReqAssento() || ENCODER1 < 0);
  bool donePer = (!encReqPerneira() || ENCODER2 < 0);
  bool doneTrend = (!encReqTrend() || ENCODER_TREND < 0);
  bool errEnc = false, errAss = false, errPer = false, errTrend = false;
  bool sawEnc = false, sawAss = false, sawPer = false, sawTrend = false;
  unsigned long lastChangeEnc = millis(), lastChangeAss = millis(), lastChangePer = millis(), lastChangeTrend = millis();
  unsigned long startEnc = 0, startAss = 0, startPer = 0, startTrend = 0;
  uint32_t startPulseEnc = pulses_encosto, startPulseAss = pulses_assento, startPulsePer = pulses_perneira;
  uint32_t startPulseTrend = pulses_trend;
  uint32_t prevPulseEnc = pulses_encosto, prevPulseAss = pulses_assento, prevPulsePer = pulses_perneira;
  uint32_t prevPulseTrend = pulses_trend;
  
  if (!doneEnc) {
    startEnc = millis();
    lastChangeEnc = startEnc;
    setOutputPin(Rele_DE, true, "CAL");
    delay(1000);
  }
  
  if (!doneAss) {
    startAss = millis();
    lastChangeAss = startAss;
    setOutputPin(Rele_SA, true, "CAL");
    delay(1000);
  }
  
  if (!donePer) {
    startPer = millis();
    lastChangePer = startPer;
    setOutputPin(Rele_SP, true, "CAL");
    delay(1000);
  }
  
  if (!doneTrend && Rele_TREND_SOBE >= 0) {
    startTrend = millis();
    lastChangeTrend = startTrend;
    setOutputPin(Rele_TREND_SOBE, true, "CAL");
  } else {
    doneTrend = true;
  }

  startEnc = startAss = startPer = startTrend = millis();
  lastChangeEnc = lastChangeAss = lastChangePer = lastChangeTrend = millis();
  prevPulseEnc = pulses_encosto;
  prevPulseAss = pulses_assento;
  prevPulsePer = pulses_perneira;
  prevPulseTrend = pulses_trend;

  uint32_t maxTrend = 0;
  while (!(doneEnc && doneAss && donePer && doneTrend) && (millis() - start) < 30000) {
    uint32_t pe = pulses_encosto, pa = pulses_assento, pp = pulses_perneira, pt = pulses_trend;
    contagem_tempo_incoder_virtual();
    if (!doneEnc) {
      if (pe != prevPulseEnc) {
        lastChangeEnc = millis();
        sawEnc = true;
        prevPulseEnc = pe;
      }
      if ((millis() - startEnc > CAL_NO_PULSE_TIMEOUT_MS) && !sawEnc) {
        setOutputPin(Rele_DE, false, "CAL");
        doneEnc = true;
        errEnc = true;
        for (int i=0;i<3;i++){ bip(); delay(120);} delay(200); for (int i=0;i<3;i++){ bip(); delay(120);}
        Serial.println("[ERRO CAL] Encosto: sem pulsos do encoder. Verifique ligacao.");
      } else if (sawEnc && millis() - lastChangeEnc > 500) {
        setOutputPin(Rele_DE, false, "CAL");
        doneEnc = true;
        Serial.print("Encosto max = "); Serial.println(incoder_virtual_encosto_service);
      }
    }
    if (!doneAss) {
      if (pa != prevPulseAss) {
        lastChangeAss = millis();
        sawAss = true;
        prevPulseAss = pa;
      }
      if ((millis() - startAss > CAL_NO_PULSE_TIMEOUT_MS) && !sawAss) {
        setOutputPin(Rele_SA, false, "CAL");
        doneAss = true;
        errAss = true;
        for (int i=0;i<3;i++){ bip(); delay(120);} delay(200); for (int i=0;i<3;i++){ bip(); delay(120);}
        Serial.println("[ERRO CAL] Assento: sem pulsos do encoder. Verifique ligacao.");
      } else if (sawAss && millis() - lastChangeAss > 500) {
        setOutputPin(Rele_SA, false, "CAL");
        doneAss = true;
        Serial.print("Assento max = "); Serial.println(incoder_virtual_asento_service);
      }
    }
    if (!donePer) {
      if (pp != prevPulsePer) {
        lastChangePer = millis();
        sawPer = true;
        prevPulsePer = pp;
      }
      if ((millis() - startPer > CAL_NO_PULSE_TIMEOUT_MS) && !sawPer) {
        setOutputPin(Rele_SP, false, "CAL");
        donePer = true;
        errPer = true;
        for (int i=0;i<3;i++){ bip(); delay(120);} delay(200); for (int i=0;i<3;i++){ bip(); delay(120);}
        Serial.println("[ERRO CAL] Perneira: sem pulsos do encoder. Verifique ligacao.");
      } else if (sawPer && millis() - lastChangePer > 500) {
        setOutputPin(Rele_SP, false, "CAL");
        donePer = true;
        Serial.print("Perneira max = "); Serial.println(incoder_virtual_perneira_service);
      }
    }
    if (!doneTrend) {
      if (pt != prevPulseTrend) {
        lastChangeTrend = millis();
        sawTrend = true;
        prevPulseTrend = pt;
        maxTrend = pt;
      }
      if ((millis() - startTrend > CAL_NO_PULSE_TIMEOUT_MS) && !sawTrend) {
        if (Rele_TREND_SOBE >= 0) setOutputPin(Rele_TREND_SOBE, false, "CAL");
        doneTrend = true;
        errTrend = true;
        for (int i=0;i<3;i++){ bip(); delay(120);} delay(200); for (int i=0;i<3;i++){ bip(); delay(120);}
        Serial.println("[ERRO CAL] Trend: sem pulsos do encoder. Verifique ligacao.");
      } else if (sawTrend && millis() - lastChangeTrend > 500) {
        if (Rele_TREND_SOBE >= 0) setOutputPin(Rele_TREND_SOBE, false, "CAL");
        doneTrend = true;
        Serial.print("Trend max = "); Serial.println(maxTrend);
      }
    }
    delay(10);
  }
  setOutputPin(Rele_DE, false, "CAL");
  setOutputPin(Rele_SA, false, "CAL");
  setOutputPin(Rele_SP, false, "CAL");
  if (Rele_TREND_SOBE >= 0) setOutputPin(Rele_TREND_SOBE, false, "CAL");
  if (Rele_TREND_DESCE >= 0) setOutputPin(Rele_TREND_DESCE, false, "CAL");
  calibrationInProgress = false;
  if (errEnc || errAss || errPer || errTrend) {
    Serial.println("[ERRO CAL] Calibracao incompleta. Pelo menos um encoder nao gerou pulsos.");
    for (int i=0;i<5;i++){ bip(); delay(120);}
  } else {
    Preferences p;
    p.begin("cadeira", false);
    p.putInt("encosto_max", incoder_virtual_encosto_service);
    p.putInt("assento_max", incoder_virtual_asento_service);
    p.putInt("perneira_max", incoder_virtual_perneira_service);
    p.putUInt("trend_max", maxTrend);
    p.putBool("cal_done", true);
    p.end();
    fim_encosto_encoder = incoder_virtual_encosto_service;
    fim_asento_encoder = incoder_virtual_asento_service;
    fim_perneira_encoder = incoder_virtual_perneira_service;
    ignoreLimitLocks = false;
    ignoreGavetaLock = false;
    trava_bt_DE = true;
    trava_bt_SA = true;
    trava_bt_SP = true;
    trava_bt_SE = false;
    trava_bt_DA = false;
    trava_bt_DP = false;
    Serial.println("[OK] Calibracao concluida e salva.");

    int encMaxOut = fim_encosto_encoder;
    int assMaxOut = fim_asento_encoder;
    int perMaxOut = fim_perneira_encoder;
    if (encMaxOut > LIMIT_STOP_MARGIN_PULSES) encMaxOut -= LIMIT_STOP_MARGIN_PULSES;
    if (assMaxOut > LIMIT_STOP_MARGIN_PULSES) assMaxOut -= LIMIT_STOP_MARGIN_PULSES;
    if (perMaxOut > LIMIT_STOP_MARGIN_PULSES) perMaxOut -= LIMIT_STOP_MARGIN_PULSES;
    if (encMaxOut < 0) encMaxOut = 0;
    if (assMaxOut < 0) assMaxOut = 0;
    if (perMaxOut < 0) perMaxOut = 0;

    String limitsMsg = String("ENC_LIMITS:MIN:0,0,0:MAX:") +
                       String(encMaxOut) + "," + String(assMaxOut) + "," + String(perMaxOut);
    enviarBLE(limitsMsg);
    mqttEnqueuePublish(MQTT_TOPIC_BASE + "tx_cmd", limitsMsg, false);
    mqttStatusDirty = true;
  }
}

// ========== EXECUÃ‡ÃƒO DA POSIÃ‡ÃƒO GINECOLÃ“GICA (VZ) ==========
void executa_vz() {
  Serial.println("VZ acionado");
  bipForce();
  buzzerPulseStart2s();

  if (isGavetaAberta()) {
    Serial.println("[GAVETA] Aberta - bloqueando VZ (perneira nao pode baixar)");
    enviarBLE("GAVETA:OPEN");
    enviarBLE("VZ:GAVETA");
    mqttEnqueuePublish(MQTT_TOPIC_BASE + "tx_cmd", "GAVETA_OPEN", false);
    mqttEnqueuePublish(MQTT_TOPIC_BASE + "tx_cmd", "VZ:GAVETA", false);
    mqttStatusDirty = true;
    cont = 0;
    buzzerPulseStop();
    AT_SEG();
    return;
  }

  int startEncPos = incoder_virtual_encosto_service;
  int startAssPos = incoder_virtual_asento_service;
  int startPerPos = incoder_virtual_perneira_service;
  bool alreadyAtVz = (!calibrationInProgress && !ignoreLimitLocks &&
                      startEncPos == 0 && startAssPos == 0 && startPerPos == 0);
  bool canEarlyFinish = !alreadyAtVz;
  uint32_t startPulseEnc = pulses_encosto;
  uint32_t startPulseAss = pulses_assento;
  uint32_t startPulsePer = pulses_perneira;
  uint32_t startTime = millis();
  Serial.print("[VZ] TIMEOUT_MS=");
  Serial.print(VZ_MAX_TIME_MS);
  Serial.print(" START ENC=");
  Serial.print(startEncPos);
  Serial.print(" ASS=");
  Serial.print(startAssPos);
  Serial.print(" PER=");
  Serial.print(startPerPos);
  Serial.println();

  if (alreadyAtVz) {
    Serial.println("[VZ] JA ESTA EM VZ");
    cont = 0;
    buzzerPulseStop();
    return;
  }

  reflectorPreMove(3, "VZ");

  setOutputPin(Rele_DA, true, "VZ");
  delay(250);
  setOutputPin(Rele_SE, true, "VZ");
  delay(250);
  setOutputPin(Rele_DP, true, "VZ");

  startPulseEnc = pulses_encosto;
  startPulseAss = pulses_assento;
  startPulsePer = pulses_perneira;
  startTime = millis();

  Serial.println("Executando VZ");
  faz_bt_seg = 1;
  uint32_t prevAbortMask = userButtonMaskRaw();
  uint32_t lastAnyPulseAt = millis();
  bool sawAnyPulse = false;
  uint32_t prevPe = pulses_encosto;
  uint32_t prevPa = pulses_assento;
  uint32_t prevPp = pulses_perneira;

  while (cont == 1) {
    Watch_Dog();
    buzzerTestTick();
    delay(1);
    
    // Verifica parada via BLE
    if (comandoBLE.length() > 0) {
      String cmd = comandoBLE;
      comandoBLE = "";
      cmd.trim();
      cmd.toUpperCase();
      if (cmd == "AT_SEG" || cmd == "STOP") {
        AT_SEG();
        cont = 0;
        contador2 = 0;
        buzzerPulseStop();
        return;
      }
    }

    uint32_t curMask = userButtonMaskRaw();
    uint32_t newPress = (curMask & ~prevAbortMask);
    prevAbortMask = curMask;
    if (newPress != 0) {
      Serial.println("[VZ] ABORTADO POR BOTAO");
      AT_SEG();
      cont = 0;
      contador2 = 0;
      buzzerPulseStop();
      ignoreButtonsUntilRelease = true;
      return;
    }
    
    Button_geral();
    contagem_tempo_incoder_virtual();
    reflectorAutoTick();

    uint32_t pe = pulses_encosto;
    uint32_t pa = pulses_assento;
    uint32_t pp = pulses_perneira;
    if (pe != prevPe || pa != prevPa || pp != prevPp) {
      sawAnyPulse = true;
      lastAnyPulseAt = millis();
      prevPe = pe;
      prevPa = pa;
      prevPp = pp;
    }
    if (sawAnyPulse && (millis() - lastAnyPulseAt) > VZ_NO_PULSE_FINISH_MS) {
      Serial.println("[VZ] FIM POR FALTA DE PULSO");
      cont = 0;
    }
    if (!sawAnyPulse && (millis() - startTime) > VZ_NO_PULSE_START_ABORT_MS) {
      Serial.println("[VZ] SEM PULSO - ASSUMINDO LIMITE");
      cont = 0;
    }

    if (canEarlyFinish &&
        incoder_virtual_encosto_service == 0 &&
        incoder_virtual_asento_service == 0 &&
        incoder_virtual_perneira_service == 0) {
      cont = 0;
    } else if (millis() - startTime >= VZ_MAX_TIME_MS) {
      cont = 0;
    }
  }

  setOutputPin(Rele_DA, false, "VZ");
  setOutputPin(Rele_SE, false, "VZ");
  setOutputPin(Rele_DP, false, "VZ");
  reflectorAutoTick();

  uint32_t dEnc = pulses_encosto - startPulseEnc;
  uint32_t dAss = pulses_assento - startPulseAss;
  uint32_t dPer = pulses_perneira - startPulsePer;
  Serial.print("[VZ] PULSOS ENC=");
  Serial.print(dEnc);
  Serial.print(" ASS=");
  Serial.print(dAss);
  Serial.print(" PER=");
  Serial.println(dPer);
  if (encReqEncosto() && ENCODER3 >= 0 && dEnc == 0) Serial.println("[ERRO ENC] Encosto sem pulso no VZ");
  if (encReqAssento() && ENCODER1 >= 0 && dAss == 0) Serial.println("[ERRO ENC] Assento sem pulso no VZ");
  if (encReqPerneira() && ENCODER2 >= 0 && dPer == 0) Serial.println("[ERRO ENC] Perneira sem pulso no VZ");

  bipForce();
  buzzerPulseStop();
  faz_bt_seg = 0;
  Serial.println("Fim VZ");
  supabaseLogUsage("VZ_DONE");
  incoder_virtual_encosto_service = 0;
  incoder_virtual_asento_service = 0;
  incoder_virtual_perneira_service = 0;
}

// ========== EXECUÃ‡ÃƒO DA POSIÃ‡ÃƒO INICIAL VZ ==========
void executa_vz_ini() {
  Serial.println("====================================");
  Serial.println("  VZ INICIAL - " + NUMERO_SERIE_CADEIRA);
  Serial.println("====================================");
  Serial.println("Executando VZ inicial - Ativando reles sequencialmente");
  Serial.println("[INFO] VZ inicial em andamento (loop principal suspenso; abort por botoes ativo)...");
  buzzerPulseStart2s();
  if (isGavetaAberta()) {
    Serial.println("[GAVETA] Aberta - bloqueando VZ_INI (perneira nao pode baixar)");
    enviarBLE("GAVETA:OPEN");
    enviarBLE("VZ_INI:GAVETA");
    mqttEnqueuePublish(MQTT_TOPIC_BASE + "tx_cmd", "GAVETA_OPEN", false);
    mqttEnqueuePublish(MQTT_TOPIC_BASE + "tx_cmd", "VZ_INI:GAVETA", false);
    mqttStatusDirty = true;
    buzzerPulseStop();
    AT_SEG();
    return;
  }

  if (!calibrationInProgress && !ignoreLimitLocks &&
      incoder_virtual_encosto_service == 0 &&
      incoder_virtual_asento_service == 0 &&
      incoder_virtual_perneira_service == 0) {
    Serial.println("[VZ_INI] JA ESTA EM VZ");
    buzzerPulseStop();
    return;
  }

  vzInicialEmAndamento = true; // Bloqueia loop() completamente

  reflectorPreMove(3, "VZ_INI");
  
  uint32_t startPulseEnc = pulses_encosto;
  uint32_t startPulseAss = pulses_assento;
  uint32_t startPulsePer = pulses_perneira;
  int startEncPos = incoder_virtual_encosto_service;
  int startAssPos = incoder_virtual_asento_service;
  int startPerPos = incoder_virtual_perneira_service;
  bool canEarlyFinish = (startEncPos != 0 || startAssPos != 0 || startPerPos != 0);
  bool doAssento = !seatTravelLearned;

  if (doAssento) {
    Serial.println("Ativando Rele_DA...");
    setOutputPin(Rele_DA, true, "VZ_INI");
    delay(500);
  }
  
  Serial.println("Ativando Rele_SE...");
  setOutputPin(Rele_SE, true, "VZ_INI");
  delay(500);
  
  Serial.println("Ativando Rele_DP...");
  setOutputPin(Rele_DP, true, "VZ_INI");
  delay(500);

  startPulseEnc = pulses_encosto;
  startPulseAss = pulses_assento;
  startPulsePer = pulses_perneira;

  Serial.print("Aguardando ");
  Serial.print(VZ_MAX_TIME_MS);
  Serial.println(" ms...");
  Serial.println("[DEBUG] Heap livre: " + String(ESP.getFreeHeap()) + " bytes");
  Serial.println("[DEBUG] Iniciando loop com feed do watchdog...");
  Serial.flush();
  
  unsigned long startTime = millis();
  int lastSecond = 0;
  uint32_t prevAbortMask = userButtonMaskRaw();
  uint32_t lastAnyPulseAt = millis();
  bool sawAnyPulse = false;
  uint32_t prevPe = pulses_encosto;
  uint32_t prevPa = pulses_assento;
  uint32_t prevPp = pulses_perneira;
  while (millis() - startTime < VZ_MAX_TIME_MS) {
    yield();  // Alimenta watchdog continuamente
    Watch_Dog();
    buzzerTestTick();
    contagem_tempo_incoder_virtual();
    reflectorAutoTick();
    uint32_t curMask = userButtonMaskRaw();
    uint32_t newPress = (curMask & ~prevAbortMask);
    prevAbortMask = curMask;
    if (newPress != 0) {
      Serial.println("[VZ_INI] ABORTADO POR BOTAO");
      AT_SEG();
      buzzerPulseStop();
      vzInicialEmAndamento = false;
      ignoreButtonsUntilRelease = true;
      return;
    }

    uint32_t pe = pulses_encosto;
    uint32_t pp = pulses_perneira;
    uint32_t pa = pulses_assento;
    bool changed = (pe != prevPe) || (pp != prevPp) || (doAssento && (pa != prevPa));
    if (changed) {
      sawAnyPulse = true;
      lastAnyPulseAt = millis();
      prevPe = pe;
      prevPp = pp;
      if (doAssento) prevPa = pa;
    }
    if (sawAnyPulse && (millis() - lastAnyPulseAt) > VZ_NO_PULSE_FINISH_MS) {
      Serial.println("[VZ_INI] FIM POR FALTA DE PULSO");
      break;
    }
    if (!sawAnyPulse && (millis() - startTime) > VZ_NO_PULSE_START_ABORT_MS) {
      Serial.println("[VZ_INI] SEM PULSO - ASSUMINDO LIMITE");
      break;
    }

    if (canEarlyFinish &&
        incoder_virtual_encosto_service == 0 &&
        incoder_virtual_asento_service == 0 &&
        incoder_virtual_perneira_service == 0) {
      break;
    }
    int currentSecond = (millis() - startTime) / 1000;
    if (currentSecond > lastSecond) {
      Serial.printf("[%d] ", currentSecond);
      Serial.flush();
      lastSecond = currentSecond;
    }
    delay(100);  // Delay pequeno para não travar CPU
  }
  
  Serial.println("\n[DEBUG] Loop completo!");
  Serial.flush();
  
  Serial.println("\nVZ inicial concluido.");
  Serial.println("Desligando reles do VZ inicial...");
  setOutputPin(Rele_DA, false, "VZ_INI");
  setOutputPin(Rele_SE, false, "VZ_INI");
  setOutputPin(Rele_DP, false, "VZ_INI");

  uint32_t dEnc = pulses_encosto - startPulseEnc;
  uint32_t dAss = pulses_assento - startPulseAss;
  uint32_t dPer = pulses_perneira - startPulsePer;
  Serial.print("[VZ_INI] PULSOS ENC=");
  Serial.print(dEnc);
  Serial.print(" ASS=");
  Serial.print(dAss);
  Serial.print(" PER=");
  Serial.println(dPer);
  if (encReqEncosto() && ENCODER3 >= 0 && dEnc == 0) Serial.println("[ERRO ENC] Encosto sem pulso no VZ_INI");
  if (doAssento && encReqAssento() && ENCODER1 >= 0 && dAss == 0) Serial.println("[ERRO ENC] Assento sem pulso no VZ_INI");
  if (encReqPerneira() && ENCODER2 >= 0 && dPer == 0) Serial.println("[ERRO ENC] Perneira sem pulso no VZ_INI");
  if (doAssento && dAss > 0 && !seatTravelLearned) {
    seatTravelLearned = true;
    Preferences p;
    if (p.begin("cadeira", false)) {
      p.putBool("ass_travel_done", true);
      p.end();
    }
    Serial.println("[OK] Percurso assento gravado com sucesso.");
  }

  Serial.println("Fim VZ inicial - Reles desligados");
  Serial.println("Resetando contadores de posicao...");
  incoder_virtual_encosto_service = 0;
  incoder_virtual_asento_service = 0;
  incoder_virtual_perneira_service = 0;
  Serial.println("Contadores resetados.");
  
  Serial.println("[INFO] VZ inicial completo (loop principal retomado).");
  vzInicialEmAndamento = false; // Libera loop() para executar normalmente
  buzzerPulseStop();
}

// ========== EXECUÃ‡ÃƒO DA POSIÃ‡ÃƒO DE PARTO (PT) ==========
void executa_pt() {
  Serial.println("PT acionado");
  bipForce();
  buzzerPulseStart2s();

  bool encAtMax = false;
  bool assAtMax = false;
  bool perAtMax = false;
  if (!calibrationInProgress && !ignoreLimitLocks) {
    int encMax = effectiveEncostoMax();
    int assMax = effectiveAssentoMax();
    int perMax = effectivePerneiraMax();
    encAtMax = (encMax > 0 && incoder_virtual_encosto_service >= encMax);
    assAtMax = (assMax > 0 && incoder_virtual_asento_service >= assMax);
    perAtMax = (perMax > 0 && incoder_virtual_perneira_service >= perMax);
  }

  if (!calibrationInProgress && !ignoreLimitLocks && encAtMax && assAtMax && perAtMax) {
    Serial.println("[PT] JA ESTA EM PT");
    cont13 = 0;
    buzzerPulseStop();
    return;
  }

  reflectorPreMove(2, "PT");

  if (!assAtMax) setOutputPin(Rele_SA, true, "PT");
  delay(250);
  if (!encAtMax) setOutputPin(Rele_DE, true, "PT");
  delay(250);
  if (!perAtMax) setOutputPin(Rele_SP, true, "PT");

  Serial.println("Executando PT");
  faz_bt_seg = 1;
  uint32_t prevAbortMask = userButtonMaskRaw();
  uint32_t startTime = millis();
  uint32_t prevPe = pulses_encosto;
  uint32_t prevPa = pulses_assento;
  uint32_t prevPp = pulses_perneira;
  uint32_t lastChangeEnc = startTime;
  uint32_t lastChangeAss = startTime;
  uint32_t lastChangePer = startTime;
  bool sawEnc = false, sawAss = false, sawPer = false;
  bool doneEnc = encAtMax, doneAss = assAtMax, donePer = perAtMax;
  bool sawAnyPulse = false;

  while (cont13 == 1) {
    Watch_Dog();
    buzzerTestTick();
    delay(1);
    
    // Verifica parada via BLE
    if (comandoBLE.length() > 0) {
      String cmd = comandoBLE;
      comandoBLE = "";
      cmd.trim();
      cmd.toUpperCase();
      if (cmd == "AT_SEG" || cmd == "STOP") {
        AT_SEG();
        cont13 = 0;
        contador = 0;
        buzzerPulseStop();
        return;
      }
    }

    uint32_t curMask = userButtonMaskRaw();
    uint32_t newPress = (curMask & ~prevAbortMask);
    prevAbortMask = curMask;
    if (newPress != 0) {
      Serial.println("[PT] ABORTADO POR BOTAO");
      AT_SEG();
      cont13 = 0;
      contador = 0;
      buzzerPulseStop();
      ignoreButtonsUntilRelease = true;
      return;
    }
    
    Button_geral();
    contagem_tempo_incoder_virtual();
    reflectorAutoTick();
    uint32_t now = millis();

    uint32_t pe = pulses_encosto;
    uint32_t pa = pulses_assento;
    uint32_t pp = pulses_perneira;

    if (pe != prevPe) {
      prevPe = pe;
      lastChangeEnc = now;
      sawEnc = true;
      sawAnyPulse = true;
    }
    if (pa != prevPa) {
      prevPa = pa;
      lastChangeAss = now;
      sawAss = true;
      sawAnyPulse = true;
    }
    if (pp != prevPp) {
      prevPp = pp;
      lastChangePer = now;
      sawPer = true;
      sawAnyPulse = true;
    }

    if (!doneEnc && sawEnc && (now - lastChangeEnc) > VZ_NO_PULSE_FINISH_MS) {
      doneEnc = true;
      setOutputPin(Rele_DE, false, "PT");
    }
    if (!doneAss && sawAss && (now - lastChangeAss) > VZ_NO_PULSE_FINISH_MS) {
      doneAss = true;
      setOutputPin(Rele_SA, false, "PT");
    }
    if (!donePer && sawPer && (now - lastChangePer) > VZ_NO_PULSE_FINISH_MS) {
      donePer = true;
      setOutputPin(Rele_SP, false, "PT");
    }

    if (!sawAnyPulse && (now - startTime) > VZ_NO_PULSE_START_ABORT_MS) {
      Serial.println("[PT] SEM PULSO - ASSUMINDO LIMITE");
      cont13 = 0;
    } else if (doneEnc && doneAss && donePer) {
      cont13 = 0;
    } else if ((now - startTime) > VZ_MAX_TIME_MS) {
      cont13 = 0;
    }
  }

  setOutputPin(Rele_SA, false, "PT");
  setOutputPin(Rele_DE, false, "PT");
  setOutputPin(Rele_SP, false, "PT");
  cont13 = 0;
  reflectorAutoTick();

  bipForce();
  buzzerPulseStop();
  faz_bt_seg = 0;
  Serial.println("Fim PT");
}

// ========== EXECUÃ‡ÃƒO DA MEMÃ“RIA M1 (via botÃ£o fÃ­sico) ==========
void executa_M1() {
  buttonState = digitalRead(M1);
  while (buttonState == LOW) {
    buttonState = digitalRead(M1);
    cont12++;

    if (cont12 < 500) {
      Serial.println("Contando tempo botao Memoria 1");
      Serial.print("cont12 = ");
      Serial.println(cont12);
    } else {
      Serial.println("Executando M1");
      faz_m1 = true;
      bip();
      delay(200);
      bip();
      delay(200);
      cont12 = 0;
      reflectorPreMove(2, "M1");

      Serial.println("Executando Memoria 1");
      Serial.print("Posicao do encosto = ");
      Serial.println(incoder_virtual_encosto_service);
      Serial.print("Posicao do assento = ");
      Serial.println(incoder_virtual_asento_service);
      Serial.print("Posicao da perneira = ");
      Serial.println(incoder_virtual_perneira_service);
    }
  }

  cont12 = 0;

  // Se pressionou rÃ¡pido, salva a posiÃ§Ã£o atual
  if (faz_m1 == false) {
    incoder_virtual_encosto_M1 = incoder_virtual_encosto_service;
    incoder_virtual_asento_M1 = incoder_virtual_asento_service;
    incoder_virtual_perneira_M1 = incoder_virtual_perneira_service;

    preferences.begin("encoder_encosto", false);
    preferences.putInt("encoder_encosto", incoder_virtual_encosto_M1);
    preferences.end();
    
    preferences.begin("encoder_asento", false);
    preferences.putInt("encoder_asento", incoder_virtual_asento_M1);
    preferences.end();
    
    preferences.begin("encoder_pern", false);
    preferences.putInt("encoder_perneira", incoder_virtual_perneira_M1);
    preferences.end();

    Serial.println("Posicoes de memoria gravadas");
    Serial.print("Posicao do encosto na memoria = ");
    Serial.println(incoder_virtual_encosto_M1);
    Serial.print("Posicao do assento na memoria = ");
    Serial.println(incoder_virtual_asento_M1);
    Serial.print("Posicao da perneira na memoria = ");
    Serial.println(incoder_virtual_perneira_M1);

    enviarBLE("M1:SAVED");
    supabaseSaveMemoryPosition(1);

    bip();
    delay(200);
    bip();
  }

  // Se segurou o botÃ£o, executa o movimento para a posiÃ§Ã£o salva
  while (faz_m1 == true) {
    Watch_Dog();
    buzzerTestTick();
    contagem_tempo_incoder_virtual();
    reflectorAutoTick();

    faz_bt_seg = 1;
    Button_Seg();

    if (faz_bt_seg == 0) {
      bip();
      delay(700);
      bip();
      faz_m1 = false;
      return;
    }

    if (ENCODER3 < 0) {
      setOutputPin(Rele_DE, false, "M1");
      setOutputPin(Rele_SE, false, "M1");
      en_DE_acabou = true;
      en_SE_acabou = true;
    } else {
      if (incoder_virtual_encosto_service < incoder_virtual_encosto_M1) {
        setOutputPin(Rele_DE, true, "M1");
        setOutputPin(Rele_SE, false, "M1");
        en_DE_acabou = false;
        en_SE_acabou = false;
      } else if (incoder_virtual_encosto_service > incoder_virtual_encosto_M1) {
        setOutputPin(Rele_SE, true, "M1");
        setOutputPin(Rele_DE, false, "M1");
        en_DE_acabou = false;
        en_SE_acabou = false;
      } else {
        setOutputPin(Rele_DE, false, "M1");
        setOutputPin(Rele_SE, false, "M1");
        en_DE_acabou = true;
        en_SE_acabou = true;
      }
    }

    if (ENCODER1 < 0) {
      setOutputPin(Rele_SA, false, "M1");
      setOutputPin(Rele_DA, false, "M1");
      en_SA_acabou = true;
      en_DA_acabou = true;
    } else {
      if (incoder_virtual_asento_service < incoder_virtual_asento_M1) {
        setOutputPin(Rele_SA, true, "M1");
        setOutputPin(Rele_DA, false, "M1");
        en_SA_acabou = false;
        en_DA_acabou = false;
      } else if (incoder_virtual_asento_service > incoder_virtual_asento_M1) {
        setOutputPin(Rele_DA, true, "M1");
        setOutputPin(Rele_SA, false, "M1");
        en_SA_acabou = false;
        en_DA_acabou = false;
      } else {
        setOutputPin(Rele_SA, false, "M1");
        setOutputPin(Rele_DA, false, "M1");
        en_SA_acabou = true;
        en_DA_acabou = true;
      }
    }

    if (ENCODER2 < 0) {
      setOutputPin(Rele_SP, false, "M1");
      setOutputPin(Rele_DP, false, "M1");
      en_SP_acabou = true;
      en_DP_acabou = true;
    } else {
      if (incoder_virtual_perneira_service < incoder_virtual_perneira_M1) {
        setOutputPin(Rele_SP, true, "M1");
        setOutputPin(Rele_DP, false, "M1");
        en_SP_acabou = false;
        en_DP_acabou = false;
      } else if (incoder_virtual_perneira_service > incoder_virtual_perneira_M1) {
        setOutputPin(Rele_DP, true, "M1");
        setOutputPin(Rele_SP, false, "M1");
        en_SP_acabou = false;
        en_DP_acabou = false;
      } else {
        setOutputPin(Rele_SP, false, "M1");
        setOutputPin(Rele_DP, false, "M1");
        en_SP_acabou = true;
        en_DP_acabou = true;
      }
    }

    // Verifica se todos os movimentos terminaram
    if (en_SE_acabou == true && en_DE_acabou == true && en_SA_acabou == true && en_DA_acabou == true && en_SP_acabou == true && en_DP_acabou == true) {
      faz_m1 = false;
      en_SP_acabou = false;
      en_DP_acabou = false;
      en_SE_acabou = false;
      en_DE_acabou = false;
      en_SA_acabou = false;
      en_DA_acabou = false;

      bip();
      delay(500);
      bip();

      Serial.println("Fim do movimento da memoria 1");
      enviarBLE("M1:DONE");
      return;
    }
    delay(5);
  }
}

// ========== PARADA DE EMERGÃŠNCIA (AT_SEG) ==========
void AT_SEG() {
  Serial.println("Parando todos os movimentos");
  enviarBLE("AT_SEG:STOPPING");

  setOutputPin(Rele_DE, false, "AT_SEG");
  setOutputPin(Rele_SE, false, "AT_SEG");
  setOutputPin(Rele_SA, false, "AT_SEG");
  setOutputPin(Rele_DA, false, "AT_SEG");
  setOutputPin(Rele_SP, false, "AT_SEG");
  setOutputPin(Rele_DP, false, "AT_SEG");
  if (Rele_TREND_SOBE >= 0) setOutputPin(Rele_TREND_SOBE, false, "AT_SEG");
  if (Rele_TREND_DESCE >= 0) setOutputPin(Rele_TREND_DESCE, false, "AT_SEG");

  estado_de = false;
  estado_se = false;
  estado_sa = false;
  estado_da = false;
  estado_sp = false;
  estado_dp = false;
  estado_trend_sobe = false;
  estado_trend_desce = false;
  trendRemoteActive = false;
  lastTrendCmdMs = 0;

  faz_bt_seg = 0;
  cont = 0;
  cont13 = 0;
  delay(100);
}

// ========== BIP (buzzer) ==========
void bip() {
#if OFFLINE_DEMO
  if (!buzzerForceOnce) return;
#endif
  buzzerForceOnce = false;
  digitalWrite(BUZZER, HIGH);
  delay(100);
  digitalWrite(BUZZER, LOW);
}

void bipLong() {
#if OFFLINE_DEMO
  return;
#endif
  digitalWrite(BUZZER, HIGH);
  delay(400);
  digitalWrite(BUZZER, LOW);
}

void bipForce() {
  buzzerForceOnce = true;
  bip();
}

// ========== MONITORAMENTO DO SISTEMA (DEBUG) ==========
void monitoraSistema() {
  if (millis() - ultimoMonitoramentoSistema < INTERVALO_MONITORAMENTO) return;
  ultimoMonitoramentoSistema = millis();

  Serial.println("--- MONITORAMENTO DO SISTEMA ---");
  Serial.print("Free Heap: "); Serial.print(ESP.getFreeHeap() / 1024); Serial.println(" KB");
  Serial.print("Min Free Heap: "); Serial.print(ESP.getMinFreeHeap() / 1024); Serial.println(" KB");
  Serial.print("WiFi Status: "); 
  switch(WiFi.status()) {
    case WL_CONNECTED: Serial.println("CONECTADO"); break;
    case WL_DISCONNECTED: Serial.println("DESCONECTADO"); break;
    case WL_CONNECTION_LOST: Serial.println("CONEXAO PERDIDA"); break;
    default: Serial.println(WiFi.status()); break;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("IP: "); Serial.println(WiFi.localIP());
    Serial.print("RSSI: "); Serial.println(WiFi.RSSI());
  }
#if HAS_BLE
  Serial.print("BLE Status: "); 
  Serial.println(bleClienteConectado ? "CONECTADO" : "AGUARDANDO CONEXAO");
  Serial.print("BLE Advertising: ");
  Serial.println(bleAdvertisingAtivo ? "ATIVO" : "PARADO");
  if (bleClienteConectado && pServer != NULL) {
    Serial.print("BLE Clientes: "); 
    Serial.println(pServer->getConnectedCount());
    Serial.print("BLE TX FAIL: ");
    Serial.print(bleTxFailCount);
    Serial.print(" lastRc=");
    Serial.print(bleTxLastErrRc);
    Serial.print(" suspendMs=");
    uint32_t now = millis();
    if (bleTxSuspendUntilMs != 0 && static_cast<int32_t>(now - bleTxSuspendUntilMs) < 0) {
      Serial.println((uint32_t)(bleTxSuspendUntilMs - now));
    } else {
      Serial.println(0);
    }
  }
#else
  Serial.println("BLE Status: NA");
#endif
  Serial.print("Horimetro: "); Serial.println(horimetro);
  Serial.print("Cadeira Habilitada: "); Serial.println(cadeiraHabilitada ? "SIM" : "NAO");
  if (!cadeiraHabilitada && !buzzerTestEnabled) {
    bip(); delay(120);
    bip(); delay(120);
    bip();
  }
  Serial.print("MQTT Status: ");
  bool mqttConnected = mqttClient.connected();
  String hostSnapshot;
  uint16_t portSnapshot = 0;
  bool tlsSnapshot = false;
  String clientIdSnapshot;
  bool cleanSnapshot = true;
  if (mqttLockMs(20)) {
    hostSnapshot = mqttHost;
    portSnapshot = mqttPort;
    tlsSnapshot = mqttUseTls;
    clientIdSnapshot = mqttClientId;
    cleanSnapshot = mqttCleanSession;
    mqttUnlock();
  } else {
    hostSnapshot = mqttHost;
    portSnapshot = mqttPort;
    tlsSnapshot = mqttUseTls;
    clientIdSnapshot = mqttClientId;
    cleanSnapshot = mqttCleanSession;
  }
  Serial.print(mqttConnected ? "CONECTADO " : "DESCONECTADO ");
  Serial.print("HOST=");
  Serial.print(hostSnapshot);
  Serial.print(" PORT=");
  Serial.print(portSnapshot);
  Serial.print(" TLS=");
  Serial.println(tlsSnapshot ? "SIM" : "NAO");
  Serial.print("MQTT CLIENT_ID: ");
  Serial.println(clientIdSnapshot.length() > 0 ? clientIdSnapshot : ("ESP32-" + NUMERO_SERIE_CADEIRA));
  Serial.print("MQTT CLEAN: ");
  Serial.println(cleanSnapshot ? "1" : "0");
  Serial.print("MQTT RX: "); Serial.print(mqttRxCount);
  Serial.print(" TX: "); Serial.println(mqttTxCount);
  if (mqttLastRxMs) {
    Serial.print("Ultimo MQTT RX: ");
    Serial.print((millis() - mqttLastRxMs) / 1000);
    Serial.println("s");
  }
  if (mqttLastTxMs) {
    Serial.print("Ultimo MQTT TX: ");
    Serial.print((millis() - mqttLastTxMs) / 1000);
    Serial.println("s");
  }
  
  // Contadores de tempo para próximas atualizações
  unsigned long tempoAtual = millis();
  unsigned long tempoAteProximoHorimetro = 0;
  unsigned long tempoAteProximaVerificacao = 0;
  
  if (tempoAtual - ultimaAtualizacaoSupabase < INTERVALO_ATUALIZACAO_SUPABASE) {
    tempoAteProximoHorimetro = (INTERVALO_ATUALIZACAO_SUPABASE - (tempoAtual - ultimaAtualizacaoSupabase)) / 1000;
  }
  
  if (tempoAtual - ultimaVerificacaoStatus < INTERVALO_VERIFICACAO_STATUS) {
    tempoAteProximaVerificacao = (INTERVALO_VERIFICACAO_STATUS - (tempoAtual - ultimaVerificacaoStatus)) / 1000;
  }
  
  Serial.print("Proximo envio horimetro: "); Serial.print(tempoAteProximoHorimetro); Serial.println("s");
  Serial.print("Proxima verificacao status: "); Serial.print(tempoAteProximaVerificacao); Serial.println("s");
  
#if USE_PCF8574
  // Removido a impressao constante daqui
#endif

  Serial.println("--------------------------------");
}

