# Cadeira Odontológica - Instruções para Agentes de IA

## Visão Geral
App Flutter + firmware ESP32 para controle de cadeira odontológica/ginecológica via **Bluetooth Low Energy (BLE)**. O app envia comandos de texto de 2 letras para o ESP32 que controla relés conectados aos motores da cadeira.

## Arquitetura

### Fluxo de Dados
```
Flutter UI → BluetoothService (BLE) → ESP32 (BLE) → Relés → Motores da Cadeira
         ↓
  SupabaseService (bloqueio remoto/rastreamento de manutenção - opcional)
```

### Reconexão Automática
O app **reconecta automaticamente** ao ESP32 quando:
- O dispositivo foi previamente conectado
- O ESP32 entra no alcance do Bluetooth
- A conexão cai inesperadamente

A reconexão acontece a cada 5 segundos em background até o dispositivo ser encontrado.

### Padrão de Gerenciamento de Estado
Todos os serviços estendem `ChangeNotifier` e são providos via `MultiProvider` em `lib/main.dart`:
- Use `context.read<Service>()` para acesso único a métodos (ações)
- Use `context.watch<Service>()` para rebuilds reativos quando o estado muda
- Exemplo: `context.read<BluetoothService>().sendCommand()` vs `context.watch<BluetoothService>().isConnected`

### Serviços Principais (`lib/services/`)
| Serviço | Propósito | Inicialização |
|---------|-----------|---------------|
| `BluetoothService` | Gerenciamento de conexão BLE, reconexão automática, envio de comandos via `flutter_blue_plus` | Criado no Provider, auto-reconecta em background |
| `SupabaseService` | Bloqueio remoto da cadeira, rastreamento de manutenção (singleton, funciona offline) | `await SupabaseService.initialize()` em `main()` |
| `SettingsService` | Persistência local via `shared_preferences` (horímetro, senhas, intervalos de manutenção) | `await settingsService.loadSettings()` em `main()` |

### Modelos (`lib/models/`)
- **`ChairState`**: Estado em tempo real dos componentes (refletor, pernas, assento, encosto)
- **`ChairCommand`**: Constantes estáticas para todos os comandos - **SEMPRE use estas, nunca hardcode strings**
- **`ChairSettings`**: Config persistente (horímetro, intervalos de manutenção, senha admin)

## Protocolo de Comandos
Comandos são códigos de 2 letras enviados via Bluetooth com terminador newline (`\n`):

| Comando | Ação | Tipo de Envio | Método Handler | Estado Atualizado |
|---------|------|---------------|----------------|-------------------|
| `RF` | Liga/desliga refletor | **Toggle** (uma vez) | `toggleReflector()` | `reflectorOn` |
| `SE` | Encosto sobe (sentar) | **Contínuo** (500ms) | `moveBackUp()` | `backUpOn = true, backDownOn = false` |
| `DE` | Encosto desce (deitar) | **Contínuo** (500ms) | `moveBackDown()` | `backDownOn = true, backUpOn = false` |
| `SA` | Assento sobe | **Contínuo** (500ms) | `moveSeatUp()` | `seatUpOn = true, seatDownOn = false` |
| `DA` | Assento desce | **Contínuo** (500ms) | `moveSeatDown()` | `seatDownOn = true, seatUpOn = false` |
| `SP` | Pernas sobem | **Contínuo** (500ms) | `toggleUpperLegs()` | `upperLegsOn` |
| `DP` | Pernas descem | **Contínuo** (500ms) | `toggleLowerLegs()` | `lowerLegsOn` |
| `VZ` | Posição ginecológica | **Único** (uma vez) | `setGynecologicalPosition()` | Nenhum (sequência automática) |
| `PT` | Posição de parto | **Único** (uma vez) | `setBirthPosition()` | Nenhum (sequência automática) |
| `AT_SEG` | Parada de emergência | **Único** (uma vez) | `stopAll()` | Nenhum (para tudo) |

### Tipos de Envio de Comandos

**Toggle (RF)**:
- Envia o comando **uma única vez** ao pressionar o botão
- ESP32 implementa flip-flop internamente (ON → OFF → ON)
- App apenas alterna o estado visual ao receber confirmação

**Contínuo (SE, DE, SA, DA, SP, DP)**:
- Comando **repetido a cada 500ms** enquanto o botão estiver pressionado
- ESP32 mantém o motor ativo enquanto recebe comandos
- Para automaticamente quando para de receber (timeout no ESP32)

**Único (VZ, PT, AT_SEG)**:
- Envia o comando **uma única vez** ao pressionar
- ESP32 executa sequência completa automaticamente
- Não requer envio repetido

**Importante**: 
- Comandos de movimento (SA/DA, SE/DE) são mutuamente exclusivos - ativar um desativa o oposto automaticamente
- Comandos contínuos devem implementar Timer no Flutter para envio periódico enquanto botão pressionado

## Padrões Críticos

### Adicionando Novos Comandos
1. Adicionar constante na classe `ChairCommand` em lib/models/chair_state.dart
2. Adicionar método handler no `BluetoothService` com atualização de estado + `notifyListeners()`
3. Implementar o processamento no dispositivo BLE conectado
4. Configurar pinos de controle no hardware conforme necessário

### Padrão de Widget
Use o widget `ControlButton` em lib/widgets/control_button.dart para controles da cadeira - fornece estilo consistente com indicação de estado ativo.
```dart
ControlButton(
  label: 'Nome',
  icon: Icons.arrow_upward,
  onPressed: () => context.read<BluetoothService>().comando(),
  color: Colors.blue,
  isActive: context.watch<BluetoothService>().chairState.estadoOn,
)
```

### Convenção de Pinos ESP32
- Entradas usam `INPUT_PULLUP` - botões conectam ao GND
- Saídas controlam relés - `HIGH` = ativo
- Veja README.md para tabela de pinagem completa
- LED indicador: GPIO 2 (pisca quando sistema ativo)
- Buzzer: GPIO 32 (feedback sonoro de operações)

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
Configure as credenciais em lib/services/supabase_service.dart:
```dart
static const String _supabaseUrl = 'SUA_URL';
static const String _supabaseAnonKey = 'SUA_CHAVE';
```
O app funciona totalmente offline quando não configurado - recursos de bloqueio remoto ficam desabilitados.

## Navegação e Rotas
App usa rotas nomeadas definidas em lib/main.dart:
- `/` - `ChairControlScreen` (tela principal de controle)
- `/bluetooth` - `BluetoothConnectionScreen` (conexão com dispositivos)
- `/settings` - `SettingsScreen` (configurações de usuário)
- `/manufacturer` - `ManufacturerSettingsScreen` (configurações avançadas)

## Estrutura de Arquivos
```
lib/
├── main.dart           # Entrada do app, setup do MultiProvider
├── models/             # Classes de dados (ChairState, ChairCommand, ChairSettings)
├── screens/            # Telas completas (controle, bluetooth, configurações)
├── services/           # Lógica de negócio (Bluetooth, Supabase, Settings)
└── widgets/            # Componentes de UI reutilizáveis
```
