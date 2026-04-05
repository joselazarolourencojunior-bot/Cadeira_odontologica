import 'dart:async';
import 'dart:convert';
import 'package:flutter/foundation.dart';
import 'package:mqtt_client/mqtt_client.dart';
import 'package:mqtt_client/mqtt_server_client.dart';
import '../models/chair_state.dart';
import 'settings_service.dart';

class MqttService extends ChangeNotifier {
  MqttServerClient? _client;
  bool _isConnected = false;
  bool _isConnecting = false;
  ChairState _chairState = ChairState();
  final SettingsService _settingsService;
  String? _lastError;
  late final String _sessionId = DateTime.now().millisecondsSinceEpoch
      .toString();
  late String _lastChairTopicPrefix;
  final Map<String, Timer> _rxWatchdogs = {};

  MqttService({required SettingsService settingsService})
    : _settingsService = settingsService {
    _lastChairTopicPrefix = _chairTopicPrefix;
    _settingsService.addListener(_onSettingsChanged);
  }

  // Configurações MQTT
  static const String _brokerUrl = 'test.mosquitto.org';
  static const int _port = 1883;

  // Tópicos MQTT - usando serial da cadeira
  String get _chairTopicPrefix {
    final serial = _settingsService.bluetoothSerial?.trim() ?? '';
    if (serial.isEmpty) return 'CADEIRA-DESCONHECIDA';

    final normalized = serial.toUpperCase();
    if (normalized.startsWith('CADEIRA-')) return normalized;

    return 'CADEIRA-$normalized';
  }

  String get _clientId => 'APP-$_chairTopicPrefix-$_sessionId';
  String get _commandTopic => '$_chairTopicPrefix/command';
  String get _statusTopic => '$_chairTopicPrefix/status';

  bool get isConnected => _isConnected;
  bool get isConnecting => _isConnecting;
  ChairState get chairState => _chairState;
  String? get lastError => _lastError;
  bool get hasValidSerial =>
      (_settingsService.bluetoothSerial?.trim().isNotEmpty ?? false);

  @override
  void dispose() {
    _settingsService.removeListener(_onSettingsChanged);
    super.dispose();
  }

  void _onSettingsChanged() {
    final nextPrefix = _chairTopicPrefix;
    if (nextPrefix == _lastChairTopicPrefix) return;
    _lastChairTopicPrefix = nextPrefix;
    if (_isConnected || _isConnecting) {
      disconnect();
    }
    if (!hasValidSerial) {
      _lastError = 'Aguardando Bluetooth para obter SERIAL e habilitar MQTT';
      _isConnected = false;
      _isConnecting = false;
      notifyListeners();
      return;
    }
    Future.microtask(() {
      connect();
    });
  }

  /// Conecta ao broker MQTT remoto
  Future<void> connect() async {
    if (_isConnected || _isConnecting) return;
    if (!hasValidSerial) {
      _lastError = 'Aguardando Bluetooth para obter SERIAL e habilitar MQTT';
      _isConnected = false;
      _isConnecting = false;
      notifyListeners();
      return;
    }

    _isConnecting = true;
    _lastError = null;
    notifyListeners();

    try {
      _lastChairTopicPrefix = _chairTopicPrefix;
      _client = MqttServerClient(_brokerUrl, _clientId);
      _client!.port = _port;
      _client!.secure = false;
      _client!.logging(on: false);
      _client!.keepAlivePeriod = 20;
      _client!.onDisconnected = _onDisconnected;
      _client!.onConnected = _onConnected;
      _client!.onSubscribed = _onSubscribed;

      final connMessage = MqttConnectMessage()
          .withClientIdentifier(_clientId)
          .startClean()
          .withWillQos(MqttQos.atMostOnce);

      _client!.connectionMessage = connMessage;

      await _client!.connect();

      if (_client!.connectionStatus!.state == MqttConnectionState.connected) {
        _isConnected = true;
        _isConnecting = false;

        // Subscrever aos tópicos
        _client!.subscribe(_commandTopic, MqttQos.atMostOnce);
        _client!.subscribe(_statusTopic, MqttQos.atMostOnce);

        // Configurar listener para mensagens recebidas
        _client!.updates!.listen(_onMessageReceived);

        notifyListeners();
      } else {
        throw Exception('Falha na conexão MQTT');
      }
    } catch (e) {
      _lastError = e.toString();
      debugPrint('❌ MQTT connect error: $e');
      _isConnecting = false;
      _isConnected = false;
      notifyListeners();
    }
  }

