import 'dart:async';
import 'dart:convert';
import 'package:flutter/foundation.dart';
import 'package:mqtt_client/mqtt_client.dart';
import 'package:mqtt_client/mqtt_server_client.dart';
import '../models/chair_state.dart';
import '../models/chair_state.dart' as chair_commands;
import 'settings_service.dart';

class MqttService extends ChangeNotifier {
  MqttServerClient? _client;
  bool _isConnected = false;
  bool _isConnecting = false;
  ChairState _chairState = ChairState();
  final SettingsService _settingsService;

  MqttService({required SettingsService settingsService})
    : _settingsService = settingsService;

  // Configurações MQTT
  static const String _brokerUrl = 'broker.emqx.io';
  static const int _port = 1883;

  // Tópicos MQTT - usando serial da cadeira
  String get _chairTopicPrefix {
    final serial = _settingsService.settings.chairSerial.trim();
    if (serial.isEmpty) return 'CADEIRA-DESCONHECIDA';

    final normalized = serial.toUpperCase();
    if (normalized.startsWith('CADEIRA-')) return normalized;

    return 'CADEIRA-$normalized';
  }

  String get _clientId => 'APP-$_chairTopicPrefix';
  String get _commandTopic => '$_chairTopicPrefix/command';
  String get _statusTopic => '$_chairTopicPrefix/status';

  bool get isConnected => _isConnected;
  bool get isConnecting => _isConnecting;
  ChairState get chairState => _chairState;

