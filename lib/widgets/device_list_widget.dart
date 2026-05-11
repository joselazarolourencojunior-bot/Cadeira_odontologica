import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'package:flutter_bluetooth_serial/flutter_bluetooth_serial.dart';
import '../services/bluetooth_service.dart';

class DeviceListWidget extends StatefulWidget {
  const DeviceListWidget({super.key});

  @override
  State<DeviceListWidget> createState() => _DeviceListWidgetState();
}

class _DeviceListWidgetState extends State<DeviceListWidget> {
  List<BluetoothDevice> _devices = [];
  bool _isLoading = false;

  @override
  void initState() {
    super.initState();
    _loadDevices();
  }

  Future<void> _loadDevices() async {
    setState(() {
      _isLoading = true;
    });

    try {
      final devices = await BluetoothService.getPairedDevices();
      setState(() {
        _devices = devices;
        _isLoading = false;
      });
    } catch (e) {
      setState(() {
        _isLoading = false;
      });
      _showError('Erro ao carregar dispositivos: $e');
    }
  }

  @override
  Widget build(BuildContext context) {
    if (_isLoading) {
      return const Center(child: CircularProgressIndicator());
    }

    if (_devices.isEmpty) {
      return Center(
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            Icon(
              Icons.bluetooth_disabled,
              size: 48,
              color: Colors.grey.shade400,
            ),
            const SizedBox(height: 16),
            Text(
              'Nenhum dispositivo pareado encontrado',
              style: TextStyle(color: Colors.grey.shade600, fontSize: 16),
            ),
            const SizedBox(height: 8),
            Text(
              'Pareie o ESP32 da cadeira nas configurações do Bluetooth',
              textAlign: TextAlign.center,
              style: TextStyle(color: Colors.grey.shade500, fontSize: 14),
            ),
            const SizedBox(height: 16),
            ElevatedButton(
              onPressed: _loadDevices,
              child: const Text('Atualizar Lista'),
            ),
          ],
        ),
      );
    }

    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        Text(
          'Dispositivos Pareados (${_devices.length})',
          style: const TextStyle(fontSize: 16, fontWeight: FontWeight.bold),
        ),
        const SizedBox(height: 10),
        Expanded(
          child: ListView.builder(
            itemCount: _devices.length,
            itemBuilder: (context, index) {
              final device = _devices[index];
              return Card(
                child: Consumer<BluetoothService>(
                  builder: (context, bluetoothService, child) {
                    return ListTile(
                      leading: Icon(
                        Icons.bluetooth,
                        color: Colors.blue.shade700,
                      ),
                      title: Text(
                        device.name ?? 'Dispositivo Desconhecido',
                        style: const TextStyle(fontWeight: FontWeight.bold),
                      ),
                      subtitle: Text(device.address),
                      trailing: bluetoothService.isConnecting
                          ? const CircularProgressIndicator()
                          : ElevatedButton(
                              onPressed: () => _connectToDevice(device),
                              style: ElevatedButton.styleFrom(
                                backgroundColor: Colors.blue.shade700,
                                foregroundColor: Colors.white,
                              ),
                              child: const Text('Conectar'),
                            ),
                      onTap: bluetoothService.isConnecting
                          ? null
                          : () => _connectToDevice(device),
                    );
                  },
                ),
              );
            },
          ),
        ),
        ElevatedButton(
          onPressed: _loadDevices,
          child: const Text('Atualizar Lista'),
        ),
      ],
    );
  }

  Future<void> _connectToDevice(BluetoothDevice device) async {
    final bluetoothService = Provider.of<BluetoothService>(
      context,
      listen: false,
    );

    final success = await bluetoothService.connect(device);

    if (success) {
      _showSuccess('Conectado com sucesso ao ${device.name}!');
      // Navegar para a tela de controle será feito automaticamente
      // pelo listener no BluetoothConnectionScreen
    } else {
      _showError('Falha ao conectar ao dispositivo ${device.name}');
    }
  }

  void _showError(String message) {
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(content: Text(message), backgroundColor: Colors.red),
    );
  }

  void _showSuccess(String message) {
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(content: Text(message), backgroundColor: Colors.green),
    );
  }
}