  /// Desconecta do broker MQTT
  void disconnect() {
    if (_client != null && _isConnected) {
      _client!.disconnect();
    }
    _isConnected = false;
    _isConnecting = false;
    for (final timer in _rxWatchdogs.values) {
      timer.cancel();
    }
    _rxWatchdogs.clear();
    notifyListeners();
  }

  /// Envia um comando via MQTT
  Future<void> sendCommand(String command) async {
    final msg = 'MQTT em modo somente leitura (RX). Comando não enviado.';
    if (_lastError != msg) {
      _lastError = msg;
      notifyListeners();
    }
  }

  /// Publica o status da cadeira
  Future<void> publishStatus() async {
    final msg = 'MQTT em modo somente leitura (RX). Status não publicado.';
    if (_lastError != msg) {
      _lastError = msg;
      notifyListeners();
    }
  }

  /// Atualiza o estado da cadeira baseado em comandos ou confirmações recebidos
  void updateChairState(String message) {
    final confirmation = message.trim();

    if (confirmation == 'SA:ON') {
      _chairState.seatUpOn = true;
      _chairState.seatDownOn = false;
      _chairState.shouldStopAllTimers = false;
      _armWatchdog('seat', () {
        _chairState.seatUpOn = false;
        _chairState.seatDownOn = false;
      });
    } else if (confirmation == 'DA:ON') {
      _chairState.seatDownOn = true;
      _chairState.seatUpOn = false;
      _chairState.shouldStopAllTimers = false;
      _armWatchdog('seat', () {
        _chairState.seatUpOn = false;
        _chairState.seatDownOn = false;
      });
    } else if (confirmation == 'DE:ON') {
      _chairState.backUpOn = true;
      _chairState.backDownOn = false;
      _chairState.shouldStopAllTimers = false;
      _armWatchdog('back', () {
        _chairState.backUpOn = false;
        _chairState.backDownOn = false;
      });
    } else if (confirmation == 'SE:ON') {
      _chairState.backDownOn = true;
      _chairState.backUpOn = false;
      _chairState.shouldStopAllTimers = false;
      _armWatchdog('back', () {
        _chairState.backUpOn = false;
        _chairState.backDownOn = false;
      });
    } else if (confirmation == 'SP:ON') {
      _chairState.upperLegsOn = true;
      _chairState.lowerLegsOn = false;
      _chairState.shouldStopAllTimers = false;
      _armWatchdog('legs', () {
        _chairState.upperLegsOn = false;
        _chairState.lowerLegsOn = false;
      });
    } else if (confirmation == 'DP:ON') {
      _chairState.lowerLegsOn = true;
      _chairState.upperLegsOn = false;
      _chairState.shouldStopAllTimers = false;
      _armWatchdog('legs', () {
        _chairState.upperLegsOn = false;
        _chairState.lowerLegsOn = false;
      });
    } else if (confirmation == 'RF:ON') {
      _chairState.reflectorOn = true;
    } else if (confirmation == 'RF:OFF') {
      _chairState.reflectorOn = false;
    } else if (confirmation == ChairCommand.seatUp) {
      _chairState.seatUpOn = true;
      _chairState.seatDownOn = false;
      _chairState.shouldStopAllTimers = false;
      _armWatchdog('seat', () {
        _chairState.seatUpOn = false;
        _chairState.seatDownOn = false;
      });
    } else if (confirmation == ChairCommand.seatDown) {
      _chairState.seatDownOn = true;
      _chairState.seatUpOn = false;
      _chairState.shouldStopAllTimers = false;
      _armWatchdog('seat', () {
        _chairState.seatUpOn = false;
        _chairState.seatDownOn = false;
      });
    } else if (confirmation == ChairCommand.backUp) {
      _chairState.backUpOn = true;
      _chairState.backDownOn = false;
      _chairState.shouldStopAllTimers = false;
      _armWatchdog('back', () {
        _chairState.backUpOn = false;
        _chairState.backDownOn = false;
      });
    } else if (confirmation == ChairCommand.backDown) {
      _chairState.backDownOn = true;
      _chairState.backUpOn = false;
      _chairState.shouldStopAllTimers = false;
      _armWatchdog('back', () {
        _chairState.backUpOn = false;
        _chairState.backDownOn = false;
      });
    } else if (confirmation == ChairCommand.upperLegs) {
      _chairState.upperLegsOn = true;
      _chairState.lowerLegsOn = false;
      _chairState.shouldStopAllTimers = false;
      _armWatchdog('legs', () {
        _chairState.upperLegsOn = false;
        _chairState.lowerLegsOn = false;
      });
    } else if (confirmation == ChairCommand.lowerLegs) {
      _chairState.lowerLegsOn = true;
      _chairState.upperLegsOn = false;
      _chairState.shouldStopAllTimers = false;
      _armWatchdog('legs', () {
        _chairState.upperLegsOn = false;
        _chairState.lowerLegsOn = false;
      });
    } else if (confirmation == ChairCommand.gynecologicalPosition ||
        confirmation == 'VZ:OK') {
      _chairState.isMovingToGineco = true;
      _chairState.isMovingToParto = false;
    } else if (confirmation == ChairCommand.birthPosition ||
        confirmation == 'PT:OK') {
      _chairState.isMovingToParto = true;
      _chairState.isMovingToGineco = false;
    } else if (confirmation == 'VZ:OFF') {
      _chairState.isMovingToGineco = false;
    } else if (confirmation == 'PT:OFF') {
      _chairState.isMovingToParto = false;
    } else if (confirmation == ChairCommand.stop ||
        confirmation == ChairCommand.stopAll ||
        confirmation == 'STOP:OK') {
      _chairState.backUpOn = false;
      _chairState.backDownOn = false;
      _chairState.seatUpOn = false;
      _chairState.seatDownOn = false;
      _chairState.upperLegsOn = false;
      _chairState.lowerLegsOn = false;
      _rxWatchdogs.remove('seat')?.cancel();
      _rxWatchdogs.remove('back')?.cancel();
      _rxWatchdogs.remove('legs')?.cancel();
    } else if (confirmation == 'VZ:DONE' ||
        confirmation == 'PT:DONE' ||
        confirmation == 'M1:DONE') {
      // Sequências concluídas - desliga todos os estados
      _chairState.backUpOn = false;
      _chairState.backDownOn = false;
      _chairState.seatUpOn = false;
      _chairState.seatDownOn = false;
      _chairState.upperLegsOn = false;
      _chairState.lowerLegsOn = false;
      _chairState.isMovingToGineco = false;
      _chairState.isMovingToParto = false;
      _rxWatchdogs.remove('seat')?.cancel();
      _rxWatchdogs.remove('back')?.cancel();
      _rxWatchdogs.remove('legs')?.cancel();
    } else if (confirmation.contains(':LIMIT')) {
      // Limite atingido - para o motor correspondente
      if (confirmation.startsWith('DE:')) {
        _chairState.backDownOn = false;
        _chairState.backDownLimit = true;
        _armLimitClear(
          'backDownLimit',
          () => _chairState.backDownLimit = false,
        );
      }
      if (confirmation.startsWith('SE:')) {
        _chairState.backUpOn = false;
        _chairState.backUpLimit = true;
        _armLimitClear('backUpLimit', () => _chairState.backUpLimit = false);
      }
      if (confirmation.startsWith('SA:')) {
        _chairState.seatUpOn = false;
        _chairState.seatUpLimit = true;
        _armLimitClear('seatUpLimit', () => _chairState.seatUpLimit = false);
      }
      if (confirmation.startsWith('DA:')) {
        _chairState.seatDownOn = false;
        _chairState.seatDownLimit = true;
        _armLimitClear(
          'seatDownLimit',
          () => _chairState.seatDownLimit = false,
        );
      }
      if (confirmation.startsWith('SP:')) {
        _chairState.upperLegsOn = false;
        _chairState.legUpLimit = true;
        _armLimitClear('legUpLimit', () => _chairState.legUpLimit = false);
      }
      if (confirmation.startsWith('DP:')) {
        _chairState.lowerLegsOn = false;
        _chairState.legDownLimit = true;
        _armLimitClear('legDownLimit', () => _chairState.legDownLimit = false);
      }
      _chairState.shouldStopAllTimers = true;
    } else if (confirmation == 'AT_SEG:OK') {
      // Emergência ativada - desliga tudo
      _chairState.backUpOn = false;
      _chairState.backDownOn = false;
      _chairState.seatUpOn = false;
      _chairState.seatDownOn = false;
      _chairState.upperLegsOn = false;
      _chairState.lowerLegsOn = false;
      _chairState.reflectorOn = false;
      _chairState.isMovingToGineco = false;
      _chairState.isMovingToParto = false;
      _rxWatchdogs.remove('seat')?.cancel();
      _rxWatchdogs.remove('back')?.cancel();
      _rxWatchdogs.remove('legs')?.cancel();
    }

    notifyListeners();
  }

