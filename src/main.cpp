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
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <rom/rtc.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_task_wdt.h"
#include <time.h>

// ========== CONFIGURAÇÕES SUPABASE ==========
// ⚠️ ATENÇÃO: CREDENCIAIS PRIVADAS
// ⚠️ ESTE REPOSITÓRIO DEVE SER MANTIDO COMO PRIVADO
// ⚠️ NÃO compartilhe estas credenciais publicamente
// 
// IMPORTANTE: Estas são credenciais sensíveis que controlam
// o acesso ao banco de dados Supabase. Mantenha-as seguras!
//
// Para configurar suas próprias credenciais:
// 1. Acesse https://app.supabase.com
// 2. Vá em Settings > API do seu projeto
// 3. Copie a URL e a chave anon/public
// 4. Substitua os valores abaixo
//
const char* SUPABASE_URL = "https://mkoqceekhnkpviixqnnk.supabase.co";
const char* SUPABASE_KEY = "sb_publishable_HLUfLEw2UuIWjzd5LfqLkw_oaodzV7V";
const char* SENHA_AP = "12345678";                  // Senha da rede de configuração (mínimo 8 caracteres)
// ============================================

// Número de série único baseado no MAC ID do ESP32
String NUMERO_SERIE_CADEIRA = "";
String NOME_DISPOSITIVO = "";  // Nome único: CadeiraOdonto-XXXX (usado em WiFi e Bluetooth)

Preferences preferences;
// BLE Server e Características
BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristicTX = NULL;
BLECharacteristic* pCharacteristicRX = NULL;
bool bleClienteConectado = false;
String comandoBLE = "";

// UUIDs para o serviço BLE (Nordic UART Service)
#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E" // Recebe comandos
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E" // Envia respostas
WiFiManager wifiManager;

// VariÃ¡veis de controle Supabase
bool cadeiraHabilitada = true;
bool manutencaoNecessaria = false;
unsigned long ultimaAtualizacaoSupabase = 0;
const unsigned long INTERVALO_ATUALIZACAO_SUPABASE = 60000;  // Atualiza a cada 1 minuto
unsigned long ultimaVerificacaoStatus = 0;
const unsigned long INTERVALO_VERIFICACAO_STATUS = 300000;   // Verifica status a cada 5 minutos
unsigned long ultimoMonitoramentoSistema = 0;
const unsigned long INTERVALO_MONITORAMENTO = 10000;         // Monitora a cada 10 segundos

// HorÃ­metro
float horimetro = 0;
unsigned long tempoInicioMotor = 0;
bool motorLigado = false;
unsigned long totalMillisMotor = 0;

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
  <img class="logo-img" src="data:image/webp;base64,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" alt="VIP Hospitalar">
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
void executaComandoBluetooth(String cmd);
void enviaStatusBluetooth();
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
void Button_geral();
void executa_vz();
void executa_vz_ini();
void executa_M1();
void executa_pt();
void AT_SEG();
void bip();
void verificaBotaoResetWifi();
void resetaConfiguracoesWifi();

// ========== CALLBACKS BLE ==========
class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    bleClienteConectado = true;
    Serial.println("\n====================================");
    Serial.println("  [BLE] DISPOSITIVO CONECTADO!");
    Serial.println("====================================");
    Serial.print("Total de clientes conectados: ");
    Serial.println(pServer->getConnectedCount());
    Serial.println("====================================\n");
  };

  void onDisconnect(BLEServer* pServer) {
    bleClienteConectado = false;
    Serial.println("\n====================================");
    Serial.println("  [BLE] DISPOSITIVO DESCONECTADO");
    Serial.println("====================================");
    delay(500);
    pServer->startAdvertising(); // Reinicia advertising
    Serial.println("[BLE] Advertising reiniciado");
    Serial.println("Aguardando nova conexao...");
    Serial.println("====================================\n");
  }
};

class MyCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string rxValue = pCharacteristic->getValue();
    
    if (rxValue.length() > 0) {
      comandoBLE = "";
      for (int i = 0; i < rxValue.length(); i++) {
        comandoBLE += rxValue[i];
      }
      comandoBLE.trim();
      Serial.print("[BLE] Comando recebido: ");
      Serial.println(comandoBLE);
    }
  }
};

void enviarBLE(String msg) {
  if (bleClienteConectado && pCharacteristicTX != NULL) {
    pCharacteristicTX->setValue(msg.c_str());
    pCharacteristicTX->notify();
    Serial.print("[BLE] Enviado: ");
    Serial.println(msg);
  }
}

// Constantes - Pinos de ENTRADA (botÃµes)
const int DE = 5;
const int SA = 34;
const int DA = 17;
const int SE = 13;
const int VZ = 22;
const int SP = 18;
const int DP = 19;
const int PT = 21;
const int RF = 23;
const int M1 = 15;

// Constantes - Pinos de SAÃDA (relÃ©s e indicadores)
const int Rele_SA = 33;
const int Rele_DA = 25;
const int Rele_SE = 27;
const int Rele_DE = 26;
const int Rele_SP = 12;
const int Rele_DP = 14;
const int LED = 02;
const int AD_PIC = 16;
const int Rele_refletor = 4;
const int BUZZER = 32;

// BotÃ£o para resetar WiFi (segure RF por 5 segundos)
const int BOTAO_RESET_WIFI = RF;

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
int const fim_encosto_encoder = 50;
int const fim_asento_encoder = 50;
int const fim_perneira_encoder = 50;

int cont13 = 0;

bool estado_r = 0;
bool estado_sp = 0;
bool estado_dp = 0;
bool estado_sa = 0;
bool estado_da = 0;
bool estado_se = 0;
bool estado_de = 0;
bool faz_bt_seg = 0;

// Debounce para comandos BLE
unsigned long ultimoComandoBLE = 0;
String ultimoCmdBLE = "";

