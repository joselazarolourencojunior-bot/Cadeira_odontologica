import 'dart:async';
import 'dart:convert';
import 'package:flutter/foundation.dart';
import 'package:flutter/scheduler.dart';
import 'package:flutter/services.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:vibration/vibration.dart';
import '../models/chair_state.dart';

class BluetoothService extends ChangeNotifier {
  BluetoothDevice? _device;
  BluetoothCharacteristic? _txCharacteristic;
  BluetoothCharacteristic? _statusCharacteristic;
  bool _isConnected = false;
  bool _isConnecting = false;
  bool _autoReconnectEnabled = true;
  ChairState _chairState = ChairState();

  StreamSubscription? _scanSubscription;
  StreamSubscription? _connectionStateSubscription;
  StreamSubscription? _statusSubscription;
  Timer? _reconnectTimer;

  // Controle de envio de comandos para evitar sobrecarga
  DateTime? _lastCommandTime;
  String?
  _lastCommand; // Guarda último comando para permitir mudança de direção

  // Chave para salvar último dispositivo conectado
  static const String _lastDeviceIdKey = 'last_connected_device_id';
  static const String _lastDeviceNameKey = 'last_connected_device_name';

  BluetoothDevice? get device => _device;
  bool get isConnected => _isConnected;
  bool get isConnecting => _isConnecting;
  bool get autoReconnectEnabled => _autoReconnectEnabled;
  ChairState get chairState => _chairState;
  String? get connectedDeviceName => _device?.platformName;
  String? get connectedDeviceId => _device?.remoteId.str;

  /// Retorna identificador único da cadeira no formato do banco de dados
  /// Formato: CADEIRA-F8B3B7A5AC08 (MAC sem dois pontos)
  String? get chairUniqueId {
    if (_device == null) return null;
    // Remove os dois pontos do MAC e adiciona prefixo
    final macClean = _device!.remoteId.str.replaceAll(':', '').toUpperCase();
    return 'CADEIRA-$macClean';
  }

  BluetoothService() {
    _initAutoReconnect();
  }

  @override
  void dispose() {
    _scanSubscription?.cancel();
    _connectionStateSubscription?.cancel();
    _statusSubscription?.cancel();
    _reconnectTimer?.cancel();
    super.dispose();
  }

  // Inicializa reconexão automática
  Future<void> _initAutoReconnect() async {
    final prefs = await SharedPreferences.getInstance();
    final lastDeviceId = prefs.getString(_lastDeviceIdKey);

    if (lastDeviceId != null && _autoReconnectEnabled) {
      debugPrint('🔄 Tentando reconectar ao último dispositivo: $lastDeviceId');
      _startAutoReconnect(lastDeviceId);
    }
  }

  // Inicia escaneamento automático para reconexão
  void _startAutoReconnect(String deviceId) {
    _reconnectTimer?.cancel();

    // Tenta reconectar a cada 5 segundos
    _reconnectTimer = Timer.periodic(const Duration(seconds: 5), (_) async {
      if (!_isConnected && !_isConnecting && _autoReconnectEnabled) {
        await _scanForDevice(deviceId);
      }
    });
  }

  // Escaneia por dispositivo específico
  Future<void> _scanForDevice(String deviceId) async {
    try {
      debugPrint('🔍 Procurando dispositivo: $deviceId');

      // Verifica se Bluetooth está ligado
      final adapterState = await FlutterBluePlus.adapterState.first;
      if (adapterState != BluetoothAdapterState.on) {
        debugPrint('⚠️ Bluetooth desligado');
        return;
      }

      // Busca dispositivos conectados ao sistema
      final connectedDevices = await FlutterBluePlus.systemDevices([]);
      for (var device in connectedDevices) {
        if (device.remoteId.str == deviceId) {
          debugPrint('✅ Dispositivo encontrado nos conectados do sistema');
          await _connectToDevice(device);
          return;
        }
      }

      // Inicia scan
      await FlutterBluePlus.startScan(
        timeout: const Duration(seconds: 4),
        androidUsesFineLocation: false,
      );

      _scanSubscription?.cancel();
      _scanSubscription = FlutterBluePlus.scanResults.listen((results) async {
        for (var result in results) {
          if (result.device.remoteId.str == deviceId) {
            debugPrint(
              '✅ Dispositivo encontrado no scan: ${result.device.platformName}',
            );
            await FlutterBluePlus.stopScan();
            await _connectToDevice(result.device);
            break;
          }
        }
      });
    } catch (e) {
      debugPrint('❌ Erro ao escanear: $e');
    }
  }

