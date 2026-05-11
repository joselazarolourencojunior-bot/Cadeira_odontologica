#include "BluetoothSerial.h"

// Verificar se Bluetooth está disponível
#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth não está habilitado! Execute "make menuconfig" para habilitá-lo
#endif

#if !defined(CONFIG_BT_SPP_ENABLED)
#error Bluetooth Serial não disponível ou não habilitado! Execute "make menuconfig" para habilitá-lo
#endif

BluetoothSerial SerialBT;

// Constants won't change. They're used here to set pin numbers INPUT'S:
const int DE = 5;
const int SA = 34;
const int DA = 17;
const int SE = 13;
const int VZ = 22;
const int SP = 18;
const int DP = 19;
const int PT = 21;
const int RF = 23;

// Constants won't change. They're used here to set pin numbers OUTPUT'S:
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

// Variables will change:
int buttonState = 0;
int buttonState1 = 0;
int buttonState2 = 0;
int buttonState3 = 0;
int buttonState4 = 0;
int buttonState5 = 0;
unsigned long previousMillis = 0;
unsigned long currentMillis;
unsigned long interval = 500;

int ledState = 0;
int cont = 0;
int contador = 0;
bool estado_r = 0;
bool estado_sp = 0;
bool estado_dp = 0;
bool estado_sa = 0;
bool estado_da = 0;
bool estado_se = 0;
bool estado_de = 0;
bool faz_bt_seg = 0;

unsigned long currentMillis20 = 0;
unsigned long previousMillis_amostra20 = 0;
int TEMPO_prints = 15000;

String receivedCommand = "";
bool commandReady = false;

void setup() {
  // Inicializar comunicação serial
  Serial.begin(115200);
  
  // Inicializar Bluetooth Serial
  SerialBT.begin("CadeiraOdontologica"); // Nome do dispositivo Bluetooth
  Serial.println("Dispositivo Bluetooth iniciado. Procure por 'CadeiraOdontologica' para parear.");

  // Inicializar pinos de entrada com pull-up
  pinMode(DE, INPUT_PULLUP);
  pinMode(SA, INPUT_PULLUP);
  pinMode(DA, INPUT_PULLUP);
  pinMode(SE, INPUT_PULLUP);
  pinMode(RF, INPUT_PULLUP);
  pinMode(VZ, INPUT_PULLUP);
  pinMode(PT, INPUT_PULLUP);
  pinMode(SP, INPUT_PULLUP);
  pinMode(DP, INPUT_PULLUP);
  pinMode(AD_PIC, INPUT);

  // Inicializar pinos de saída
  pinMode(Rele_SA, OUTPUT);
  pinMode(Rele_DA, OUTPUT);
  pinMode(Rele_SE, OUTPUT);
  pinMode(Rele_DE, OUTPUT);
  pinMode(Rele_DP, OUTPUT);
  pinMode(Rele_SP, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(LED, OUTPUT);
  pinMode(Rele_refletor, OUTPUT);

  // Desligar todos os relés no início
  digitalWrite(Rele_SA, LOW);
  digitalWrite(Rele_DA, LOW);
  digitalWrite(Rele_SE, LOW);
  digitalWrite(Rele_DE, LOW);
  digitalWrite(Rele_DP, LOW);
  digitalWrite(Rele_SP, LOW);
  digitalWrite(Rele_refletor, LOW);

  Serial.println("Inicio programa cadeira GO");
  SerialBT.println("Cadeira Odontológica conectada via Bluetooth");

  bip();
  delay(100);
  bip();
}

void loop() {
  // Gerenciar LED indicador
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis > interval) {
    previousMillis = currentMillis;
    ledState = !ledState;
    digitalWrite(LED, ledState);
  }

  // Verificar comandos Bluetooth
  checkBluetoothCommands();

  // Verificar botões físicos
  Button_Seg();
  Button_geral();
  monitora_tempo_rele();
}

void checkBluetoothCommands() {
  while (SerialBT.available()) {
    char receivedChar = SerialBT.read();
    
    if (receivedChar == '\n') {
      commandReady = true;
    } else {
      receivedCommand += receivedChar;
    }
  }

  if (commandReady) {
    processBluetoothCommand(receivedCommand);
    receivedCommand = "";
    commandReady = false;
  }
}