// Timeout para motores (dead man's switch) - desliga se não receber comando
const unsigned long MOTOR_TIMEOUT = 1000; // 1 segundo
unsigned long ultimoComandoSE = 0;
unsigned long ultimoComandoDE = 0;
unsigned long ultimoComandoSA = 0;
unsigned long ultimoComandoDA = 0;
unsigned long ultimoComandoSP = 0;
unsigned long ultimoComandoDP = 0;

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
const unsigned long INTERVALO_ENCODER = 250;
bool habilitaEncoderVirtual = true; // Controle para desabilitar encoder durante VZ inicial
bool vzInicialEmAndamento = false; // Flag para bloquear loop() durante VZ inicial

void monitoraSistema();

void print_reset_reason(RESET_REASON reason) {
  switch (reason) {
    case 1 : Serial.println("POWERON_RESET");break;          /**<1, Vbat power on reset*/
    case 3 : Serial.println("SW_RESET");break;               /**<3, Software reset digital core*/
    case 4 : Serial.println("OWDT_RESET");break;             /**<4, Legacy watch dog reset digital core*/
    case 5 : Serial.println("DEEPSLEEP_RESET");break;        /**<5, Deep Sleep reset digital core*/
    case 6 : Serial.println("SDIO_RESET");break;             /**<6, Reset by SLC module, reset digital core*/
    case 7 : Serial.println("TG0WDT_SYS_RESET");break;       /**<7, Timer Group0 Watch dog reset digital core*/
    case 8 : Serial.println("TG1WDT_SYS_RESET");break;       /**<8, Timer Group1 Watch dog reset digital core*/
    case 9 : Serial.println("RTCWDT_SYS_RESET");break;       /**<9, RTC Watch dog Reset digital core*/
    case 10: Serial.println("INTRUSION_RESET");break;        /**<10, Instr CPU reset digital core*/
    case 11: Serial.println("TGWDT_CPU_RESET");break;        /**<11, Time Group reset CPU*/
    case 12: Serial.println("SW_CPU_RESET");break;           /**<12, Software reset CPU*/
    case 13: Serial.println("RTCWDT_CPU_RESET");break;       /**<13, RTC Watch dog Reset CPU*/
    case 14: Serial.println("EXT_CPU_RESET");break;          /**<14, for APP CPU, reseted by PRO CPU*/
    case 15: Serial.println("BROWN_OUT_RESET");break;        /**<15, Reset when the vdd voltage is not stable*/
    case 16: Serial.println("RTCWDT_RTC_RESET");break;       /**<16, RTC Watch dog reset digital core and rtc module*/
    default: Serial.println("NO_MEAN");
  }
}