  // Habilita/desabilita reconexão automática
  void setAutoReconnect(bool enabled) {
    _autoReconnectEnabled = enabled;
    notifyListeners();

    if (!enabled) {
      _reconnectTimer?.cancel();
    } else if (!_isConnected) {
      _initAutoReconnect();
    }
  }

  // Modo Demo - Simular conexão para testes
  void connectDemo() {
    _isConnected = true;
    _isConnecting = false;
    notifyListeners();
  }

  // Conectar ao dispositivo BLE
  Future<bool> connect(BluetoothDevice device) async {
    if (_isConnecting || _isConnected) return false;

    return await _connectToDevice(device);
  }

  Future<bool> _connectToDevice(BluetoothDevice device) async {
    _isConnecting = true;
    _device = device;
    notifyListeners();

    try {
      debugPrint('🔗 Conectando ao ${device.platformName}...');

      // Conecta ao dispositivo
      await device.connect(timeout: const Duration(seconds: 15));

      // Descobre serviços
      debugPrint('🔍 Descobrindo serviços...');
      final services = await device.discoverServices();

      // Encontra as características para enviar comandos e receber status
      for (var service in services) {
        final serviceUuid = service.uuid.toString().toLowerCase();
        debugPrint('📡 Serviço encontrado: $serviceUuid');

        // Verifica se é o serviço Nordic UART da cadeira
        if (serviceUuid == '6e400001-b5a3-f393-e0a9-e50e24dcca9e') {
          debugPrint('🎯 Serviço Nordic UART da cadeira detectado!');
          for (var characteristic in service.characteristics) {
            final charUuid = characteristic.uuid.toString().toLowerCase();
            debugPrint('  📝 Característica: $charUuid');

            // Característica RX para comandos (escrita)
            if (charUuid == '6e400002-b5a3-f393-e0a9-e50e24dcca9e') {
              debugPrint('  ✅ Característica RX (comando) encontrada!');
              _txCharacteristic = characteristic;
            }
            // Característica TX para status (notificação)
            else if (charUuid == '6e400003-b5a3-f393-e0a9-e50e24dcca9e') {
              debugPrint('  ✅ Característica TX (status) encontrada!');
              _statusCharacteristic = characteristic;
            }
          }
        }
      }

      if (_txCharacteristic == null) {
        throw Exception('Característica de comando não encontrada');
      }

      if (_statusCharacteristic == null) {
        throw Exception('Característica de status não encontrada');
      }

      // Configura notificações para receber status
      await _statusCharacteristic!.setNotifyValue(true);
      _statusSubscription?.cancel();
      _statusSubscription = _statusCharacteristic!.lastValueStream.listen((
        value,
      ) {
        _handleStatusUpdate(value);
      });

      // Solicita status inicial
      await sendCommand(ChairCommand.status);

      // Salva o dispositivo para reconexão
      final prefs = await SharedPreferences.getInstance();
      await prefs.setString(_lastDeviceIdKey, device.remoteId.str);
      await prefs.setString(_lastDeviceNameKey, device.platformName);

      _isConnected = true;
      _isConnecting = false;

      // Monitora estado da conexão
      _connectionStateSubscription?.cancel();
      _connectionStateSubscription = device.connectionState.listen((state) {
        if (state == BluetoothConnectionState.disconnected) {
          debugPrint('⚠️ Dispositivo desconectado');
          _handleDisconnection();
        }
      });

      SchedulerBinding.instance.addPostFrameCallback((_) {
        notifyListeners();
      });
      debugPrint('✅ Conectado com sucesso!');
      return true;
    } catch (e) {
      debugPrint('❌ Erro ao conectar: $e');
      _isConnecting = false;
      _isConnected = false;
      SchedulerBinding.instance.addPostFrameCallback((_) {
        notifyListeners();
      });
      return false;
    }
  }

