# Cadeira Odontológica - Instruções para Agentes de IA

## Visão Geral
App Flutter + firmware ESP32 para controle de cadeira odontológica/ginecológica via Bluetooth. O app comunica com o ESP32 usando comandos de texto de 2 letras via Bluetooth Serial.

## Arquitetura

### Fluxo de Dados
```
App Flutter → BluetoothService → Bluetooth Serial → ESP32 → Relés → Motores da Cadeira
```

### Serviços Principais (Padrão Provider)
Todos na pasta **services**:
- **BluetoothService**: Gerencia conexão Bluetooth e envio de comandos via flutter_bluetooth_serial
- **SupabaseService**: Backend para bloqueio remoto e rastreamento de manutenção (singleton)
- **SettingsService**: Persiste configurações localmente via shared_preferences

### Modelos
Todos na pasta **models**:
- **ChairState**: Estado em tempo real dos componentes (refletor, pernas, assento, encosto)
- **ChairCommand**: Classe estática com constantes de comandos - SEMPRE use estas, nunca hardcode strings
- **ChairSettings**: Config persistente (horímetro, intervalos de manutenção, senha admin)

## Protocolo de Comandos
Comandos são códigos de 2 letras enviados via Bluetooth com terminador newline:

| Comando | Ação |
|---------|------|
| `RF` | Liga/desliga refletor |
| `SE` | Encosto sobe (sentar) |
| `DE` | Encosto desce (deitar) |
| `SA` | Assento sobe |
| `DA` | Assento desce |
| `SP` | Pernas sobem |
| `DP` | Pernas descem |
| `VZ` | Posição ginecológica |
| `PT` | Posição de parto |
| `AT_SEG` | Parada de emergência |

## Padrões Críticos

### Adicionando Novos Comandos
1. Adicionar constante na classe **ChairCommand** (arquivo chair_state.dart na pasta models)
2. Adicionar handler no **BluetoothService** com atualização de estado + notifyListeners()
3. Adicionar processamento no firmware ESP32 na função processBluetoothCommand()
4. Adicionar pino de relé se precisar de novo controle de hardware

### Gerenciamento de Estado
- Todos os serviços estendem ChangeNotifier e são providos via MultiProvider no main.dart
- Use context.read<Service>() para acesso único, context.watch<Service>() para rebuilds

### Configuração de Pinos ESP32
Pinos de entrada usam INPUT_PULLUP - botões conectam ao GND. Pinos de saída controlam relés (HIGH = ativo). Veja README.md para pinagem completa.

## Comandos de Desenvolvimento
```bash
flutter pub get          # Instalar dependências
flutter run              # Rodar no dispositivo conectado (precisa Bluetooth)
flutter build apk        # Compilar APK Android
```

## Integração Supabase
Credenciais devem ser configuradas no SupabaseService:
```dart
static const String _supabaseUrl = 'SUA_URL';
static const String _supabaseAnonKey = 'SUA_CHAVE';
```
O app funciona offline quando Supabase não está configurado - recursos de bloqueio remoto ficam desabilitados.

## Testes sem Hardware
Use BluetoothService.connectDemo() para simular conexão e testar a UI.