// ========== SETUP ==========
void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // Desabilita detector de Brownout
  setCpuFrequencyMhz(80); // Reduz frequÃªncia para 80MHz durante inicializaÃ§Ã£o
  
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n--- INICIALIZANDO SISTEMA ---");
  Serial.print("CPU0 Reset Reason: "); print_reset_reason(rtc_get_reset_reason(0));
  Serial.print("CPU1 Reset Reason: "); print_reset_reason(rtc_get_reset_reason(1));
  
  // Gera nÃºmero de sÃ©rie Ãºnico baseado no MAC ID do ESP32
  Serial.println("Gerando numero de serie...");
  NUMERO_SERIE_CADEIRA = geraNumeroSerieDoMAC();
  Serial.print("Numero de Serie: ");
  Serial.println(NUMERO_SERIE_CADEIRA);
  
  // Gera nome do dispositivo (últimos 4 dígitos do MAC)
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char nomeTemp[20];
  snprintf(nomeTemp, sizeof(nomeTemp), "CadeiraOdonto-%02X%02X", mac[4], mac[5]);
  NOME_DISPOSITIVO = String(nomeTemp);
  Serial.print("Nome do Dispositivo: ");
  Serial.println(NOME_DISPOSITIVO);
  delay(500);
  
  // ConfiguraÃ§Ã£o de pinos primeiro (menor consumo)
  Serial.println("Configurando pinos GPIO...");

  // Inicializa pinos de entrada com pull-up
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
  pinMode(AD_PIC, INPUT);

  // Inicializa pinos de saÃ­da
  pinMode(Rele_SA, OUTPUT);
  pinMode(Rele_DA, OUTPUT);
  pinMode(Rele_SE, OUTPUT);
  pinMode(Rele_DE, OUTPUT);
  pinMode(Rele_DP, OUTPUT);
  pinMode(Rele_SP, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(LED, OUTPUT);
  pinMode(Rele_refletor, OUTPUT);

  // Desliga todos os relÃ©s no inÃ­cio
  digitalWrite(Rele_SA, LOW);
  digitalWrite(Rele_DA, LOW);
  digitalWrite(Rele_SE, LOW);
  digitalWrite(Rele_DE, LOW);
  digitalWrite(Rele_DP, LOW);
  digitalWrite(Rele_SP, LOW);
  digitalWrite(Rele_refletor, LOW);
  Serial.println("Pinos configurados.");
  delay(500);

  // ===== EXECUTA VZ INICIAL ANTES DE TUDO =====
  Serial.println("\n====================================");
  Serial.println("  EXECUTANDO VZ INICIAL");
  Serial.println("  (ANTES de WiFi e Bluetooth)");
  Serial.println("====================================");
  executa_vz_ini();
  Serial.println("[OK] VZ inicial finalizado.");
  Serial.println("====================================\n");
  delay(1000);
  Serial.println("Inicio programa cadeira GO - Com WiFiManager + Supabase");

  // Carrega dados salvos (encoder virtual e horÃ­metro)
  Serial.println("Carregando preferencias...");
  carregaPreferencias();
  delay(500);

  // ===== INICIALIZA BLE =====
  Serial.println("\n====================================");
  Serial.println("  INICIALIZANDO BLE");
  Serial.println("====================================");
  Serial.print("Inicializando BLE com nome: ");
  Serial.println(NOME_DISPOSITIVO);
  
  BLEDevice::init(NOME_DISPOSITIVO.c_str());
  
  // Cria o BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  
  // Cria o BLE Service (Nordic UART Service)
  BLEService *pService = pServer->createService(SERVICE_UUID);
  
  // Cria a Característica TX (notificação para enviar dados ao app)
  pCharacteristicTX = pService->createCharacteristic(
    CHARACTERISTIC_UUID_TX,
    BLECharacteristic::PROPERTY_NOTIFY
  );
  pCharacteristicTX->addDescriptor(new BLE2902());
  
  // Cria a Característica RX (escrita para receber comandos do app)
  pCharacteristicRX = pService->createCharacteristic(
    CHARACTERISTIC_UUID_RX,
    BLECharacteristic::PROPERTY_WRITE
  );
  pCharacteristicRX->setCallbacks(new MyCallbacks());
  
  // Inicia o serviço
  pService->start();
  
  // Configura e inicia o advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);  // iPhone requer isso
  pAdvertising->setMaxPreferred(0x12);
  BLEDevice::startAdvertising();
  
  Serial.println("[OK] BLE iniciado e advertising ativo!");
  Serial.print("Nome BLE: ");
  Serial.println(NOME_DISPOSITIVO);
  Serial.println("UUID do Serviço: 6E400001-B5A3-F393-E0A9-E50E24DCCA9E");
  Serial.println("Aguardando conexão de cliente BLE...");
  Serial.println("====================================\n");
  delay(1000);

  // Volta CPU para 240MHz antes do WiFi
  setCpuFrequencyMhz(240);
  Serial.println("CPU em 240MHz - Iniciando WiFi...");
  delay(1000);

  // Configura e conecta WiFi usando WiFiManager
  configuraWiFiManager();

  // Delay para estabilizar rede
  Serial.println("Aguardando estabilizacao da rede...");
  delay(2000);

  // Sincroniza horário com servidor NTP
  if (WiFi.status() == WL_CONNECTED) {
    sincronizaNTP();
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
  
  Serial.println("\n====================================");
  Serial.println("       SISTEMA PRONTO!");
  Serial.println("====================================");
  Serial.print("Nome: ");
  Serial.println(NOME_DISPOSITIVO);
  Serial.print("Serial: ");
  Serial.println(NUMERO_SERIE_CADEIRA);
  Serial.print("WiFi: ");
  Serial.println(WiFi.status() == WL_CONNECTED ? "ONLINE" : "OFFLINE");
  Serial.print("Memoria livre: ");
  Serial.print(ESP.getFreeHeap());
  Serial.println(" bytes");
  Serial.println("====================================");
  
  bip();
  delay(100);
  bip();
}

// ========== MONITORAMENTO DE TIMEOUT DOS MOTORES (DEAD MAN'S SWITCH) ==========
void verificaTimeoutMotores() {
  unsigned long agora = millis();
  
  // SE - Encosto sobe
  if (estado_se && (agora - ultimoComandoSE) > MOTOR_TIMEOUT) {
    estado_se = false;
    digitalWrite(Rele_SE, LOW);
    faz_bt_seg = 0;
    enviarBLE("SE:TIMEOUT");
    Serial.println("[TIMEOUT] Motor SE desligado por seguranca");
  }
  
  // DE - Encosto desce
  if (estado_de && (agora - ultimoComandoDE) > MOTOR_TIMEOUT) {
    estado_de = false;
    digitalWrite(Rele_DE, LOW);
    faz_bt_seg = 0;
    enviarBLE("DE:TIMEOUT");
    Serial.println("[TIMEOUT] Motor DE desligado por seguranca");
  }
  
  // SA - Assento sobe
  if (estado_sa && (agora - ultimoComandoSA) > MOTOR_TIMEOUT) {
    estado_sa = false;
    digitalWrite(Rele_SA, LOW);
    faz_bt_seg = 0;
    enviarBLE("SA:TIMEOUT");
    Serial.println("[TIMEOUT] Motor SA desligado por seguranca");
  }
  
  // DA - Assento desce
  if (estado_da && (agora - ultimoComandoDA) > MOTOR_TIMEOUT) {
    estado_da = false;
    digitalWrite(Rele_DA, LOW);
    faz_bt_seg = 0;
    enviarBLE("DA:TIMEOUT");
    Serial.println("[TIMEOUT] Motor DA desligado por seguranca");
  }
  
  // SP - Pernas sobem
  if (estado_sp && (agora - ultimoComandoSP) > MOTOR_TIMEOUT) {
    estado_sp = false;
    digitalWrite(Rele_SP, LOW);
    faz_bt_seg = 0;
    enviarBLE("SP:TIMEOUT");
    Serial.println("[TIMEOUT] Motor SP desligado por seguranca");
  }
  
  // DP - Pernas descem
  if (estado_dp && (agora - ultimoComandoDP) > MOTOR_TIMEOUT) {
    estado_dp = false;
    digitalWrite(Rele_DP, LOW);
    faz_bt_seg = 0;
    enviarBLE("DP:TIMEOUT");
    Serial.println("[TIMEOUT] Motor DP desligado por seguranca");
  }
}

// ========== LOOP PRINCIPAL ==========
void loop() {
  // Se VZ inicial estiver em andamento, nÃ£o executa nada do loop
  if (vzInicialEmAndamento) {
    delay(100);
    return;
  }
  
  // Verifica se botÃ£o de reset WiFi foi pressionado por 5 segundos
  verificaBotaoResetWifi();
  
  // Verifica timeout dos motores (dead man's switch)
  verificaTimeoutMotores();
  
  // Processa comandos Bluetooth do app
  processaComandosBluetooth();

  // Monitoramento do sistema (Debug)
  monitoraSistema();

  // Atualiza horÃ­metro
  atualizaHorimetro();

  // Envia dados ao Supabase periodicamente
  atualizaSupabase();

  // Verifica status da cadeira periodicamente
  verificacaoPeriodicaStatus();

  // FunÃ§Ãµes originais de controle
  contagem_tempo_incoder_virtual();
  Watch_Dog();
  Button_Seg();
  Button_geral();
  monitora_tempo_rele();
}

// ========== GERAÃ‡ÃƒO DO NÃšMERO DE SÃ‰RIE BASEADO NO MAC ==========
String geraNumeroSerieDoMAC() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  
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
    
    // 3 bips indicam falha
    bip(); delay(200);
    bip(); delay(200);
    bip();
  } else {
    Serial.println("WiFi conectado!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("SSID: ");
    Serial.println(WiFi.SSID());
    
    // 2 bips indicam sucesso
    bip(); delay(100);
    bip();
  }
}