  // Trata atualização de status recebida via BLE
  void _handleStatusUpdate(List<int> value) {
    try {
      final receivedString = utf8.decode(value);
      debugPrint('📥 Dados recebidos: $receivedString');

      // Verifica se é uma resposta de STATUS (JSON)
      if (receivedString.startsWith('STATUS:')) {
        final jsonString = receivedString.substring(7); // Remove "STATUS:"
        debugPrint('📊 Status JSON: $jsonString');

        final Map<String, dynamic> status = json.decode(jsonString);
        _chairState = ChairState.fromJson(status);

        debugPrint('✅ Estado da cadeira atualizado via STATUS');
      }
      // Outras respostas são apenas confirmações de comandos
      else {
        debugPrint('ℹ️ Confirmação recebida: $receivedString');
        // Aqui podemos adicionar lógica para atualizar estado baseado nas confirmações
        // Por exemplo, se receber "DE:ON", podemos marcar estado_de como true
        _parseCommandConfirmation(receivedString);
      }

      // Adia notificação para evitar setState durante build
      SchedulerBinding.instance.addPostFrameCallback((_) {
        notifyListeners();
      });
    } catch (e) {
      debugPrint('❌ Erro ao processar dados recebidos: $e');
    }
  }

  DateTime? _lastBackStop;
  DateTime? _lastSeatStop;
  DateTime? _lastLegStop;

