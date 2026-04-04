# Cadeira Odontológica - Instruções para Agentes de IA

## Visão Geral
App Flutter + firmware ESP32 para controle de cadeira odontológica/ginecológica via Bluetooth Serial. O app envia comandos de texto de 2 letras para o ESP32 que controla relés conectados aos motores da cadeira.

## Arquitetura

### Fluxo de Dados
```
Flutter UI → BluetoothService → Bluetooth Serial → ESP32 → Relés → Motores da Cadeira
         ↓
  SupabaseService (bloqueio remoto/rastreamento de manutenção - opcional)
```

### Padrão de Gerenciamento de Estado
Todos os serviços estendem `ChangeNotifier` e são providos via `MultiProvider` em [main.dart](lib/main.dart):
- Use `context.read<Service>()` para acesso único (ações)
- Use `context.watch<Service>()` para rebuilds reativos

### Serviços Principais (`lib/services/`)
| Serviço | Propósito |
|---------|-----------|
| `BluetoothService` | Gerenciamento de conexão, envio de comandos via `flutter_bluetooth_serial` |
| `SupabaseService` | Bloqueio remoto da cadeira, rastreamento de manutenção (singleton, funciona offline) |
| `SettingsService` | Persistência local via `shared_preferences` (horímetro, senhas) |

### Modelos (`lib/models/`)
- **`ChairState`**: Estado em tempo real dos componentes (refletor, pernas, assento, encosto)
- **`ChairCommand`**: Constantes estáticas para todos os comandos - **SEMPRE use estas, nunca hardcode strings**
- **`ChairSettings`**: Config persistente (horímetro, intervalos de manutenção, senha admin)

## Protocolo de Comandos
Comandos são códigos de 2 letras enviados via Bluetooth com terminador newline:

| Comando | Ação | Método Handler |
|---------|------|----------------|
| `RF` | Liga/desliga refletor | `toggleReflector()` |
| `SE` | Encosto sobe (sentar) | `moveBackUp()` |
| `DE` | Encosto desce (deitar) | `moveBackDown()` |
| `SA` | Assento sobe | `moveSeatUp()` |
| `DA` | Assento desce | `moveSeatDown()` |
| `SP` | Pernas sobem | `toggleUpperLegs()` |
| `DP` | Pernas descem | `toggleLowerLegs()` |
| `VZ` | Posição ginecológica | `setGynecologicalPosition()` |
| `PT` | Posição de parto | `setBirthPosition()` |
| `AT_SEG` | Parada de emergência | `stopAll()` |

## Padrões Críticos

### Adicionando Novos Comandos
1. Adicionar constante na classe `ChairCommand` ([chair_state.dart](lib/models/chair_state.dart))
2. Adicionar método handler no `BluetoothService` com atualização de estado + `notifyListeners()`
3. Adicionar processamento no firmware ESP32 na função `processBluetoothCommand()` ([esp32](lib/esp32))
4. Adicionar pino de relé se precisar de novo controle de hardware

### Padrão de Widget
Use o widget `ControlButton` ([control_button.dart](lib/widgets/control_button.dart)) para controles da cadeira - fornece estilo consistente com indicação de estado ativo.

### Convenção de Pinos ESP32
- Entradas usam `INPUT_PULLUP` - botões conectam ao GND
- Saídas controlam relés - `HIGH` = ativo
- Veja [README.md](README.md) para tabela de pinagem completa

## Comandos de Desenvolvimento
```bash
flutter pub get              # Instalar dependências
flutter run                  # Rodar no dispositivo conectado (requer Bluetooth)
flutter build apk            # Compilar APK Android
flutter build apk --release  # Build de produção
```

## Testes sem Hardware
Chame `BluetoothService.connectDemo()` para simular conexão e testar a UI sem ESP32.

## Integração Supabase
Configure as credenciais em [supabase_service.dart](lib/services/supabase_service.dart):
```dart
static const String _supabaseUrl = 'SUA_URL';
static const String _supabaseAnonKey = 'SUA_CHAVE';
```
O app funciona totalmente offline quando não configurado - recursos de bloqueio remoto ficam desabilitados.

## Estrutura de Arquivos
```
lib/
├── main.dart           # Entrada do app, setup do MultiProvider
├── esp32               # Firmware Arduino do ESP32 (referência)
├── models/             # Classes de dados (ChairState, ChairCommand, ChairSettings)
├── screens/            # Telas completas (controle, bluetooth, configurações)
├── services/           # Lógica de negócio (Bluetooth, Supabase, Settings)
└── widgets/            # Componentes de UI reutilizáveis
```