// Verifica se botÃ£o de reset WiFi estÃ¡ pressionado por 5 segundos
void verificaBotaoResetWifi() {
  if (digitalRead(BOTAO_RESET_WIFI) == LOW) {
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
  
  Serial.println("ConfiguraÃ§Ãµes WiFi apagadas. Reiniciando...");
  delay(1000);
  
  // Reinicia o ESP32
  ESP.restart();
}

// ========== BLUETOOTH - PROCESSA COMANDOS DO APP ==========
void processaComandosBluetooth() {
  // Processa comandos BLE (recebidos via callback)
  if (comandoBLE.length() > 0) {
    String cmd = comandoBLE;
    comandoBLE = ""; // Limpa comando processado
    cmd.trim();
    cmd.toUpperCase();
    
    // Debounce apenas para comandos de estado simples (não motores)
    // Motores podem precisar ser acionados repetidamente
    bool isMotorCmd = (cmd == "SE" || cmd == "DE" || cmd == "SA" || 
                       cmd == "DA" || cmd == "SP" || cmd == "DP");
    
    if (!isMotorCmd) {
      unsigned long agora = millis();
      if (cmd == ultimoCmdBLE && (agora - ultimoComandoBLE) < 300) {
        Serial.println("[BLE] Comando duplicado ignorado: " + cmd);
        return;
      }
      ultimoCmdBLE = cmd;
      ultimoComandoBLE = agora;
    }
    
    executaComandoBluetooth(cmd);
  }
}

void executaComandoBluetooth(String cmd) {
  cmd.trim();
  cmd.toUpperCase();
  
  Serial.print("Comando BT recebido: ");
  Serial.println(cmd);

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
    return;
  }

  // ===== COMANDOS DE CONTROLE DA CADEIRA =====
  if (cmd == "RF") {
    // Liga/desliga refletor
    estado_r = !estado_r;
    digitalWrite(Rele_refletor, estado_r);
    enviarBLE(estado_r ? "RF:ON" : "RF:OFF");
    Serial.println("Refletor alternado via BLE");
  }
  else if (cmd == "SE") {
    // Encosto sobe (sentar) - Dead man's switch
    if (!trava_bt_SE) {
      ultimoComandoSE = millis();
      if (!estado_se) {
        estado_se = true;
        digitalWrite(Rele_SE, HIGH);
        faz_bt_seg = 1;
        enviarBLE("SE:ON");
        iniciaTimerMotor();
      } else {
        enviarBLE("SE:KEEP"); // Motor já ligado, mantendo
      }
    } else {
      enviarBLE("SE:LIMIT");
    }
  }
  else if (cmd == "DE") {
    // Encosto desce (deitar) - Dead man's switch
    if (!trava_bt_DE) {
      ultimoComandoDE = millis();
      if (!estado_de) {
        estado_de = true;
        digitalWrite(Rele_DE, HIGH);
        faz_bt_seg = 1;
        enviarBLE("DE:ON");
        iniciaTimerMotor();
      } else {
        enviarBLE("DE:KEEP");
      }
    } else {
      enviarBLE("DE:LIMIT");
    }
  }
  else if (cmd == "SA") {
    // Assento sobe - Dead man's switch
    if (!trava_bt_SA) {
      ultimoComandoSA = millis();
      if (!estado_sa) {
        estado_sa = true;
        digitalWrite(Rele_SA, HIGH);
        faz_bt_seg = 1;
        enviarBLE("SA:ON");
        iniciaTimerMotor();
      } else {
        enviarBLE("SA:KEEP");
      }
    } else {
      enviarBLE("SA:LIMIT");
    }
  }
  else if (cmd == "DA") {
    // Assento desce - Dead man's switch
    if (!trava_bt_DA) {
      ultimoComandoDA = millis();
      if (!estado_da) {
        estado_da = true;
        digitalWrite(Rele_DA, HIGH);
        faz_bt_seg = 1;
        enviarBLE("DA:ON");
        iniciaTimerMotor();
      } else {
        enviarBLE("DA:KEEP");
      }
    } else {
      enviarBLE("DA:LIMIT");
    }
  }
  else if (cmd == "SP") {
    // Pernas sobem - Dead man's switch
    if (!trava_bt_SP) {
      ultimoComandoSP = millis();
      if (!estado_sp) {
        estado_sp = true;
        digitalWrite(Rele_SP, HIGH);
        faz_bt_seg = 1;
        enviarBLE("SP:ON");
        iniciaTimerMotor();
      } else {
        enviarBLE("SP:KEEP");
      }
    } else {
      enviarBLE("SP:LIMIT");
    }
  }
  else if (cmd == "DP") {
    // Pernas descem - Dead man's switch
    if (!trava_bt_DP) {
      ultimoComandoDP = millis();
      if (!estado_dp) {
        estado_dp = true;
        digitalWrite(Rele_DP, HIGH);
        faz_bt_seg = 1;
        enviarBLE("DP:ON");
        iniciaTimerMotor();
      } else {
        enviarBLE("DP:KEEP");
      }
    } else {
      enviarBLE("DP:LIMIT");
    }
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
  }
  else if (cmd == "HORIMETRO") {
    // Retorna horÃ­metro
    enviarBLE("HORIMETRO:" + String(horimetro, 2));
  }
  else if (cmd == "PING") {
    // Teste de conexÃ£o
    enviarBLE("PONG");
  }
  else {
    enviarBLE("ERRO:CMD_INVALIDO");
  }
}