  // Processa confirmações de comandos enviados
  void _parseCommandConfirmation(String confirmation) {
    debugPrint('📥 Confirmação: $confirmation');

    final now = DateTime.now();
    const ignoreWindow = Duration(
      milliseconds: 2500,
    ); // Janela aumentada para 2.5 segundos

    if (confirmation.startsWith('GAVETA:OPEN')) {
      _chairState.gavetaOpen = true;
      _chairState.upperLegsOn = false;
      _chairState.lowerLegsOn = false;
      _chairState.shouldStopAllTimers = true;
      notifyListeners();
      return;
    }

    if (confirmation.startsWith('GAVETA:CLOSED')) {
      _chairState.gavetaOpen = false;
      notifyListeners();
      return;
    }

    if (confirmation == 'SP:GAVETA' || confirmation == 'DP:GAVETA') {
      _chairState.gavetaOpen = true;
      _chairState.upperLegsOn = false;
      _chairState.lowerLegsOn = false;
      _chairState.shouldStopAllTimers = true;
      notifyListeners();
      return;
    }

    // Sempre processamos LIMIT e STOP (críticos)
    if (confirmation.contains('LIMIT')) {
      if (confirmation.startsWith('DE:')) _chairState.backDownLimit = true;
      if (confirmation.startsWith('SE:')) _chairState.backUpLimit = true;
      if (confirmation.startsWith('SA:')) _chairState.seatUpLimit = true;
      if (confirmation.startsWith('DA:')) _chairState.seatDownLimit = true;
      if (confirmation.startsWith('SP:')) _chairState.legUpLimit = true;
      if (confirmation.startsWith('DP:')) _chairState.legDownLimit = true;

      _chairState.shouldStopAllTimers = true;
      notifyListeners();
      return;
    }

    // Se for mensagem de desligado/parado, atualizamos sempre
    if (confirmation.contains('OFF') ||
        confirmation.contains('STOP') ||
        confirmation == 'STOP:OK' ||
        confirmation == 'AT_SEG:OK' ||
        confirmation == 'VZ:OFF' ||
        confirmation == 'PT:OFF') {
      if (confirmation.startsWith('DE:')) _chairState.backDownOn = false;
      if (confirmation.startsWith('SE:')) _chairState.backUpOn = false;
      if (confirmation.startsWith('SA:')) _chairState.seatUpOn = false;
      if (confirmation.startsWith('DA:')) _chairState.seatDownOn = false;
      if (confirmation.startsWith('SP:')) _chairState.upperLegsOn = false;
      if (confirmation.startsWith('DP:')) _chairState.lowerLegsOn = false;

      if (confirmation == 'VZ:OFF') _chairState.isMovingToGineco = false;
      if (confirmation == 'PT:OFF') _chairState.isMovingToParto = false;

      if (confirmation == 'STOP:OK' ||
          confirmation == 'AT_SEG:OK' ||
          confirmation == 'VZ:OFF' ||
          confirmation == 'PT:OFF') {
        stopAllLocal();
      }
      notifyListeners();
      return;
    }

    // Lógica para mensagens de "ON" (ex: DE:ON)
    // Se o App acabou de mandar um STOP (janela de 800ms), ignoramos o ON para evitar "eco"
    if (confirmation.contains('ON') ||
        confirmation == 'VZ:OK' ||
        confirmation == 'PT:OK' ||
        (confirmation.contains(':') && !confirmation.contains('OK'))) {
      if (confirmation.startsWith('DE:')) {
        if (_lastBackStop == null ||
            now.difference(_lastBackStop!) > ignoreWindow) {
          _chairState.backDownOn = true;
          _chairState.backUpOn = false;
        }
      } else if (confirmation.startsWith('SE:')) {
        if (_lastBackStop == null ||
            now.difference(_lastBackStop!) > ignoreWindow) {
          _chairState.backUpOn = true;
          _chairState.backDownOn = false;
        }
      } else if (confirmation.startsWith('SA:')) {
        if (_lastSeatStop == null ||
            now.difference(_lastSeatStop!) > ignoreWindow) {
          _chairState.seatUpOn = true;
          _chairState.seatDownOn = false;
        }
      } else if (confirmation.startsWith('DA:')) {
        if (_lastSeatStop == null ||
            now.difference(_lastSeatStop!) > ignoreWindow) {
          _chairState.seatDownOn = true;
          _chairState.seatUpOn = false;
        }
      } else if (confirmation.startsWith('SP:')) {
        if (_lastLegStop == null ||
            now.difference(_lastLegStop!) > ignoreWindow) {
          _chairState.upperLegsOn = true;
          _chairState.lowerLegsOn = false;
        }
      } else if (confirmation.startsWith('DP:')) {
        if (_lastLegStop == null ||
            now.difference(_lastLegStop!) > ignoreWindow) {
          _chairState.lowerLegsOn = true;
          _chairState.upperLegsOn = false;
        }
      } else if (confirmation == 'VZ:OK') {
        _chairState.isMovingToGineco = true;
        _chairState.isMovingToParto = false;
      } else if (confirmation == 'PT:OK') {
        _chairState.isMovingToParto = true;
        _chairState.isMovingToGineco = false;
      }
    }

    if (confirmation == 'RF:ON') {
      _chairState.reflectorOn = true;
    } else if (confirmation == 'RF:OFF') {
      _chairState.reflectorOn = false;
    } else if (confirmation == 'VZ:OK' || confirmation == 'PT:OK') {
      // Início de automação (VZ/PT) - limpamos os registros de stop para permitir ver a animação
      _lastBackStop = null;
      _lastSeatStop = null;
      _lastLegStop = null;
    }

    notifyListeners();
  }

  // Trata desconexão
  void _handleDisconnection() {
    _isConnected = false;
    _chairState = ChairState();

    // Adia notificação para evitar setState durante build
    SchedulerBinding.instance.addPostFrameCallback((_) {
      notifyListeners();
    });

    // Inicia reconexão automática se habilitada
    if (_autoReconnectEnabled && _device != null) {
      debugPrint('🔄 Iniciando reconexão automática...');
      _startAutoReconnect(_device!.remoteId.str);
    }
  }

  // Desconectar
  Future<void> disconnect() async {
    _reconnectTimer?.cancel();
    _scanSubscription?.cancel();
    _connectionStateSubscription?.cancel();
    _statusSubscription?.cancel();

    if (_device != null) {
      try {
        await _device!.disconnect();
      } catch (e) {
        debugPrint('Erro ao desconectar: $e');
      }
      _device = null;
      _txCharacteristic = null;
      _statusCharacteristic = null;
    }

    _isConnected = false;
    _isConnecting = false;
    _chairState = ChairState();
    notifyListeners();
  }

