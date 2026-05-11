import 'package:flutter/material.dart';
import 'package:flutter/foundation.dart' show kIsWeb;
import 'package:provider/provider.dart';
import 'package:permission_handler/permission_handler.dart';
import '../services/bluetooth_service.dart';
import '../widgets/device_list_widget.dart';

class BluetoothConnectionScreen extends StatefulWidget {
  const BluetoothConnectionScreen({super.key});

  @override
  State<BluetoothConnectionScreen> createState() =>
      _BluetoothConnectionScreenState();
}

class _BluetoothConnectionScreenState extends State<BluetoothConnectionScreen> {
  bool _isLoading = false;

  @override
  void initState() {
    super.initState();
    _checkPermissions();
  }

  Future<void> _checkPermissions() async {
    // Permissões de Bluetooth não são suportadas na web
    if (!kIsWeb) {
      await [
        Permission.bluetooth,
        Permission.bluetoothConnect,
        Permission.bluetoothScan,
        Permission.location,
      ].request();
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Conexão Bluetooth'),
        backgroundColor: Colors.blue.shade700,
        foregroundColor: Colors.white,
        actions: [
          IconButton(
            icon: const Icon(Icons.settings),
            tooltip: 'Configurações',
            onPressed: () => Navigator.pushNamed(context, '/settings'),
          ),
        ],
      ),
      body: Consumer<BluetoothService>(
        builder: (context, bluetoothService, child) {
          if (bluetoothService.isConnected) {
            return _buildConnectedView(bluetoothService);
          } else {
            return _buildConnectionView(bluetoothService);
          }
        },
      ),
    );
  }

  Widget _buildConnectedView(BluetoothService bluetoothService) {
    return Padding(
      padding: const EdgeInsets.all(16.0),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Card(
            color: Colors.green.shade50,
            child: Padding(
              padding: const EdgeInsets.all(16.0),
              child: Row(
                children: [
                  Icon(Icons.bluetooth_connected, color: Colors.green.shade700),
                  const SizedBox(width: 8),
                  const Text(
                    'Conectado à Cadeira',
                    style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold),
                  ),
                ],
              ),
            ),
          ),
          const SizedBox(height: 20),
          ElevatedButton(
            onPressed: () =>
                Navigator.pushReplacementNamed(context, '/control'),
            style: ElevatedButton.styleFrom(
              backgroundColor: Colors.blue.shade700,
              foregroundColor: Colors.white,
              padding: const EdgeInsets.all(16),
            ),
            child: const Text(
              'Ir para Controle da Cadeira',
              style: TextStyle(fontSize: 16),
            ),
          ),
          const SizedBox(height: 10),
          OutlinedButton(
            onPressed: () async {
              await bluetoothService.disconnect();
            },
            child: const Text('Desconectar'),
          ),
        ],
      ),
    );
  }

  Widget _buildConnectionView(BluetoothService bluetoothService) {
    return Padding(
      padding: const EdgeInsets.all(16.0),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Card(
            child: Padding(
              padding: const EdgeInsets.all(16.0),
              child: Column(
                children: [
                  Icon(Icons.bluetooth, size: 48, color: Colors.blue.shade700),
                  const SizedBox(height: 8),
                  const Text(
                    'Cadeira Odontológica/Ginecológica',
                    style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold),
                    textAlign: TextAlign.center,
                  ),
                  const SizedBox(height: 8),
                  const Text(
                    'Conecte-se ao dispositivo ESP32 da cadeira para começar o controle.',
                    textAlign: TextAlign.center,
                    style: TextStyle(color: Colors.grey),
                  ),
                ],
              ),
            ),
          ),
          const SizedBox(height: 20),
          if (_isLoading)
            const Center(child: CircularProgressIndicator())
          else
            ElevatedButton(
              onPressed: bluetoothService.isConnecting ? null : _scanForDevices,
              style: ElevatedButton.styleFrom(
                backgroundColor: Colors.blue.shade700,
                foregroundColor: Colors.white,
                padding: const EdgeInsets.all(16),
              ),
              child: Text(
                bluetoothService.isConnecting
                    ? 'Conectando...'
                    : 'Buscar Dispositivos',
                style: const TextStyle(fontSize: 16),
              ),
            ),
          const SizedBox(height: 10),
          OutlinedButton.icon(
            onPressed: () {
              bluetoothService.connectDemo();
              Navigator.pushReplacementNamed(context, '/control');
            },
            icon: const Icon(Icons.play_circle_outline),
            label: const Text('Modo Demo (Testar sem Bluetooth)'),
            style: OutlinedButton.styleFrom(padding: const EdgeInsets.all(16)),
          ),
          const SizedBox(height: 20),
          const Expanded(child: DeviceListWidget()),
        ],
      ),
    );
  }

  Future<void> _scanForDevices() async {
    setState(() {
      _isLoading = true;
    });

    // Verificar se Bluetooth está habilitado
    bool isEnabled = await BluetoothService.isBluetoothEnabled();
    if (!isEnabled) {
      bool enabled = await BluetoothService.enableBluetooth();
      if (!enabled) {
        _showError('Bluetooth precisa ser habilitado para continuar.');
        setState(() {
          _isLoading = false;
        });
        return;
      }
    }

    setState(() {
      _isLoading = false;
    });
  }

  void _showError(String message) {
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(content: Text(message), backgroundColor: Colors.red),
    );
  }
}