// Envia status completo via BLE em formato JSON
void enviaStatusBluetooth() {
  StaticJsonDocument<512> doc;
  
  doc["serial"] = NUMERO_SERIE_CADEIRA;
  doc["habilitada"] = cadeiraHabilitada;
  doc["manutencao"] = manutencaoNecessaria;
  doc["horimetro"] = horimetro;
  doc["wifi"] = WiFi.isConnected();
  doc["wifi_ssid"] = WiFi.isConnected() ? WiFi.SSID() : "";
  
  // Estados dos relÃ©s
  doc["refletor"] = estado_r;
  doc["encosto_pos"] = incoder_virtual_encosto_service;
  doc["assento_pos"] = incoder_virtual_asento_service;
  doc["perneira_pos"] = incoder_virtual_perneira_service;
  
  // Limites
  doc["se_limit"] = trava_bt_SE;
  doc["de_limit"] = trava_bt_DE;
  doc["sa_limit"] = trava_bt_SA;
  doc["da_limit"] = trava_bt_DA;
  doc["sp_limit"] = trava_bt_SP;
  doc["dp_limit"] = trava_bt_DP;

  String output;
  serializeJson(doc, output);
  enviarBLE("STATUS:" + output);
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
                          (digitalRead(Rele_SP) == HIGH) || (digitalRead(Rele_DP) == HIGH);
  
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
  preferences.begin("horimetro", true);
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
  String url = String(SUPABASE_URL) + "/rest/v1/chairs?serial_number=eq." + NUMERO_SERIE_CADEIRA;
  
  if (http.begin(client, url)) {
    http.setTimeout(10000); // 10s timeout
    http.addHeader("Content-Type", "application/json");
    http.addHeader("apikey", SUPABASE_KEY);
    http.addHeader("Authorization", String("Bearer ") + SUPABASE_KEY);
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
  String url = String(SUPABASE_URL) + "/rest/v1/chairs";
  
  if (http.begin(client, url)) {
    http.setTimeout(10000); // 10s timeout
    http.addHeader("Content-Type", "application/json");
    http.addHeader("apikey", SUPABASE_KEY);
    http.addHeader("Authorization", String("Bearer ") + SUPABASE_KEY);
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
  String url = String(SUPABASE_URL) + "/rest/v1/chairs?serial_number=eq." + NUMERO_SERIE_CADEIRA + "&select=enabled,maintenance_required,maintenance_hours";
  
  if (http.begin(client, url)) {
    http.setTimeout(10000); // 10s timeout
    http.addHeader("apikey", SUPABASE_KEY);
    http.addHeader("Authorization", String("Bearer ") + SUPABASE_KEY);
    
    int httpCode = http.GET();
    Serial.print("HTTP Code: "); Serial.println(httpCode);
    
    if (httpCode == 200) {
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
    }
    http.end();
  } else {
    Serial.println("Falha ao iniciar conexao HTTP");
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
  // Carrega posiÃ§Ãµes do encoder virtual M1
  preferences.begin("encoder_encosto", true);
  incoder_virtual_encosto_M1 = preferences.getInt("encoder_encosto", 0);
  preferences.end();
  
  preferences.begin("encoder_asento", true);
  incoder_virtual_asento_M1 = preferences.getInt("encoder_asento", 0);
  preferences.end();
  
  preferences.begin("encoder_perneira", true);
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
  
  while (true) {
    Watch_Dog();
    
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
    
    // Movimento do encosto
    if (incoder_virtual_encosto_service > incoder_virtual_encosto_M1) {
      digitalWrite(Rele_DE, HIGH);
      delay(250);
      incoder_virtual_encosto_service--;
      Serial.print("Encosto posicao: ");
      Serial.println(incoder_virtual_encosto_service);
    } else {
      digitalWrite(Rele_DE, LOW);
      en_DE_acabou = true;
    }

    if (incoder_virtual_encosto_service < incoder_virtual_encosto_M1) {
      digitalWrite(Rele_SE, HIGH);
      delay(250);
      incoder_virtual_encosto_service++;
      Serial.print("Encosto posicao: ");
      Serial.println(incoder_virtual_encosto_service);
    } else {
      digitalWrite(Rele_SE, LOW);
      en_SE_acabou = true;
    }

    // Movimento do assento
    if (incoder_virtual_asento_service > incoder_virtual_asento_M1) {
      digitalWrite(Rele_DA, HIGH);
      delay(250);
      incoder_virtual_asento_service--;
      Serial.print("Assento posicao: ");
      Serial.println(incoder_virtual_asento_service);
    } else {
      digitalWrite(Rele_DA, LOW);
      en_DA_acabou = true;
    }

    if (incoder_virtual_asento_service < incoder_virtual_asento_M1) {
      digitalWrite(Rele_SA, HIGH);
      delay(250);
      incoder_virtual_asento_service++;
      Serial.print("Assento posicao: ");
      Serial.println(incoder_virtual_asento_service);
    } else {
      digitalWrite(Rele_SA, LOW);
      en_SA_acabou = true;
    }

    // Movimento da perneira
    if (incoder_virtual_perneira_service > incoder_virtual_perneira_M1) {
      digitalWrite(Rele_DP, HIGH);
      delay(250);
      incoder_virtual_perneira_service--;
      Serial.print("Perneira posicao: ");
      Serial.println(incoder_virtual_perneira_service);
    } else {
      digitalWrite(Rele_DP, LOW);
      en_DP_acabou = true;
    }

    if (incoder_virtual_perneira_service < incoder_virtual_perneira_M1) {
      digitalWrite(Rele_SP, HIGH);
      delay(250);
      incoder_virtual_perneira_service++;
      Serial.print("Perneira posicao: ");
      Serial.println(incoder_virtual_perneira_service);
    } else {
      digitalWrite(Rele_SP, LOW);
      en_SP_acabou = true;
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
      return;
    }
  }
}

// ========== FUNÃ‡Ã•ES DO ENCODER VIRTUAL ==========
void contagem_tempo_incoder_virtual() {
  // NÃ£o executa se encoder virtual estiver desabilitado (ex: durante VZ inicial)
  if (!habilitaEncoderVirtual) {
    return;
  }
  
  if (millis() - ultimoMillisEncoder < INTERVALO_ENCODER) {
    return;
  }
  ultimoMillisEncoder = millis();

  // Encosto - Sentar (SE)
  buttonState7 = digitalRead(Rele_SE);
  if (buttonState7 == HIGH) {
    if (incoder_virtual_encosto_service > 0) {
      incoder_virtual_encosto_service--;
      Serial.print("Encoder Encosto: ");
      Serial.println(incoder_virtual_encosto_service);
      trava_bt_DE = false;
    } else {
      digitalWrite(Rele_SE, LOW);
      trava_bt_SE = true;
      Serial.println("Limite SE atingido");
    }
  }

  // Encosto - Deitar (DE)
  buttonState8 = digitalRead(Rele_DE);
  if (buttonState8 == HIGH) {
    if (incoder_virtual_encosto_service < fim_encosto_encoder) {
      incoder_virtual_encosto_service++;
      Serial.print("Encoder Encosto: ");
      Serial.println(incoder_virtual_encosto_service);
      trava_bt_SE = false;
    } else {
      digitalWrite(Rele_DE, LOW);
      trava_bt_DE = true;
      Serial.println("Limite DE atingido");
    }
  }

  // Assento - Descer (DA)
  buttonState9 = digitalRead(Rele_DA);
  if (buttonState9 == HIGH) {
    if (incoder_virtual_asento_service > 0) {
      incoder_virtual_asento_service--;
      Serial.print("Encoder Assento: ");
      Serial.println(incoder_virtual_asento_service);
      trava_bt_DA = false;
      trava_bt_SA = false;
    } else {
      digitalWrite(Rele_DA, LOW);
      trava_bt_DA = true;
      Serial.println("Limite DA atingido");
    }
  }

  // Assento - Subir (SA)
  buttonState10 = digitalRead(Rele_SA);
  if (buttonState10 == HIGH) {
    if (incoder_virtual_asento_service < fim_asento_encoder) {
      incoder_virtual_asento_service++;
      Serial.print("Encoder Assento: ");
      Serial.println(incoder_virtual_asento_service);
      trava_bt_SA = false;
      trava_bt_DA = false;
    } else {
      digitalWrite(Rele_SA, LOW);
      trava_bt_SA = true;
      Serial.println("Limite SA atingido");
    }
  }
  
  // Perneira - Descer (DP)
  buttonState11 = digitalRead(Rele_DP);
  if (buttonState11 == HIGH) {
    if (incoder_virtual_perneira_service > 0) {
      incoder_virtual_perneira_service--;
      Serial.print("Encoder Perneira: ");
      Serial.println(incoder_virtual_perneira_service);
      trava_bt_DP = false;
      trava_bt_SP = false;
    } else {
      digitalWrite(Rele_DP, LOW);
      trava_bt_DP = true;
      Serial.println("Limite DP atingido");
    }
  }

  // Perneira - Subir (SP)
  buttonState12 = digitalRead(Rele_SP);
  if (buttonState12 == HIGH) {
    if (incoder_virtual_perneira_service < fim_perneira_encoder) {
      incoder_virtual_perneira_service++;
      Serial.print("Encoder Perneira: ");
      Serial.println(incoder_virtual_perneira_service);
      trava_bt_DP = false;
      trava_bt_SP = false;
    } else {
      digitalWrite(Rele_SP, LOW);
      trava_bt_SP = true;
      Serial.println("Limite SP atingido");
    }
  }
}

// ========== WATCH DOG (LED heartbeat) ==========
void Watch_Dog() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis > interval) {
    previousMillis = currentMillis;
    ledState = !ledState;
    digitalWrite(LED, ledState);
  }
}

unsigned long inicioAtivacaoRele = 0;
const unsigned long TIMEOUT_RELE = 30000; // 30 segundos

// ========== MONITORAMENTO DE TEMPO DOS RELÃ‰S (seguranÃ§a) ==========
void monitora_tempo_rele() {
  bool algumReleAtivo = (digitalRead(Rele_SE) == HIGH || 
                         digitalRead(Rele_SA) == HIGH || 
                         digitalRead(Rele_DA) == HIGH || 
                         digitalRead(Rele_SP) == HIGH || 
                         digitalRead(Rele_DP) == HIGH || 
                         digitalRead(Rele_DE) == HIGH);

  if (algumReleAtivo) {
    if (inicioAtivacaoRele == 0) {
      inicioAtivacaoRele = millis();
    } else if (millis() - inicioAtivacaoRele >= TIMEOUT_RELE) {
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
bool lastButtonState_RF = HIGH;
bool lastButtonState_SP = HIGH;
bool lastButtonState_DP = HIGH;
bool lastButtonState_DE = HIGH;
bool lastButtonState_SE = HIGH;
bool lastButtonState_SA = HIGH;
bool lastButtonState_DA = HIGH;
bool lastButtonState_VZ = HIGH;
bool lastButtonState_M1 = HIGH;
bool lastButtonState_PT = HIGH;

// ========== LEITURA DOS BOTÃ•ES FÃSICOS (NÃ£o bloqueante) ==========
void Button_geral() {
  if (faz_bt_seg != 0) return;

  // FunÃ§Ã£o auxiliar para ler botÃ£o com detecÃ§Ã£o de borda
  auto readButton = [](int pin, bool &lastState, const char* msg, bool &toggleVar, int relayPin, bool lock = false) {
    bool currentState = digitalRead(pin);
    if (currentState == LOW && lastState == HIGH) { // Borda de descida (pressionado)
      if (!lock) {
        toggleVar = !toggleVar;
        digitalWrite(relayPin, toggleVar);
        Serial.print("Botao "); Serial.print(msg); Serial.println(" acionado");
      } else {
        Serial.print("Botao "); Serial.print(msg); Serial.println(" bloqueado (limite)");
      }
    }
    lastState = currentState;
  };

  readButton(RF, lastButtonState_RF, "RF", estado_r, Rele_refletor);
  readButton(SP, lastButtonState_SP, "SP", estado_sp, Rele_SP, trava_bt_SP);
  readButton(DP, lastButtonState_DP, "DP", estado_dp, Rele_DP, trava_bt_DP);
  readButton(DE, lastButtonState_DE, "DE", estado_de, Rele_DE, trava_bt_DE);
  readButton(SE, lastButtonState_SE, "SE", estado_se, Rele_SE, trava_bt_SE);
  readButton(SA, lastButtonState_SA, "SA", estado_sa, Rele_SA, trava_bt_SA);
  readButton(DA, lastButtonState_DA, "DA", estado_da, Rele_DA, trava_bt_DA);

  // BotÃµes de funÃ§Ãµes especiais (VZ, M1, PT)
  bool currVZ = digitalRead(VZ);
  if (currVZ == LOW && lastButtonState_VZ == HIGH) {
    Serial.println("Botao VZ acionado");
    cont = 1;
    executa_vz();
  }
  lastButtonState_VZ = currVZ;

  bool currM1 = digitalRead(M1);
  if (currM1 == LOW && lastButtonState_M1 == HIGH) {
    Serial.println("Botao M1 acionado");
    executa_M1();
  }
  lastButtonState_M1 = currM1;

  bool currPT = digitalRead(PT);
  if (currPT == LOW && lastButtonState_PT == HIGH) {
    Serial.println("Botao PT acionado");
    cont13 = 1;
    executa_pt();
  }
  lastButtonState_PT = currPT;
}

// ========== EXECUÃ‡ÃƒO DA POSIÃ‡ÃƒO GINECOLÃ“GICA (VZ) ==========
void executa_vz() {
  Serial.println("VZ acionado");
  bip();

  digitalWrite(Rele_DA, HIGH);
  delay(250);
  digitalWrite(Rele_SE, HIGH);
  delay(250);
  digitalWrite(Rele_DP, HIGH);

  Serial.println("Executando VZ");
  faz_bt_seg = 1;

  while (cont == 1) {
    Watch_Dog();
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
        return;
      }
    }
    
    Button_geral();
    Button_Seg();
    contagem_tempo_incoder_virtual();
    contador2++;

    // Proteção contra overflow (15 segundos = 15000ms)
    if (contador2 >= 15000 || contador2 < 0) {
      digitalWrite(Rele_DA, LOW);
      digitalWrite(Rele_SE, LOW);
      digitalWrite(Rele_DP, LOW);
      cont = 0;
      contador2 = 0;

      bip();
      faz_bt_seg = 0;
      Serial.println("Fim VZ");
      enviarBLE("VZ:DONE");
      incoder_virtual_encosto_service = 0;
      incoder_virtual_asento_service = 0;
      incoder_virtual_perneira_service = 0;
    }
  }
}

// ========== EXECUÃ‡ÃƒO DA POSIÃ‡ÃƒO INICIAL VZ ==========
void executa_vz_ini() {
  Serial.println("====================================");
  Serial.println("  VZ INICIAL - " + NUMERO_SERIE_CADEIRA);
  Serial.println("====================================");
  Serial.println("Executando VZ inicial - Ativando reles sequencialmente");
  Serial.println("[INFO] Bloqueando loop() durante VZ inicial...");
  vzInicialEmAndamento = true; // Bloqueia loop() completamente
  
  Serial.println("[INFO] Desabilitando controle de encoder virtual temporariamente...");
  habilitaEncoderVirtual = false; // Desabilita encoder durante VZ inicial

  Serial.println("Ativando Rele_DA...");
  digitalWrite(Rele_DA, HIGH);
  delay(500);
  
  Serial.println("Ativando Rele_SE...");
  digitalWrite(Rele_SE, HIGH);
  delay(500);
  
  Serial.println("Ativando Rele_DP...");
  digitalWrite(Rele_DP, HIGH);
  delay(500);

  Serial.println("Aguardando 15 segundos...");
  Serial.println("[DEBUG] Heap livre: " + String(ESP.getFreeHeap()) + " bytes");
  Serial.println("[DEBUG] Iniciando loop de 15s com feed do watchdog...");
  Serial.flush();
  
  // Loop alimentando o watchdog a cada iteração (15 segundos)
  unsigned long startTime = millis();
  int lastSecond = 0;
  while (millis() - startTime < 15000) {
    esp_task_wdt_reset();  // Alimenta watchdog continuamente
    int currentSecond = (millis() - startTime) / 1000;
    if (currentSecond > lastSecond) {
      Serial.printf("[%d] ", currentSecond);
      Serial.flush();
      lastSecond = currentSecond;
    }
    delay(100);  // Delay pequeno para não travar CPU
  }
  
  Serial.println("\n[DEBUG] Loop completo - 15s finalizados!");
  Serial.flush();
  
  Serial.println("\n15s completos.");
  Serial.println("Desligando reles do VZ inicial...");
  digitalWrite(Rele_DA, LOW);
  digitalWrite(Rele_SE, LOW);
  digitalWrite(Rele_DP, LOW);

  Serial.println("Fim VZ inicial - Reles desligados");
  Serial.println("Resetando encoders virtuais...");
  incoder_virtual_encosto_service = 0;
  incoder_virtual_asento_service = 0;
  incoder_virtual_perneira_service = 0;
  Serial.println("Encoders resetados.");
  
  Serial.println("[INFO] Reabilitando controle de encoder virtual.");
  habilitaEncoderVirtual = true; // Reabilita encoder apÃ³s VZ inicial
  
  Serial.println("[INFO] Desbloqueando loop() - VZ inicial completo.");
  vzInicialEmAndamento = false; // Libera loop() para executar normalmente
}

// ========== EXECUÃ‡ÃƒO DA POSIÃ‡ÃƒO DE PARTO (PT) ==========
void executa_pt() {
  Serial.println("PT acionado");
  bip();

  digitalWrite(Rele_SA, HIGH);
  delay(250);
  digitalWrite(Rele_DE, HIGH);
  delay(250);
  digitalWrite(Rele_SP, HIGH);

  Serial.println("Executando PT");
  faz_bt_seg = 1;

  while (cont13 == 1) {
    Watch_Dog();
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
        return;
      }
    }
    
    Button_geral();
    Button_Seg();
    contagem_tempo_incoder_virtual();
    contador++;

    // Proteção contra overflow (15 segundos = 15000ms)
    if (contador >= 15000 || contador < 0) {
      digitalWrite(Rele_SA, LOW);
      digitalWrite(Rele_DE, LOW);
      digitalWrite(Rele_SP, LOW);
      cont13 = 0;
      contador = 0;

      bip();
      faz_bt_seg = 0;
      Serial.println("Fim PT");
      enviarBLE("PT:DONE");
    }
  }
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
    
    preferences.begin("encoder_perneira", false);
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

    bip();
    delay(200);
    bip();
  }

  // Se segurou o botÃ£o, executa o movimento para a posiÃ§Ã£o salva
  while (faz_m1 == true) {
    Watch_Dog();

    faz_bt_seg = 1;
    Button_Seg();

    if (faz_bt_seg == 0) {
      bip();
      delay(700);
      bip();
      faz_m1 = false;
      return;
    }

    // Movimento do Encosto
    if (incoder_virtual_encosto_service > incoder_virtual_encosto_M1) {
      digitalWrite(Rele_DE, HIGH);
      delay(250);
      incoder_virtual_encosto_service--;
      Serial.print("Posicao encoder virtual encosto = ");
      Serial.println(incoder_virtual_encosto_service);
    } else {
      digitalWrite(Rele_DE, LOW);
      en_DE_acabou = true;
    }

    if (incoder_virtual_encosto_service < incoder_virtual_encosto_M1) {
      digitalWrite(Rele_SE, HIGH);
      delay(250);
      incoder_virtual_encosto_service++;
      Serial.print("Posicao encoder virtual encosto = ");
      Serial.println(incoder_virtual_encosto_service);
    } else {
      digitalWrite(Rele_SE, LOW);
      en_SE_acabou = true;
    }

    // Movimento do Assento
    if (incoder_virtual_asento_service > incoder_virtual_asento_M1) {
      digitalWrite(Rele_DA, HIGH);
      delay(250);
      incoder_virtual_asento_service--;
      Serial.print("Posicao encoder virtual assento = ");
      Serial.println(incoder_virtual_asento_service);
    } else {
      digitalWrite(Rele_DA, LOW);
      en_DA_acabou = true;
    }

    if (incoder_virtual_asento_service < incoder_virtual_asento_M1) {
      digitalWrite(Rele_SA, HIGH);
      delay(250);
      incoder_virtual_asento_service++;
      Serial.print("Posicao encoder virtual assento = ");
      Serial.println(incoder_virtual_asento_service);
    } else {
      digitalWrite(Rele_SA, LOW);
      en_SA_acabou = true;
    }

    // Movimento da Perneira
    if (incoder_virtual_perneira_service > incoder_virtual_perneira_M1) {
      digitalWrite(Rele_DP, HIGH);
      delay(250);
      incoder_virtual_perneira_service--;
      Serial.print("Posicao encoder virtual perneira = ");
      Serial.println(incoder_virtual_perneira_service);
    } else {
      digitalWrite(Rele_DP, LOW);
      en_DP_acabou = true;
    }

    if (incoder_virtual_perneira_service < incoder_virtual_perneira_M1) {
      digitalWrite(Rele_SP, HIGH);
      delay(250);
      incoder_virtual_perneira_service++;
      Serial.print("Posicao encoder virtual perneira = ");
      Serial.println(incoder_virtual_perneira_service);
    } else {
      digitalWrite(Rele_SP, LOW);
      en_SP_acabou = true;
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
  }
}