  /// Conecta ao broker MQTT remoto
  Future<void> connect() async {
    if (_isConnected || _isConnecting) return;

    _isConnecting = true;
    notifyListeners();

    try {
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
        _client!.subscribe(_statusTopic, MqttQos.atMostOnce);

        // Configurar listener para mensagens recebidas
        _client!.updates!.listen(_onMessageReceived);

        notifyListeners();
      } else {
        throw Exception('Falha na conexão MQTT');
      }
    } catch (e) {
      _isConnecting = false;
      notifyListeners();
      rethrow;
    }
  }

  /// Desconecta do broker MQTT
  void disconnect() {
    if (_client != null && _isConnected) {
      _client!.disconnect();
    }
    _isConnected = false;
    _isConnecting = false;
    notifyListeners();
  }

  /// Envia um comando via MQTT
  Future<void> sendCommand(String command) async {
    if (!_isConnected || _client == null) {
      throw Exception('MQTT não conectado');
    }

    final builder = MqttClientPayloadBuilder();
    builder.addString(command);

    _client!.publishMessage(
      _commandTopic,
      MqttQos.atMostOnce,
      builder.payload!,
    );
  }

  /// Publica o status da cadeira
  Future<void> publishStatus() async {
    if (!_isConnected || _client == null) return;

    final status = {
      'reflectorOn': _chairState.reflectorOn,
      'backUpOn': _chairState.backUpOn,
      'backDownOn': _chairState.backDownOn,
      'seatUpOn': _chairState.seatUpOn,
      'seatDownOn': _chairState.seatDownOn,
      'upperLegsOn': _chairState.upperLegsOn,
      'lowerLegsOn': _chairState.lowerLegsOn,
      'timestamp': DateTime.now().toIso8601String(),
    };

    final builder = MqttClientPayloadBuilder();
    builder.addString(jsonEncode(status));

    _client!.publishMessage(_statusTopic, MqttQos.atMostOnce, builder.payload!);
  }

  /// Atualiza o estado da cadeira baseado em comandos ou confirmações recebidos
  void updateChairState(String message) {
    final confirmation = message.trim();

    if (confirmation == 'SA:ON') {
      _chairState.seatUpOn = true;
      _chairState.seatDownOn = false;
    } else if (confirmation == 'DA:ON') {
      _chairState.seatDownOn = true;
      _chairState.seatUpOn = false;
    } else if (confirmation == 'SE:ON') {
      _chairState.backUpOn = true;
      _chairState.backDownOn = false;
    } else if (confirmation == 'DE:ON') {
      _chairState.backDownOn = true;
      _chairState.backUpOn = false;
    } else if (confirmation == 'SP:ON') {
      _chairState.upperLegsOn = true;
      _chairState.lowerLegsOn = false;
    } else if (confirmation == 'DP:ON') {
      _chairState.lowerLegsOn = true;
      _chairState.upperLegsOn = false;
    } else if (confirmation == 'RF:ON') {
      _chairState.reflectorOn = true;
    } else if (confirmation == 'RF:OFF') {
      _chairState.reflectorOn = false;
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
    } else if (confirmation.contains(':LIMIT')) {
      // Limite atingido - para o motor correspondente
      if (confirmation.startsWith('DE:')) _chairState.backDownOn = false;
      if (confirmation.startsWith('SE:')) _chairState.backUpOn = false;
      if (confirmation.startsWith('SA:')) _chairState.seatUpOn = false;
      if (confirmation.startsWith('DA:')) _chairState.seatDownOn = false;
      if (confirmation.startsWith('SP:')) _chairState.upperLegsOn = false;
      if (confirmation.startsWith('DP:')) _chairState.lowerLegsOn = false;
    } else if (confirmation == 'AT_SEG:OK') {
      // Emergência ativada - desliga tudo
      _chairState.backUpOn = false;
      _chairState.backDownOn = false;
      _chairState.seatUpOn = false;
      _chairState.seatDownOn = false;
      _chairState.upperLegsOn = false;
      _chairState.lowerLegsOn = false;
      _chairState.reflectorOn = false;
    }

    notifyListeners();
  }

  void _onConnected() {
    _isConnected = true;
    _isConnecting = false;
    notifyListeners();
  }

  void _onDisconnected() {
    _isConnected = false;
    _isConnecting = false;
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

    // Processar mensagem recebida
    if (event[0].topic == _statusTopic) {
      // Status is JSON from ESP32
      try {
        final statusJson = jsonDecode(payload);
        _chairState = ChairState.fromJson(statusJson);
        notifyListeners();
      } catch (e) {
        debugPrint('Erro ao processar status MQTT: $e');
      }
    }
  }

  // Comandos específicos da cadeira
  Future<void> stopAll() async =>
      await sendCommand(chair_commands.ChairCommand.stopAll);

  Future<void> setGynecologicalPosition() async =>
      await sendCommand(chair_commands.ChairCommand.gynecologicalPosition);

  Future<void> setBirthPosition() async =>
      await sendCommand(chair_commands.ChairCommand.birthPosition);

  Future<void> toggleReflector() async {
    _chairState.reflectorOn = !_chairState.reflectorOn;
    notifyListeners();
    await sendCommand(chair_commands.ChairCommand.reflector);
  }

  // Comandos contínuos de pernas
  Future<void> toggleUpperLegs() async {
    _chairState.upperLegsOn = true;
    _chairState.lowerLegsOn = false;
    notifyListeners();
    await sendCommand(chair_commands.ChairCommand.upperLegs);
  }

  Future<void> toggleLowerLegs() async {
    _chairState.lowerLegsOn = true;
    _chairState.upperLegsOn = false;
    notifyListeners();
    await sendCommand(chair_commands.ChairCommand.lowerLegs);
  }

  // Comandos contínuos - devem ser enviados repetidamente
  Future<void> moveSeatUp() async {
    _chairState.seatUpOn = true;
    _chairState.seatDownOn = false;
    notifyListeners();
    await sendCommand(chair_commands.ChairCommand.seatUp);
  }

  Future<void> moveSeatDown() async {
    _chairState.seatDownOn = true;
    _chairState.seatUpOn = false;
    notifyListeners();
    await sendCommand(chair_commands.ChairCommand.seatDown);
  }

  Future<void> moveBackUp() async {
    _chairState.backUpOn = true;
    _chairState.backDownOn = false;
    notifyListeners();
    await sendCommand(chair_commands.ChairCommand.backUp);
  }

  Future<void> moveBackDown() async {
    _chairState.backDownOn = true;
    _chairState.backUpOn = false;
    notifyListeners();
    await sendCommand(chair_commands.ChairCommand.backDown);
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