void processBluetoothCommand(String command) {
  command.trim(); // Remove espaços em branco
  
  Serial.println("Comando recebido via Bluetooth: " + command);
  SerialBT.println("Processando comando: " + command);

  if (command == "RF") {
    toggleReflector();
  }
  else if (command == "SP") {
    toggleUpperLegs();
  }
  else if (command == "DP") {
    toggleLowerLegs();
  }
  else if (command == "SA") {
    activateSeatUp();
  }
  else if (command == "DA") {
    activateSeatDown();
  }
  else if (command == "SE") {
    activateBackUp();
  }
  else if (command == "DE") {
    activateBackDown();
  }
  else if (command == "VZ") {
    executeGynecologicalPosition();
  }
  else if (command == "PT") {
    executeBirthPosition();
  }
  else if (command == "AT_SEG") {
    AT_SEG();
    SerialBT.println("EMERGÊNCIA: Todos os movimentos foram interrompidos");
  }
  else if (command == "STATUS") {
    sendStatus();
  }
  else {
    Serial.println("Comando não reconhecido: " + command);
    SerialBT.println("ERRO: Comando não reconhecido");
  }
}

void toggleReflector() {
  estado_r = !estado_r;
  digitalWrite(Rele_refletor, estado_r);
  String status = estado_r ? "LIGADO" : "DESLIGADO";
  Serial.println("Refletor " + status);
  SerialBT.println("Refletor " + status);
}

void toggleUpperLegs() {
  estado_sp = !estado_sp;
  digitalWrite(Rele_SP, estado_sp);
  String status = estado_sp ? "ATIVADO" : "DESATIVADO";
  Serial.println("Subir Pernas " + status);
  SerialBT.println("Subir Pernas " + status);
  if (estado_sp) faz_bt_seg = 1;
}

void toggleLowerLegs() {
  estado_dp = !estado_dp;
  digitalWrite(Rele_DP, estado_dp);
  String status = estado_dp ? "ATIVADO" : "DESATIVADO";
  Serial.println("Descer Pernas " + status);
  SerialBT.println("Descer Pernas " + status);
  if (estado_dp) faz_bt_seg = 1;
}

void activateSeatUp() {
  estado_sa = !estado_sa;
  digitalWrite(Rele_SA, estado_sa);
  Serial.println("Subir Assento acionado");
  SerialBT.println("Subir Assento acionado");
  if (estado_sa) faz_bt_seg = 1;
}

void activateSeatDown() {
  estado_da = !estado_da;
  digitalWrite(Rele_DA, estado_da);
  Serial.println("Descer Assento acionado");
  SerialBT.println("Descer Assento acionado");
  if (estado_da) faz_bt_seg = 1;
}

void activateBackUp() {
  estado_se = !estado_se;
  digitalWrite(Rele_SE, estado_se);
  Serial.println("Sentar (Encosto) acionado");
  SerialBT.println("Sentar (Encosto) acionado");
  if (estado_se) faz_bt_seg = 1;
}

void activateBackDown() {
  estado_de = !estado_de;
  digitalWrite(Rele_DE, estado_de);
  Serial.println("Deitar (Encosto) acionado");
  SerialBT.println("Deitar (Encosto) acionado");
  if (estado_de) faz_bt_seg = 1;
}

void executeGynecologicalPosition() {
  Serial.println("Executando posição ginecológica");
  SerialBT.println("Executando posição ginecológica");
  
  bip();
  
  digitalWrite(Rele_DA, HIGH);
  delay(250);
  digitalWrite(Rele_SE, HIGH);
  delay(250);
  digitalWrite(Rele_DP, HIGH);

  cont = 1;
  contador = 0;
  
  while (cont == 1) {
    delay(1);
    checkBluetoothCommands(); // Permitir interrupção via Bluetooth
    Button_geral();
    Button_Seg();
    contador++;

    if (contador >= 15000) {
      digitalWrite(Rele_DA, LOW);
      digitalWrite(Rele_SE, LOW);
      digitalWrite(Rele_DP, LOW);
      cont = 0;
      contador = 0;
      bip();
      faz_bt_seg = 0;
      Serial.println("Posição ginecológica concluída");
      SerialBT.println("Posição ginecológica concluída");
    }
  }
}

void executeBirthPosition() {
  Serial.println("Executando posição de parto");
  SerialBT.println("Executando posição de parto");
  
  bip();
  
  digitalWrite(Rele_SA, HIGH);
  delay(250);
  digitalWrite(Rele_DE, HIGH);
  delay(250);
  digitalWrite(Rele_SP, HIGH);

  cont = 1;
  contador = 0;
  
  while (cont == 1) {
    delay(1);
    checkBluetoothCommands(); // Permitir interrupção via Bluetooth
    Button_geral();
    Button_Seg();
    contador++;

    if (contador >= 15000) {
      digitalWrite(Rele_SA, LOW);
      digitalWrite(Rele_DE, LOW);
      digitalWrite(Rele_SP, LOW);
      cont = 0;
      contador = 0;
      bip();
      faz_bt_seg = 0;
      Serial.println("Posição de parto concluída");
      SerialBT.println("Posição de parto concluída");
    }
  }
}

