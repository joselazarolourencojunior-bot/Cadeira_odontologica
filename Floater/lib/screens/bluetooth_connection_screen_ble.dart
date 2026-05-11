import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'package:permission_handler/permission_handler.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart' as fbp;
import '../services/bluetooth_service.dart' as app;

class BluetoothConnectionScreen extends StatefulWidget {
  const BluetoothConnectionScreen({super.key});

  @override
  State<BluetoothConnectionScreen> createState() =>
      _BluetoothConnectionScreenState();
}

class _BluetoothConnectionScreenState extends State<BluetoothConnectionScreen> {
  bool _isScanning = false;

  @override
  void initState() {
    super.initState();
    _checkPermissions();
  }

  Future<void> _checkPermissions() async {
    await [
      Permission.bluetoothConnect,
      Permission.bluetoothScan,
      Permission.location,
    ].request();
  }

  Future<void> _startScan() async {
    setState(() => _isScanning = true);
    await fbp.FlutterBluePlus.startScan();
    // O scan para automaticamente após timeout
    Future.delayed(const Duration(seconds: 15), () {
      if (mounted) setState(() => _isScanning = false);
    });
  }

  @override
  Widget build(BuildContext context) {
    final bluetoothService = context.watch<app.BluetoothService>();

    return Scaffold(
      appBar: AppBar(
        title: const Text('Conexão Bluetooth BLE'),
        backgroundColor: Colors.blue.shade700,
        foregroundColor: Colors.white,
      ),
      body: Column(
        children: [
          // Status da conexão
          Container(
            width: double.infinity,
            padding: const EdgeInsets.all(16),
            color: bluetoothService.isConnected
                ? Colors.green.shade50
                : Colors.orange.shade50,
            child: Row(
              children: [
                Icon(
                  bluetoothService.isConnected
                      ? Icons.bluetooth_connected
                      : Icons.bluetooth_searching,
                  color: bluetoothService.isConnected
                      ? Colors.green.shade700
                      : Colors.orange.shade700,
                  size: 32,
                ),
                const SizedBox(width: 12),
                Expanded(
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(
                        bluetoothService.isConnected
                            ? 'Conectado'
                            : 'Desconectado',
                        style: TextStyle(
                          fontSize: 18,
                          fontWeight: FontWeight.bold,
                          color: bluetoothService.isConnected
                              ? Colors.green.shade700
                              : Colors.orange.shade700,
                        ),
                      ),
                      if (bluetoothService.connectedDeviceName != null)
                        Text(
                          bluetoothService.connectedDeviceName!,
                          style: const TextStyle(fontSize: 14),
                        ),
                    ],
                  ),
                ),
                // Switch de reconexão automática
                Column(
                  children: [
                    Switch(
                      value: bluetoothService.autoReconnectEnabled,
                      onChanged: (value) {
                        bluetoothService.setAutoReconnect(value);
                      },
                    ),
                    const Text(
                      'Auto-reconectar',
                      style: TextStyle(fontSize: 10),
                    ),
                  ],
                ),
              ],
            ),
          ),

          // Botões de ação
          if (bluetoothService.isConnected)
            Padding(
              padding: const EdgeInsets.all(16),
              child: Column(
                children: [
                  SizedBox(
                    width: double.infinity,
                    child: ElevatedButton.icon(
                      onPressed: () => Navigator.pop(context),
                      icon: const Icon(Icons.arrow_back),
                      label: const Text('Voltar ao Controle'),
                      style: ElevatedButton.styleFrom(
                        backgroundColor: Colors.blue.shade700,
                        foregroundColor: Colors.white,
                        padding: const EdgeInsets.all(16),
                      ),
                    ),
                  ),
                  const SizedBox(height: 8),
                  SizedBox(
                    width: double.infinity,
                    child: OutlinedButton.icon(
                      onPressed: () async {
                        await bluetoothService.disconnect();
                      },
                      icon: const Icon(Icons.bluetooth_disabled),
                      label: const Text('Desconectar'),
                      style: OutlinedButton.styleFrom(
                        foregroundColor: Colors.red.shade700,
                        padding: const EdgeInsets.all(16),
                      ),
                    ),
                  ),
                  const SizedBox(height: 8),
                  SizedBox(
                    width: double.infinity,
                    child: TextButton.icon(
                      onPressed: () async {
                        final messenger = ScaffoldMessenger.of(context);
                        await bluetoothService.clearSavedDevice();
                        if (!mounted) return;
                        messenger.showSnackBar(
                          const SnackBar(
                            content: Text('Dispositivo esquecido'),
                          ),
                        );
                      },
                      icon: const Icon(Icons.delete_outline),
                      label: const Text('Esquecer Dispositivo'),
                    ),
                  ),
                ],
              ),
            )
          else
            Padding(
              padding: const EdgeInsets.all(16),
              child: SizedBox(
                width: double.infinity,
                child: ElevatedButton.icon(
                  onPressed: _isScanning ? null : _startScan,
                  icon: _isScanning
                      ? const SizedBox(
                          width: 20,
                          height: 20,
                          child: CircularProgressIndicator(
                            strokeWidth: 2,
                            color: Colors.white,
                          ),
                        )
                      : const Icon(Icons.search),
                  label: Text(
                    _isScanning ? 'Procurando...' : 'Procurar Dispositivos',
                  ),
                  style: ElevatedButton.styleFrom(
                    backgroundColor: Colors.blue.shade700,
                    foregroundColor: Colors.white,
                    padding: const EdgeInsets.all(16),
                  ),
                ),
              ),
            ),

          const Divider(),

          // Lista de dispositivos
          Expanded(
            child: StreamBuilder<List<fbp.ScanResult>>(
              stream: fbp.FlutterBluePlus.scanResults,
              builder: (context, snapshot) {
                if (!snapshot.hasData || snapshot.data!.isEmpty) {
                  return Center(
                    child: Column(
                      mainAxisAlignment: MainAxisAlignment.center,
                      children: [
                        Icon(
                          Icons.bluetooth_searching,
                          size: 64,
                          color: Colors.grey.shade400,
                        ),
                        const SizedBox(height: 16),
                        Text(
                          _isScanning
                              ? 'Procurando dispositivos...'
                              : 'Nenhum dispositivo encontrado',
                          style: TextStyle(
                            fontSize: 16,
                            color: Colors.grey.shade600,
                          ),
                        ),
                        const SizedBox(height: 8),
                        if (!_isScanning)
                          TextButton.icon(
                            onPressed: _startScan,
                            icon: const Icon(Icons.refresh),
                            label: const Text('Procurar'),
                          ),
                      ],
                    ),
                  );
                }

                return ListView.builder(
                  itemCount: snapshot.data!.length,
                  itemBuilder: (context, index) {
                    final result = snapshot.data![index];
                    final device = result.device;
                    final rssi = result.rssi;

                    return Card(
                      margin: const EdgeInsets.symmetric(
                        horizontal: 16,
                        vertical: 4,
                      ),
                      child: ListTile(
                        leading: Icon(
                          Icons.bluetooth,
                          color: Colors.blue.shade700,
                          size: 32,
                        ),
                        title: Text(
                          device.platformName.isEmpty
                              ? 'Dispositivo Desconhecido'
                              : device.platformName,
                          style: const TextStyle(fontWeight: FontWeight.bold),
                        ),
                        subtitle: Column(
                          crossAxisAlignment: CrossAxisAlignment.start,
                          children: [
                            Text('ID: ${device.remoteId}'),
                            Text('Sinal: $rssi dBm'),
                          ],
                        ),
                        trailing:
                            bluetoothService.isConnecting &&
                                bluetoothService.device?.remoteId ==
                                    device.remoteId
                            ? const SizedBox(
                                width: 24,
                                height: 24,
                                child: CircularProgressIndicator(
                                  strokeWidth: 2,
                                ),
                              )
                            : const Icon(Icons.chevron_right),
                        onTap: bluetoothService.isConnecting
                            ? null
                            : () async {
                                final messenger = ScaffoldMessenger.of(context);
                                await fbp.FlutterBluePlus.stopScan();
                                setState(() => _isScanning = false);

                                final success = await bluetoothService.connect(
                                  device,
                                );
                                if (!mounted) return;
                                if (success) {
                                  messenger.showSnackBar(
                                    SnackBar(
                                      content: Text(
                                        'Conectado a ${device.platformName}',
                                      ),
                                      backgroundColor: Colors.green,
                                    ),
                                  );
                                } else {
                                  messenger.showSnackBar(
                                    const SnackBar(
                                      content: Text('Falha ao conectar'),
                                      backgroundColor: Colors.red,
                                    ),
                                  );
                                }
                              },
                      ),
                    );
                  },
                );
              },
            ),
          ),
        ],
      ),
    );
  }
}