  void _armWatchdog(String key, VoidCallback clear) {
    _rxWatchdogs.remove(key)?.cancel();
    _rxWatchdogs[key] = Timer(const Duration(milliseconds: 900), () {
      clear();
      notifyListeners();
    });
  }

  void _armLimitClear(String key, VoidCallback clear) {
    _rxWatchdogs.remove(key)?.cancel();
    _rxWatchdogs[key] = Timer(const Duration(seconds: 2), () {
      clear();
      notifyListeners();
    });
  }

  void _onConnected() {
    _isConnected = true;
    _isConnecting = false;
    notifyListeners();
  }

  void _onDisconnected() {
    _isConnected = false;
    _isConnecting = false;
    _lastError ??= 'Desconectado';
    notifyListeners();
  }

  void _onSubscribed(String topic) {
    // Lidar com subscrição bem-sucedida se necessário
  }

  void _onMessageReceived(List<MqttReceivedMessage<MqttMessage>> event) {
    final recMess = event[0].payload as MqttPublishMessage;
    final payload = MqttPublishPayload.bytesToStringAsString(
      recMess.payload.message,
    );

    final topic = event[0].topic;

    if (topic == _statusTopic) {
      try {
        final decoded = jsonDecode(payload);
        if (decoded is Map<String, dynamic>) {
          _chairState = ChairState.fromJson(decoded);
          notifyListeners();
          return;
        }
      } catch (_) {}
      updateChairState(payload);
      return;
    }

    if (topic == _commandTopic) {
      updateChairState(payload);
      return;
    }
  }