void sendStatus() {
  SerialBT.println("=== STATUS DA CADEIRA ===");
  SerialBT.println("Refletor: " + String(estado_r ? "LIGADO" : "DESLIGADO"));
  SerialBT.println("Subir Pernas: " + String(estado_sp ? "ATIVO" : "INATIVO"));
  SerialBT.println("Descer Pernas: " + String(estado_dp ? "ATIVO" : "INATIVO"));
  SerialBT.println("Subir Assento: " + String(estado_sa ? "ATIVO" : "INATIVO"));
  SerialBT.println("Descer Assento: " + String(estado_da ? "ATIVO" : "INATIVO"));
  SerialBT.println("Sentar: " + String(estado_se ? "ATIVO" : "INATIVO"));
  SerialBT.println("Deitar: " + String(estado_de ? "ATIVO" : "INATIVO"));
  SerialBT.println("========================");
}

// Resto das funções originais mantidas...

void monitora_tempo_rele() {
  buttonState = digitalRead(DE);
  buttonState1 = digitalRead(SE);
  buttonState2 = digitalRead(SA);
  buttonState3 = digitalRead(DA);
  buttonState4 = digitalRead(DP);
  buttonState5 = digitalRead(DP);

  if (buttonState == HIGH || buttonState1 == HIGH || buttonState2 == HIGH || 
      buttonState3 == HIGH || buttonState4 == HIGH || buttonState5 == HIGH) {
    delay(1);
    contador++;

    if (contador >= 30000) {
      AT_SEG();
      contador = 0;
    }
  }
}

void Button_Seg() {
  if (digitalRead(DE) == LOW || digitalRead(SE) == LOW || digitalRead(DA) == LOW || 
      digitalRead(SA) == LOW || digitalRead(VZ) == LOW || digitalRead(RF) == LOW || 
      digitalRead(SP) == LOW || digitalRead(DP) == LOW || digitalRead(PT) == LOW) {
    AT_SEG();
  }
}

void Button_geral() {
  if (faz_bt_seg == 0) {
    // Botão Refletor
    buttonState = digitalRead(RF);
    if (buttonState == LOW) {
      toggleReflector();
      while (digitalRead(RF) == LOW) {
        delay(10);
      }
    }

    // Botão Subir Pernas
    buttonState = digitalRead(SP);
    if (buttonState == LOW) {
      toggleUpperLegs();
      while (digitalRead(SP) == LOW) {
        delay(10);
      }
    }

    // Botão Descer Pernas
    buttonState = digitalRead(DP);
    if (buttonState == LOW) {
      toggleLowerLegs();
      while (digitalRead(DP) == LOW) {
        delay(10);
      }
    }

    // Botão Deitar
    buttonState = digitalRead(DE);
    if (buttonState == LOW) {
      activateBackDown();
      while (digitalRead(DE) == LOW) {
        delay(10);
      }
    }

    // Botão Sentar
    buttonState = digitalRead(SE);
    if (buttonState == LOW) {
      activateBackUp();
      while (digitalRead(SE) == LOW) {
        delay(10);
      }
    }

    // Botão Subir Assento
    buttonState = digitalRead(SA);
    if (buttonState == LOW) {
      activateSeatUp();
      while (digitalRead(SA) == LOW) {
        delay(10);
      }
    }

    // Botão Descer Assento
    buttonState = digitalRead(DA);
    if (buttonState == LOW) {
      activateSeatDown();
      while (digitalRead(DA) == LOW) {
        delay(10);
      }
    }

    // Botão Posição Ginecológica
    buttonState = digitalRead(VZ);
    if (buttonState == LOW) {
      executeGynecologicalPosition();
      while (digitalRead(VZ) == LOW) {
        delay(10);
      }
    }

    // Botão Posição de Parto
    buttonState = digitalRead(PT);
    if (buttonState == LOW) {
      executeBirthPosition();
      while (digitalRead(PT) == LOW) {
        delay(10);
      }
    }

    delay(100);
  }
}

void AT_SEG() {
  Serial.println("Rotina de segurança ativada - parando todos os movimentos");
  SerialBT.println("SEGURANÇA: Parando todos os movimentos");

  if (faz_bt_seg == 1) {
    digitalWrite(Rele_DE, LOW);
    digitalWrite(Rele_SE, LOW);
    digitalWrite(Rele_SA, LOW);
    digitalWrite(Rele_DA, LOW);
    digitalWrite(Rele_SP, LOW);
    digitalWrite(Rele_DP, LOW);

    // Atualizar estados
    estado_de = false;
    estado_se = false;
    estado_sa = false;
    estado_da = false;
    estado_sp = false;
    estado_dp = false;

    faz_bt_seg = 0;
    cont = 0;
  }
  delay(100);
}

void bip() {
  digitalWrite(BUZZER, HIGH);
  delay(100);
  digitalWrite(BUZZER, LOW);
}