// ========== PARADA DE EMERGÃŠNCIA (AT_SEG) ==========
void AT_SEG() {
  Serial.println("Entrou rotina AT_SEG");

  if (faz_bt_seg == 1) {
    Serial.println("Parando todos os movimentos");
    enviarBLE("AT_SEG:STOPPING");

    digitalWrite(Rele_DE, LOW);
    digitalWrite(Rele_SE, LOW);
    digitalWrite(Rele_SA, LOW);
    digitalWrite(Rele_DA, LOW);
    digitalWrite(Rele_SP, LOW);
    digitalWrite(Rele_DP, LOW);

    // Atualiza estados
    estado_de = false;
    estado_se = false;
    estado_sa = false;
    estado_da = false;
    estado_sp = false;
    estado_dp = false;

    faz_bt_seg = 0;
    cont = 0;
    cont13 = 0;
  }
  delay(100);
}

// ========== BIP (buzzer) ==========
void bip() {
  digitalWrite(BUZZER, HIGH);
  delay(100);
  digitalWrite(BUZZER, LOW);
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
  Serial.print("BLE Status: "); 
  Serial.println(bleClienteConectado ? "CONECTADO" : "AGUARDANDO CONEXAO");
  if (bleClienteConectado && pServer != NULL) {
    Serial.print("BLE Clientes: "); 
    Serial.println(pServer->getConnectedCount());
  }
  Serial.print("Horimetro: "); Serial.println(horimetro);
  Serial.print("Cadeira Habilitada: "); Serial.println(cadeiraHabilitada ? "SIM" : "NAO");
  Serial.println("--------------------------------");
}