  // Comandos específicos da cadeira
  Future<void> stopAll() async => await sendCommand(ChairCommand.stopAll);

  Future<void> setGynecologicalPosition() async =>
      await sendCommand(ChairCommand.gynecologicalPosition);

  Future<void> setBirthPosition() async =>
      await sendCommand(ChairCommand.birthPosition);

  Future<void> toggleReflector() async {
    _chairState.reflectorOn = !_chairState.reflectorOn;
    notifyListeners();
    await sendCommand(ChairCommand.reflector);
  }

  // Comandos contínuos de pernas
  Future<void> toggleUpperLegs() async {
    _chairState.upperLegsOn = true;
    _chairState.lowerLegsOn = false;
    notifyListeners();
    await sendCommand(ChairCommand.upperLegs);
  }

  Future<void> toggleLowerLegs() async {
    _chairState.lowerLegsOn = true;
    _chairState.upperLegsOn = false;
    notifyListeners();
    await sendCommand(ChairCommand.lowerLegs);
  }

  // Comandos contínuos - devem ser enviados repetidamente
  Future<void> moveSeatUp() async {
    _chairState.seatUpOn = true;
    _chairState.seatDownOn = false;
    notifyListeners();
    await sendCommand(ChairCommand.seatUp);
  }

  Future<void> moveSeatDown() async {
    _chairState.seatDownOn = true;
    _chairState.seatUpOn = false;
    notifyListeners();
    await sendCommand(ChairCommand.seatDown);
  }

  Future<void> moveBackUp() async {
    _chairState.backUpOn = true;
    _chairState.backDownOn = false;
    notifyListeners();
    await sendCommand(ChairCommand.backUp);
  }

  Future<void> moveBackDown() async {
    _chairState.backDownOn = true;
    _chairState.backUpOn = false;
    notifyListeners();
    await sendCommand(ChairCommand.backDown);
  }

  // Para movimentos contínuos quando botão é solto
  void stopSeatMovement() {
    _chairState.seatUpOn = false;
    _chairState.seatDownOn = false;
    notifyListeners();
  }

  void stopBackMovement() {
    _chairState.backUpOn = false;
    _chairState.backDownOn = false;
    notifyListeners();
  }

  void stopLegMovement() {
    _chairState.upperLegsOn = false;
    _chairState.lowerLegsOn = false;
    notifyListeners();
  }
}
