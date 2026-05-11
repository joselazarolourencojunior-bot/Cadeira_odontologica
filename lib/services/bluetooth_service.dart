import 'dart:convert';
import 'package:flutter/foundation.dart';
import 'package:flutter_bluetooth_serial/flutter_bluetooth_serial.dart';
import '../models/chair_state.dart';

class BluetoothService extends ChangeNotifier {
  BluetoothConnection? _connection;
  bool _isConnected = false;
  bool _isConnecting = false;
  ChairState _chairState = ChairState();

  BluetoothConnection? get connection => _connection;
  bool get isConnected => _isConnected;
  bool get isConnecting => _isConnecting;
  ChairState get chairState => _chairState;

  // Modo Demo - Simular conexão para testes
  void connectDemo() {
    _isConnected = true;
    _isConnecting = false;
    notifyListeners();
  }

  // Conectar ao dispositivo Bluetooth
  Future<bool> connect(BluetoothDevice device) async {
    if (_isConnecting || _isConnected) return false;

    _isConnecting = true;
    notifyListeners();

    try {
      _connection = await BluetoothConnection.toAddress(device.address);
      _isConnected = true;
      _isConnecting = false;

      // Escutar dados recebidos
      _connection!.input!.listen(_onDataReceived).onDone(() {
        disconnect();
      });

      notifyListeners();
      return true;
    } catch (e) {
      debugPrint('Erro ao conectar: $e');
      _isConnecting = false;
      notifyListeners();
      return false;
    }
  }

  // Desconectar
  Future<void> disconnect() async {
    if (_connection != null) {
      await _connection!.close();
      _connection = null;
    }
    _isConnected = false;
    _isConnecting = false;
    _chairState = ChairState();
    notifyListeners();
  }

  // Enviar comando para o ESP32
  Future<bool> sendCommand(String command) async {
    if (!_isConnected || _connection == null) {
      debugPrint('Não conectado ao dispositivo');
      return false;
    }

    try {
      _connection!.output.add(Uint8List.fromList(utf8.encode('$command\n')));
      await _connection!.output.allSent;
      debugPrint('Comando enviado: $command');
      return true;
    } catch (e) {
      debugPrint('Erro ao enviar comando: $e');
      return false;
    }
  }

  // Processar dados recebidos do ESP32
  void _onDataReceived(Uint8List data) {
    String dataString = utf8.decode(data);
    debugPrint('Dados recebidos: $dataString');

    // Aqui você pode processar respostas do ESP32 se necessário
    // Por exemplo, atualizar o estado da cadeira baseado nas respostas
  }

  // Comandos específicos da cadeira
  Future<void> stopAll() async => await sendCommand(ChairCommand.stopAll);

  Future<void> setGynecologicalPosition() async =>
      await sendCommand(ChairCommand.gynecologicalPosition);

  Future<void> setBirthPosition() async =>
      await sendCommand(ChairCommand.birthPosition);

  Future<void> toggleReflector() async {
    await sendCommand(ChairCommand.reflector);
    _chairState.reflectorOn = !_chairState.reflectorOn;
    notifyListeners();
  }

  Future<void> toggleUpperLegs() async {
    await sendCommand(ChairCommand.upperLegs);
    _chairState.upperLegsOn = !_chairState.upperLegsOn;
    notifyListeners();
  }

  Future<void> toggleLowerLegs() async {
    await sendCommand(ChairCommand.lowerLegs);
    _chairState.lowerLegsOn = !_chairState.lowerLegsOn;
    notifyListeners();
  }

  Future<void> moveSeatUp() async {
    await sendCommand(ChairCommand.seatUp);
    _chairState.seatUpOn = true;
    _chairState.seatDownOn = false;
    notifyListeners();
  }

  Future<void> moveSeatDown() async {
    await sendCommand(ChairCommand.seatDown);
    _chairState.seatDownOn = true;
    _chairState.seatUpOn = false;
    notifyListeners();
  }

  Future<void> moveBackUp() async {
    await sendCommand(ChairCommand.backUp);
    _chairState.backUpOn = true;
    _chairState.backDownOn = false;
    notifyListeners();
  }

  Future<void> moveBackDown() async {
    await sendCommand(ChairCommand.backDown);
    _chairState.backDownOn = true;
    _chairState.backUpOn = false;
    notifyListeners();
  }

  // Obter lista de dispositivos Bluetooth pareados
  static Future<List<BluetoothDevice>> getPairedDevices() async {
    try {
      return await FlutterBluetoothSerial.instance.getBondedDevices();
    } catch (e) {
      debugPrint('Erro ao obter dispositivos pareados: $e');
      return [];
    }
  }

  // Verificar se Bluetooth está habilitado
  static Future<bool> isBluetoothEnabled() async {
    try {
      return await FlutterBluetoothSerial.instance.isEnabled ?? false;
    } catch (e) {
      debugPrint('Erro ao verificar estado do Bluetooth: $e');
      return false;
    }
  }

  // Solicitar habilitação do Bluetooth
  static Future<bool> enableBluetooth() async {
    try {
      return await FlutterBluetoothSerial.instance.requestEnable() ?? false;
    } catch (e) {
      debugPrint('Erro ao habilitar Bluetooth: $e');
      return false;
    }
  }
}
