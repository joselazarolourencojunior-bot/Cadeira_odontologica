# Cadeira Odontológica/Ginecológica - App Flutter

Aplicativo Flutter multiplataforma para controle de cadeira odontológica/ginecológica via Bluetooth Low Energy (BLE).

## 🚀 Características

### Aplicativo Flutter

- **Conexão Bluetooth**: Interface intuitiva para conectar com dispositivos BLE
- **Controles Individuais**: Todos os movimentos da cadeira podem ser controlados separadamente
- **Posições Pré-definidas**:
  - Posição Ginecológica (VZ)
  - Posição de Parto (PT)
- **Botão de Emergência**: Interrompe todos os movimentos instantaneamente
- **Multiplataforma**: Android e iOS
- **Integração Supabase**: Bloqueio remoto e rastreamento de manutenção

## 📱 Controles Disponíveis

| Comando | Função | Descrição |
| ------- | ------ | --------- |
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

## 🛠️ Instalação e Configuração

### Aplicativo Flutter

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

1. **Pareamento BLE**:
   - Nas configurações do dispositivo móvel
   - Bluetooth > Procurar dispositivos
   - Conecte com o dispositivo BLE da cadeira

2. **Use o App**:
   - Abra o aplicativo
   - Toque em "Buscar Dispositivos"
   - Selecione o dispositivo da cadeira
   - Toque em "Conectar"

### Operação

- **Controles Individuais**: Use os botões para cada função específica
- **Posições Automáticas**: Use os botões "Posição Ginecológica" ou "Posição de Parto"
- **Emergência**: Toque no botão vermelho "PARAR TUDO" para interromper todos os movimentos

## 🔒 Segurança

- **Timeout Automático**: Movimentos param automaticamente após timeout
- **Botão de Emergência**: Disponível no app
- **Feedback**: Vibração confirma operações

## 🚨 Troubleshooting

### Problemas de Conexão

1. Verifique se o dispositivo BLE está ligado
2. Certifique-se de que o dispositivo está pareado
3. Reinicie o Bluetooth do dispositivo móvel

### App Não Conecta

1. Verifique permissões de Bluetooth
2. Certifique-se de que o dispositivo não está conectado a outro app
3. Tente desparear e parear novamente

### Comandos Não Funcionam

1. Verifique a conexão Bluetooth
2. Teste a conectividade BLE
3. Use o comando "STATUS" para verificar estados

## 📋 Dependências

### Flutter

- `flutter_blue_plus: ^1.36.8`
- `provider: ^6.1.1`
- `permission_handler: ^11.4.0`
- `shared_preferences: ^2.5.3`
- `supabase_flutter: ^2.0.0`

## 🏗️ Estrutura do Projeto

```text
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
