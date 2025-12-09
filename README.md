# Cadeira Odontológica/Ginecológica - Controle via Bluetooth

Este projeto implementa um sistema completo de controle via Bluetooth para cadeira odontológica/ginecológica usando ESP32 e aplicativo Flutter multiplataforma.

## 🚀 Características

### Aplicativo Flutter
- **Conexão Bluetooth**: Interface intuitiva para conectar com o ESP32
- **Controles Individuais**: Todos os movimentos da cadeira podem ser controlados separadamente
- **Posições Pré-definidas**: 
  - Posição Ginecológica (VZ)
  - Posição de Parto (PT)
- **Botão de Emergência**: Interrompe todos os movimentos instantaneamente
- **Multiplataforma**: Android e iOS

### ESP32
- **Bluetooth Serial**: Comunicação bidirecional com o app
- **Controles Físicos**: Botões locais continuam funcionando
- **Sistema de Segurança**: Timeout automático e botão de emergência
- **Feedback Visual/Sonoro**: LED indicador e buzzer

## 📱 Controles Disponíveis

| Comando | Função | Descrição |
|---------|--------|-----------|
| `RF` | Refletor | Liga/Desliga o refletor |
| `SE` | Sentar | Move o encosto para posição sentada |
| `DE` | Deitar | Move o encosto para posição deitada |
| `SA` | Subir Assento | Eleva o assento |
| `DA` | Descer Assento | Abaixa o assento |
| `SP` | Subir Pernas | Eleva as pernas |
| `DP` | Descer Pernas | Abaixa as pernas |
| `VZ` | Posição Ginecológica | Sequência automática |
| `PT` | Posição de Parto | Sequência automática |
| `AT_SEG` | Emergência | Para todos os movimentos |

## 🔧 Configuração do Hardware

### Pinagem ESP32

#### Entradas (Botões)
- **DE** (Deitar): GPIO 5
- **SA** (Subir Assento): GPIO 34
- **DA** (Descer Assento): GPIO 17
- **SE** (Sentar): GPIO 13
- **VZ** (Ginecológica): GPIO 22
- **SP** (Subir Pernas): GPIO 18
- **DP** (Descer Pernas): GPIO 19
- **PT** (Parto): GPIO 21
- **RF** (Refletor): GPIO 23

#### Saídas (Relés)
- **Rele_SA**: GPIO 33
- **Rele_DA**: GPIO 25
- **Rele_SE**: GPIO 27
- **Rele_DE**: GPIO 26
- **Rele_SP**: GPIO 12
- **Rele_DP**: GPIO 14
- **LED**: GPIO 2
- **Rele_refletor**: GPIO 4
- **BUZZER**: GPIO 32

## 🛠️ Instalação e Configuração

### 1. ESP32

1. **Instale a biblioteca BluetoothSerial**:
   - No Arduino IDE: Sketch > Incluir Biblioteca > Gerenciar Bibliotecas
   - Procure por "ESP32" e instale

2. **Configure o ESP32**:
   ```cpp
   // O código já está configurado em esp32_bluetooth_chair.ino
   // Nome Bluetooth: "CadeiraOdontologica"
   ```

3. **Upload do código**:
   - Conecte o ESP32 via USB
   - Selecione a placa correta no Arduino IDE
   - Faça o upload do arquivo `esp32_bluetooth_chair.ino`

### 2. Aplicativo Flutter

1. **Instale as dependências**:
   ```bash
   flutter pub get
   ```

2. **Para Android**:
   - As permissões já estão configuradas no `AndroidManifest.xml`
   - Compile: `flutter build apk`

3. **Para iOS**:
   - As permissões estão configuradas no `Info.plist`
   - Compile: `flutter build ios`

## 📚 Como Usar

### Primeira Conexão

1. **Configure o ESP32**:
   - Carregue o código no ESP32
   - O dispositivo aparecerá como "CadeiraOdontologica" no Bluetooth

2. **Pareamento**:
   - Nas configurações do dispositivo móvel
   - Bluetooth > Procurar dispositivos
   - Conecte com "CadeiraOdontologica"

3. **Use o App**:
   - Abra o aplicativo
   - Toque em "Buscar Dispositivos"
   - Selecione "CadeiraOdontologica"
   - Toque em "Conectar"

### Operação

- **Controles Individuais**: Use os botões para cada função específica
- **Posições Automáticas**: Use os botões "Posição Ginecológica" ou "Posição de Parto"
- **Emergência**: Toque no botão vermelho "PARAR TUDO" para interromper todos os movimentos

## 🔒 Segurança

- **Timeout Automático**: Movimentos param automaticamente após 15 segundos
- **Botão de Emergência**: Disponível tanto no app quanto fisicamente
- **Detecção de Botões**: Qualquer botão físico pressionado interrompe sequências automáticas
- **Feedback**: Buzzer confirma início e fim de operações

## 🚨 Troubleshooting

### Problemas de Conexão
1. Verifique se o ESP32 está ligado (LED piscando)
2. Certifique-se de que o dispositivo está pareado
3. Reinicie o Bluetooth do dispositivo móvel
4. Reinicie o ESP32

### App não Conecta
1. Verifique permissões de Bluetooth
2. Certifique-se de que o ESP32 não está conectado a outro dispositivo
3. Tente desparear e parear novamente

### Comandos não Funcionam
1. Verifique a conexão Bluetooth
2. Observe o LED do ESP32 (deve estar piscando)
3. Teste botões físicos primeiro
4. Use o comando "STATUS" para verificar estados

## 📋 Dependências

### Flutter
- `flutter_bluetooth_serial: ^0.4.0`
- `provider: ^6.1.1`
- `permission_handler: ^11.0.1`
- `shared_preferences: ^2.2.2`

### ESP32
- Biblioteca BluetoothSerial (nativa do ESP32)

## 🏗️ Estrutura do Projeto

```
lib/
├── main.dart                    # Ponto de entrada do app
├── models/
│   └── chair_state.dart        # Modelo de dados da cadeira
├── services/
│   └── bluetooth_service.dart  # Serviço de comunicação Bluetooth
├── screens/
│   ├── bluetooth_connection_screen.dart  # Tela de conexão
│   └── chair_control_screen.dart        # Tela de controle
└── widgets/
    ├── device_list_widget.dart         # Lista de dispositivos
    ├── control_button.dart            # Botão customizado
    └── position_control.dart          # Widget de controle de posição

esp32_bluetooth_chair.ino        # Código do ESP32
```

## 📄 Licença

Este projeto é fornecido como está, para uso em cadeiras odontológicas/ginecológicas. Certifique-se de testar completamente antes do uso clínico.

## ⚠️ Aviso Importante

Este sistema deve ser testado completamente em ambiente controlado antes do uso clínico. Sempre mantenha o botão de emergência acessível e teste todos os sistemas de segurança.