  // Limpar último dispositivo salvo
  Future<void> clearSavedDevice() async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.remove(_lastDeviceIdKey);
    await prefs.remove(_lastDeviceNameKey);
    _reconnectTimer?.cancel();
  }

  // Enviar comando para o dispositivo
  Future<bool> sendCommand(String command) async {
    if (!_isConnected || _txCharacteristic == null) {
      debugPrint('⚠️ Não conectado ao dispositivo');
      return false;
    }

    // Limitar taxa de envio (mínimo 200ms entre comandos)
    // MAS permite se for comando diferente (mudança de direção)
    if (_lastCommandTime != null && _lastCommand == command) {
      final timeSinceLastCommand = DateTime.now().difference(_lastCommandTime!);
      if (timeSinceLastCommand.inMilliseconds < 200) {
        return false; // Ignora comando IGUAL se muito rápido
      }
    }

    final data = utf8.encode('$command\n');
    // Enviar sem bloquear, para permitir comandos contínuos
    _txCharacteristic!
        .write(data, withoutResponse: false, timeout: 3)
        .then((_) {
          debugPrint('📤 Comando enviado: $command');
        })
        .catchError((e) {
          debugPrint('❌ Erro ao enviar comando: $e');
          // Se erro de conexão, desconectar
          if (e.toString().contains('Timed out') ||
              e.toString().contains(
                'gatt.writeCharacteristic() returned false',
              )) {
            debugPrint('🔴 Conexão perdida - iniciando reconexão');
            _handleDisconnection();
          }
        });
    _lastCommandTime = DateTime.now();
    _lastCommand = command;
    return true;
  }

  // Comandos específicos da cadeira
  Future<void> stopAll() async => await sendCommand(ChairCommand.stopAll);

  void stopAllLocal() {
    debugPrint('🛑 Parando todos os movimentos localmente');
    _chairState.backUpOn = false;
    _chairState.backDownOn = false;
    _chairState.seatUpOn = false;
    _chairState.seatDownOn = false;
    _chairState.upperLegsOn = false;
    _chairState.lowerLegsOn = false;
    _chairState.reflectorOn = false;
    notifyListeners();
  }

  Future<void> setGynecologicalPosition() async =>
      await sendCommand(ChairCommand.gynecologicalPosition);

  Future<void> setBirthPosition() async =>
      await sendCommand(ChairCommand.birthPosition);

  Future<void> toggleReflector() async {
    _chairState.reflectorOn = !_chairState.reflectorOn;
    notifyListeners();
    await sendCommand(ChairCommand.reflector);
    // Confirmação via BLE ajustará se necessário
  }

  // Comandos contínuos de pernas - enviar repetidamente
  Future<void> toggleUpperLegs() async {
    _chairState.upperLegsOn = true;
    _chairState.lowerLegsOn = false;
    // Limpa limites ao tentar mover
    _chairState.legUpLimit = false;
    _chairState.legDownLimit = false;
    _chairState.shouldStopAllTimers = false;
    notifyListeners();
    await sendCommand(ChairCommand.upperLegs);
    // Confirmação via BLE ajustará se necessário
  }

  Future<void> toggleLowerLegs() async {
    _chairState.lowerLegsOn = true;
    _chairState.upperLegsOn = false;
    // Limpa limites ao tentar mover
    _chairState.legUpLimit = false;
    _chairState.legDownLimit = false;
    _chairState.shouldStopAllTimers = false;
    notifyListeners();
    await sendCommand(ChairCommand.lowerLegs);
    // Confirmação via BLE ajustará se necessário
  }

  // Comandos contínuos - devem ser enviados repetidamente a cada 500ms
  Future<void> moveSeatUp() async {
    _chairState.seatUpOn = true;
    _chairState.seatDownOn = false;
    // Limpa limites ao tentar mover
    _chairState.seatUpLimit = false;
    _chairState.seatDownLimit = false;
    _chairState.shouldStopAllTimers = false;
    notifyListeners();
    await sendCommand(ChairCommand.seatUp);
    // Confirmação via BLE ajustará se necessário
  }

  Future<void> moveSeatDown() async {
    _chairState.seatDownOn = true;
    _chairState.seatUpOn = false;
    // Limpa limites ao tentar mover
    _chairState.seatUpLimit = false;
    _chairState.seatDownLimit = false;
    _chairState.shouldStopAllTimers = false;
    notifyListeners();
    await sendCommand(ChairCommand.seatDown);
    // Confirmação via BLE ajustará se necessário
  }

  Future<void> moveBackUp() async {
    _chairState.backUpOn = true;
    _chairState.backDownOn = false;
    // Limpa limites ao tentar mover
    _chairState.backUpLimit = false;
    _chairState.backDownLimit = false;
    _chairState.shouldStopAllTimers = false;
    notifyListeners();
    await sendCommand(ChairCommand.backUp);
    // Confirmação via BLE ajustará se necessário
  }

  Future<void> moveBackDown() async {
    _chairState.backDownOn = true;
    _chairState.backUpOn = false;
    // Limpa limites ao tentar mover
    _chairState.backUpLimit = false;
    _chairState.backDownLimit = false;
    _chairState.shouldStopAllTimers = false;
    notifyListeners();
    await sendCommand(ChairCommand.backDown);
    // Confirmação via BLE ajustará se necessário
  }

  // Para movimentos contínuos quando botão é solto
  void stopSeatMovement() {
    debugPrint('🛑 Parando movimento do assento');
    _lastSeatStop = DateTime.now(); // Marca o momento da parada local
    _chairState.seatUpOn = false;
    _chairState.seatDownOn = false;
    sendCommand(ChairCommand.stop); // Envia comando de parada para o ESP32
    SchedulerBinding.instance.addPostFrameCallback((_) {
      notifyListeners();
    });
  }

  void stopBackMovement() {
    debugPrint('🛑 Parando movimento do encosto');
    _lastBackStop = DateTime.now(); // Marca o momento da parada local
    _chairState.backUpOn = false;
    _chairState.backDownOn = false;
    sendCommand(ChairCommand.stop); // Envia comando de parada para o ESP32
    SchedulerBinding.instance.addPostFrameCallback((_) {
      notifyListeners();
    });
  }

  void stopLegMovement() {
    debugPrint('🛑 Parando movimento das pernas');
    _lastLegStop = DateTime.now(); // Marca o momento da parada local
    _chairState.upperLegsOn = false;
    _chairState.lowerLegsOn = false;
    sendCommand(ChairCommand.stop); // Envia comando de parada para o ESP32
    SchedulerBinding.instance.addPostFrameCallback((_) {
      notifyListeners();
    });
  }

  // Obter último dispositivo conectado
  static Future<String?> getLastDeviceName() async {
    final prefs = await SharedPreferences.getInstance();
    return prefs.getString(_lastDeviceNameKey);
  }

  // Verificar se Bluetooth está habilitado
  static Future<bool> isBluetoothEnabled() async {
    try {
      final state = await FlutterBluePlus.adapterState.first;
      return state == BluetoothAdapterState.on;
    } catch (e) {
      debugPrint('Erro ao verificar estado do Bluetooth: $e');
      return false;
    }
  }

  // Solicitar habilitação do Bluetooth
  static Future<bool> enableBluetooth() async {
    try {
      if (defaultTargetPlatform == TargetPlatform.android) {
        await FlutterBluePlus.turnOn();
        return true;
      }
      return false;
    } catch (e) {
      debugPrint('Erro ao habilitar Bluetooth: $e');
      return false;
    }
  }

  // Iniciar escaneamento de dispositivos
  static Future<void> startScan() async {
    try {
      await FlutterBluePlus.startScan(
        timeout: const Duration(seconds: 15),
        androidUsesFineLocation: false,
      );
    } catch (e) {
      debugPrint('Erro ao iniciar scan: $e');
    }
  }

  // Parar escaneamento
  static Future<void> stopScan() async {
    try {
      await FlutterBluePlus.stopScan();
    } catch (e) {
      debugPrint('Erro ao parar scan: $e');
    }
  }

  // Stream de resultados do scan
  static Stream<List<ScanResult>> get scanResults =>
      FlutterBluePlus.scanResults;

  // Stream do estado do adapter Bluetooth
  static Stream<BluetoothAdapterState> get adapterState =>
      FlutterBluePlus.adapterState;

  /// Toca um beep sonoro para teste usando som do sistema
  Future<void> _playBeep() async {
    try {
      debugPrint('🔊 Tocando beep do sistema...');
      // Toca o som de clique/tap do sistema
      await SystemSound.play(SystemSoundType.click);
      debugPrint('✅ Beep do sistema executado');
    } catch (e) {
      debugPrint('❌ Erro no beep do sistema: $e');
    }
  }

  /// Feedback de alerta quando tenta mover para uma direção bloqueada
  Future<void> _alertLimitReached(String direction) async {
    debugPrint('🚨 ALERTA LIMITE: $direction - Iniciando ALERTA');

    // Verifica se o dispositivo suporta vibração
    bool? hasVibrator = await Vibration.hasVibrator();
    debugPrint('📱 Dispositivo suporta vibração: $hasVibrator');

    // TENTA VIBRAÇÃO E BEEP DE QUALQUER JEITO (mesmo se hasVibrator for false)
    debugPrint(
      '🎯 FORÇANDO tentativa de beep + vibração independente do hasVibrator',
    );

    // Beep + Vibração forçada
    try {
      debugPrint('🔊🎯 FORÇANDO Beep 1 + Vibração 1');
      await _playBeep();
      await Vibration.vibrate(duration: 200);
      debugPrint('✅ FORÇADO: Beep + Vibração 1 executada');

      await Future.delayed(const Duration(milliseconds: 150));

      debugPrint('🔊🎯 FORÇANDO Beep 2 + Vibração 2');
      await _playBeep();
      await Vibration.vibrate(duration: 200);
      debugPrint('✅ FORÇADO: Beep + Vibração 2 executada');

      await Future.delayed(const Duration(milliseconds: 150));

      debugPrint('🔊🎯 FORÇANDO Beep 3 + Vibração 3');
      await _playBeep();
      await Vibration.vibrate(duration: 200);
      debugPrint('✅ FORÇADO: Beep + Vibração 3 executada');
    } catch (e) {
      debugPrint('❌ ERRO FORÇADO na vibração: $e');
      // Fallback: apenas beep múltiplas vezes
      try {
        debugPrint('🔊🔄 FALLBACK: Apenas beeps múltiplos');
        for (int i = 0; i < 5; i++) {
          await _playBeep();
          await Future.delayed(const Duration(milliseconds: 100));
        }
        debugPrint('✅ FALLBACK: Beeps executados');
      } catch (beepError) {
        debugPrint('❌ Até beeps falharam: $beepError');
      }
    }

    debugPrint('🏁 Alerta de limite concluído');
  }

  Future<void> testVibration() async {
    debugPrint('🧪 TESTE DE VIBRAÇÃO: INÍCIO - Função foi chamada!');
    await _alertLimitReached('Teste Manual de Vibração');
    debugPrint('🧪 TESTE DE VIBRAÇÃO: FIM');
  }

  /// Teste simples de vibração sem verificações complexas
  Future<void> testVibrationSimple() async {
    debugPrint('🧪 TESTE SIMPLES: Tentando vibração direta');
    try {
      await Vibration.vibrate(duration: 500);
      debugPrint('✅ TESTE SIMPLES: Vibração executada');
    } catch (e) {
      debugPrint('❌ TESTE SIMPLES: Erro na vibração - $e');
    }
  }

  /// Teste do sistema de timeout - simula movimento sem enviar comando
  Future<void> testTimeoutVibration() async {
    debugPrint(
      '🧪 TESTE TIMEOUT: Simulando movimento sem resposta do dispositivo',
    );
    // Simula apenas o alerta de limite diretamente
    await _alertLimitReached('Teste de Timeout Simulado');
    debugPrint('🧪 TESTE TIMEOUT: Concluído');
  }
}
