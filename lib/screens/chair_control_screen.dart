import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../services/bluetooth_service.dart';
import '../services/settings_service.dart';
import '../widgets/control_button.dart';
import '../widgets/chair_visualization.dart';

class ChairControlScreen extends StatelessWidget {
  const ChairControlScreen({super.key});

  void _showBluetoothWarning(BuildContext context) {
    showDialog(
      context: context,
      builder: (context) => AlertDialog(
        title: Row(
          children: [
            Icon(Icons.bluetooth_disabled, color: Colors.orange.shade700),
            const SizedBox(width: 12),
            const Text('Bluetooth Desconectado'),
          ],
        ),
        content: const Text(
          'Para controlar a cadeira, é necessário conectar via Bluetooth primeiro.\n\n'
          'Deseja ir para a tela de conexão?',
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: const Text('CANCELAR'),
          ),
          ElevatedButton.icon(
            onPressed: () {
              Navigator.pop(context);
              Navigator.pushNamed(context, '/bluetooth');
            },
            icon: const Icon(Icons.bluetooth_searching),
            label: const Text('CONECTAR'),
            style: ElevatedButton.styleFrom(
              backgroundColor: Colors.blue.shade700,
              foregroundColor: Colors.white,
            ),
          ),
        ],
      ),
    );
  }

  void _executeCommand(
    BuildContext context,
    BluetoothService bluetoothService,
    VoidCallback command,
  ) {
    if (!bluetoothService.isConnected) {
      _showBluetoothWarning(context);
    } else {
      command();
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Controle da Cadeira'),
        backgroundColor: Colors.blue.shade700,
        foregroundColor: Colors.white,
        actions: [
          Consumer<BluetoothService>(
            builder: (context, bluetoothService, child) {
              return IconButton(
                onPressed: () async {
                  await bluetoothService.stopAll();
                  if (context.mounted) {
                    ScaffoldMessenger.of(context).showSnackBar(
                      const SnackBar(
                        content: Text(
                          'Todos os movimentos foram interrompidos',
                        ),
                        backgroundColor: Colors.orange,
                      ),
                    );
                  }
                },
                icon: const Icon(Icons.stop_circle),
                tooltip: 'Parar Todos os Movimentos',
              );
            },
          ),
          IconButton(
            onPressed: () => Navigator.pushNamed(context, '/settings'),
            icon: const Icon(Icons.settings),
            tooltip: 'Configurações',
          ),
          IconButton(
            onPressed: () => Navigator.pushNamed(context, '/bluetooth'),
            icon: const Icon(Icons.bluetooth),
            tooltip: 'Conexão Bluetooth',
          ),
        ],
      ),
      body: Consumer2<BluetoothService, SettingsService>(
        builder: (context, bluetoothService, settingsService, child) {
          return SingleChildScrollView(
            padding: const EdgeInsets.all(16.0),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                // Status da Conexão Bluetooth
                Card(
                  color: bluetoothService.isConnected
                      ? Colors.green.shade50
                      : Colors.orange.shade50,
                  elevation: 4,
                  child: Padding(
                    padding: const EdgeInsets.all(16.0),
                    child: Row(
                      children: [
                        Icon(
                          bluetoothService.isConnected
                              ? Icons.bluetooth_connected
                              : Icons.bluetooth_disabled,
                          color: bluetoothService.isConnected
                              ? Colors.green.shade700
                              : Colors.orange.shade700,
                          size: 28,
                        ),
                        const SizedBox(width: 12),
                        Expanded(
                          child: Column(
                            crossAxisAlignment: CrossAxisAlignment.start,
                            children: [
                              Text(
                                bluetoothService.isConnected
                                    ? 'Conectado via Bluetooth'
                                    : 'Modo Simulação',
                                style: TextStyle(
                                  fontSize: 16,
                                  fontWeight: FontWeight.bold,
                                  color: bluetoothService.isConnected
                                      ? Colors.green.shade700
                                      : Colors.orange.shade700,
                                ),
                              ),
                              Text(
                                bluetoothService.isConnected
                                    ? 'Cadeira conectada e pronta'
                                    : 'Funcionamento sem hardware',
                                style: TextStyle(
                                  fontSize: 12,
                                  color: Colors.grey.shade700,
                                ),
                              ),
                            ],
                          ),
                        ),
                        if (!bluetoothService.isConnected)
                          ElevatedButton.icon(
                            onPressed: () =>
                                Navigator.pushNamed(context, '/bluetooth'),
                            icon: const Icon(Icons.bluetooth_searching),
                            label: const Text('Conectar'),
                            style: ElevatedButton.styleFrom(
                              backgroundColor: Colors.blue.shade700,
                              foregroundColor: Colors.white,
                            ),
                          ),
                      ],
                    ),
                  ),
                ),
                const SizedBox(height: 12),
                
                // Alerta de Manutenção
                if (settingsService.settings.needsMaintenance())
                  Card(
                    color: Colors.red.shade50,
                    elevation: 4,
                    child: Padding(
                      padding: const EdgeInsets.all(16.0),
                      child: Row(
                        children: [
                          Icon(
                            Icons.warning,
                            color: Colors.red.shade700,
                            size: 32,
                          ),
                          const SizedBox(width: 12),
                          Expanded(
                            child: Column(
                              crossAxisAlignment: CrossAxisAlignment.start,
                              children: [
                                Text(
                                  'MANUTENÇÃO NECESSÁRIA',
                                  style: TextStyle(
                                    fontSize: 16,
                                    fontWeight: FontWeight.bold,
                                    color: Colors.red.shade700,
                                  ),
                                ),
                                Text(
                                  '${settingsService.settings.hoursSinceMaintenance.toStringAsFixed(1)}h de uso',
                                  style: TextStyle(
                                    fontSize: 14,
                                    color: Colors.red.shade600,
                                  ),
                                ),
                              ],
                            ),
                          ),
                          IconButton(
                            icon: const Icon(Icons.settings),
                            onPressed: () =>
                                Navigator.pushNamed(context, '/settings'),
                            tooltip: 'Ver Configurações',
                          ),
                        ],
                      ),
                    ),
                  )
                else if (settingsService.settings.nearMaintenance())
                  Card(
                    color: Colors.orange.shade50,
                    elevation: 4,
                    child: Padding(
                      padding: const EdgeInsets.all(16.0),
                      child: Row(
                        children: [
                          Icon(
                            Icons.info,
                            color: Colors.orange.shade700,
                            size: 32,
                          ),
                          const SizedBox(width: 12),
                          Expanded(
                            child: Column(
                              crossAxisAlignment: CrossAxisAlignment.start,
                              children: [
                                Text(
                                  'Manutenção em breve',
                                  style: TextStyle(
                                    fontSize: 16,
                                    fontWeight: FontWeight.bold,
                                    color: Colors.orange.shade700,
                                  ),
                                ),
                                Text(
                                  '${settingsService.settings.hoursSinceMaintenance.toStringAsFixed(1)}h de uso',
                                  style: TextStyle(
                                    fontSize: 14,
                                    color: Colors.orange.shade600,
                                  ),
                                ),
                              ],
                            ),
                          ),
                        ],
                      ),
                    ),
                  ),
                if (settingsService.settings.needsMaintenance() ||
                    settingsService.settings.nearMaintenance())
                  const SizedBox(height: 12),

                // Visualização da Cadeira + Controles
                Card(
                  elevation: 4,
                  child: Padding(
                    padding: const EdgeInsets.all(16.0),
                    child: Column(
                      children: [
                        const Text(
                          'Visualização e Controle',
                          style: TextStyle(
                            fontSize: 18,
                            fontWeight: FontWeight.bold,
                          ),
                        ),
                        const SizedBox(height: 16),
                        // Layout com cadeira e botões lado a lado
                        LayoutBuilder(
                          builder: (context, constraints) {
                            return Row(
                              crossAxisAlignment: CrossAxisAlignment.start,
                              children: [
                                // Visualização da Cadeira
                                Expanded(
                                  flex: 2,
                                  child: Center(
                                    child: ChairVisualization(
                                      chairState: bluetoothService.chairState,
                                    ),
                                  ),
                                ),
                                const SizedBox(width: 16),
                                // Controles ao lado
                                Expanded(
                                  flex: 1,
                                  child: Column(
                                    mainAxisSize: MainAxisSize.min,
                                    children: [
                                      // Refletor
                                      ControlButton(
                                        label: 'Refletor',
                                        icon: Icons.lightbulb,
                                        onPressed: () => _executeCommand(
                                          context,
                                          bluetoothService,
                                          () => bluetoothService.toggleReflector(),
                                        ),
                                        color: Colors.yellow.shade700,
                                        isActive: bluetoothService
                                            .chairState
                                            .reflectorOn,
                                      ),
                                      const SizedBox(height: 8),
                                      // Encosto
                                      const Text(
                                        'Encosto',
                                        style: TextStyle(
                                          fontSize: 12,
                                          fontWeight: FontWeight.bold,
                                        ),
                                      ),
                                      const SizedBox(height: 4),
                                      Row(
                                        children: [
                                          Expanded(
                                            child: ControlButton(
                                              label: '↑',
                                              icon: Icons.arrow_upward,
                                              onPressed: () => _executeCommand(
                                                context,
                                                bluetoothService,
                                                () => bluetoothService.moveBackUp(),
                                              ),
                                              color: Colors.blue.shade600,
                                              isActive: bluetoothService
                                                  .chairState
                                                  .backUpOn,
                                            ),
                                          ),
                                          const SizedBox(width: 4),
                                          Expanded(
                                            child: ControlButton(
                                              label: '↓',
                                              icon: Icons.arrow_downward,
                                              onPressed: () => _executeCommand(
                                                context,
                                                bluetoothService,
                                                () => bluetoothService.moveBackDown(),
                                              ),
                                              color: Colors.orange.shade600,
                                              isActive: bluetoothService
                                                  .chairState
                                                  .backDownOn,
                                            ),
                                          ),
                                        ],
                                      ),
                                      const SizedBox(height: 8),
                                      // Assento
                                      const Text(
                                        'Assento',
                                        style: TextStyle(
                                          fontSize: 12,
                                          fontWeight: FontWeight.bold,
                                        ),
                                      ),
                                      const SizedBox(height: 4),
                                      Row(
                                        children: [
                                          Expanded(
                                            child: ControlButton(
                                              label: '⬆',
                                              icon: Icons.arrow_upward,
                                              onPressed: () => _executeCommand(
                                                context,
                                                bluetoothService,
                                                () => bluetoothService.moveSeatUp(),
                                              ),
                                              color: Colors.blue.shade700,
                                              isActive: bluetoothService
                                                  .chairState
                                                  .seatUpOn,
                                            ),
                                          ),
                                          const SizedBox(width: 4),
                                          Expanded(
                                            child: ControlButton(
                                              label: '⬇',
                                              icon: Icons.arrow_downward,
                                              onPressed: () => _executeCommand(
                                                context,
                                                bluetoothService,
                                                () => bluetoothService.moveSeatDown(),
                                              ),
                                              color: Colors.orange.shade700,
                                              isActive: bluetoothService
                                                  .chairState
                                                  .seatDownOn,
                                            ),
                                          ),
                                        ],
                                      ),
                                      const SizedBox(height: 8),
                                      // Pernas
                                      const Text(
                                        'Pernas',
                                        style: TextStyle(
                                          fontSize: 12,
                                          fontWeight: FontWeight.bold,
                                        ),
                                      ),
                                      const SizedBox(height: 4),
                                      Row(
                                        children: [
                                          Expanded(
                                            child: ControlButton(
                                              label: '⬆',
                                              icon: Icons.arrow_upward,
                                              onPressed: () => _executeCommand(
                                                context,
                                                bluetoothService,
                                                () => bluetoothService.toggleUpperLegs(),
                                              ),
                                              color: Colors.green.shade600,
                                              isActive: bluetoothService
                                                  .chairState
                                                  .upperLegsOn,
                                            ),
                                          ),
                                          const SizedBox(width: 4),
                                          Expanded(
                                            child: ControlButton(
                                              label: '⬇',
                                              icon: Icons.arrow_downward,
                                              onPressed: () => _executeCommand(
                                                context,
                                                bluetoothService,
                                                () => bluetoothService.toggleLowerLegs(),
                                              ),
                                              color: Colors.red.shade600,
                                              isActive: bluetoothService
                                                  .chairState
                                                  .lowerLegsOn,
                                            ),
                                          ),
                                        ],
                                      ),
                                      const SizedBox(height: 8),
                                      // Emergência
                                      ControlButton(
                                        label: 'PARAR',
                                        icon: Icons.cancel,
                                        onPressed: () => _executeCommand(
                                          context,
                                          bluetoothService,
                                          () => bluetoothService.stopAll(),
                                        ),
                                        color: Colors.red.shade800,
                                      ),
                                    ],
                                  ),
                                ),
                              ],
                            );
                          },
                        ),
                      ],
                    ),
                  ),
                ),
                const SizedBox(height: 20),

                // Posições Pré-definidas
                const Text(
                  'Posições Pré-definidas',
                  style: TextStyle(fontSize: 20, fontWeight: FontWeight.bold),
                ),
                const SizedBox(height: 10),
                Row(
                  children: [
                    Expanded(
                      child: ControlButton(
                        label: 'Posição\nGinecológica',
                        icon: Icons.airline_seat_recline_extra,
                        onPressed: () => _executeCommand(
                          context,
                          bluetoothService,
                          () => bluetoothService.setGynecologicalPosition(),
                        ),
                        color: Colors.purple.shade600,
                      ),
                    ),
                    const SizedBox(width: 10),
                    Expanded(
                      child: ControlButton(
                        label: 'Posição\nde Parto',
                        icon: Icons.airline_seat_recline_normal,
                        onPressed: () => _executeCommand(
                          context,
                          bluetoothService,
                          () => bluetoothService.setBirthPosition(),
                        ),
                        color: Colors.pink.shade600,
                      ),
                    ),
                  ],
                ),
              ],
            ),
          );
        },
      ),
    );
  }
}
