import 'dart:async';
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'package:vibration/vibration.dart';
import '../services/bluetooth_service.dart';
import '../services/mqtt_service.dart';
import '../services/settings_service.dart';
import '../services/supabase_service.dart';
import '../widgets/chair_visualization.dart';
import '../widgets/encoder_status_widget.dart';

class ChairControlScreen extends StatefulWidget {
  const ChairControlScreen({super.key});

  @override
  State<ChairControlScreen> createState() => _ChairControlScreenState();
}

class _ChairControlScreenState extends State<ChairControlScreen> {
  bool _isCheckingStatus = true;
  bool _chairEnabled = true;
  bool _maintenanceRequired = false;
  double _maintenanceHours = 0;

  // Flag para controlar se já atualizamos o ID da cadeira após conectar
  bool _chairIdUpdated = false;

  BluetoothService? _bluetoothService;
  bool _lastBackUpLimit = false;
  bool _lastBackDownLimit = false;
  bool _lastSeatUpLimit = false;
  bool _lastSeatDownLimit = false;
  bool _lastLegUpLimit = false;
  bool _lastLegDownLimit = false;

  @override
  void initState() {
    super.initState();
    _checkChairStatus();
    WidgetsBinding.instance.addPostFrameCallback((_) {
      if (!mounted) return;
      _attachBluetoothListener();
      _initMqtt();
    });
  }

  @override
  void dispose() {
    _bluetoothService?.removeListener(_onBluetoothChanged);
    super.dispose();
  }

  void _attachBluetoothListener() {
    _bluetoothService?.removeListener(_onBluetoothChanged);

    final bluetooth = context.read<BluetoothService>();
    _bluetoothService = bluetooth;
    _syncLastLimitSnapshot(bluetooth);
    bluetooth.addListener(_onBluetoothChanged);
  }

  void _ensureBluetoothListener(BluetoothService bluetooth) {
    if (identical(_bluetoothService, bluetooth)) return;
    _bluetoothService?.removeListener(_onBluetoothChanged);
    _bluetoothService = bluetooth;
    _syncLastLimitSnapshot(bluetooth);
    bluetooth.addListener(_onBluetoothChanged);
  }

  void _syncLastLimitSnapshot(BluetoothService bluetooth) {
    final state = bluetooth.chairState;
    _lastBackUpLimit = state.backUpLimit;
    _lastBackDownLimit = state.backDownLimit;
    _lastSeatUpLimit = state.seatUpLimit;
    _lastSeatDownLimit = state.seatDownLimit;
    _lastLegUpLimit = state.legUpLimit;
    _lastLegDownLimit = state.legDownLimit;
  }

  void _onBluetoothChanged() {
    if (!mounted) return;
    final bluetooth = _bluetoothService;
    if (bluetooth == null) return;

    final state = bluetooth.chairState;

    String? message;
    if ((!_lastBackUpLimit && state.backUpLimit) ||
        (!_lastBackDownLimit && state.backDownLimit)) {
      message = 'Limite do encosto atingido';
    } else if ((!_lastSeatUpLimit && state.seatUpLimit) ||
        (!_lastSeatDownLimit && state.seatDownLimit)) {
      message = 'Limite do assento atingido';
    } else if ((!_lastLegUpLimit && state.legUpLimit) ||
        (!_lastLegDownLimit && state.legDownLimit)) {
      message = 'Limite da perneira atingido';
    }

    _lastBackUpLimit = state.backUpLimit;
    _lastBackDownLimit = state.backDownLimit;
    _lastSeatUpLimit = state.seatUpLimit;
    _lastSeatDownLimit = state.seatDownLimit;
    _lastLegUpLimit = state.legUpLimit;
    _lastLegDownLimit = state.legDownLimit;

    if (message != null) {
      debugPrint('🚨 $message');
      _notifyLimitReached(message);
    }
  }

  Future<void> _notifyLimitReached(String message) async {
    debugPrint('🚨 $message');
    try {
      // Vibra imediatamente
      await Vibration.vibrate(duration: 200);
    } catch (_) {}

    if (!mounted) return;
    final messenger = ScaffoldMessenger.of(context);
    messenger.removeCurrentSnackBar();
    messenger.showSnackBar(
      SnackBar(
        content: Row(
          children: [
            const Icon(Icons.warning_amber, color: Colors.white),
            const SizedBox(width: 8),
            Expanded(child: Text('⚠️ $message')),
          ],
        ),
        backgroundColor: Colors.orange.shade700,
        duration: const Duration(seconds: 2),
        behavior: SnackBarBehavior.floating,
      ),
    );
  }

  @override
  void didChangeDependencies() {
    super.didChangeDependencies();
    // Verifica se conectou via BLE e ainda não atualizou o ID
    final bluetooth = context.read<BluetoothService>();
    if (bluetooth.isConnected &&
        !_chairIdUpdated &&
        (SupabaseService.instance.chairSerialNumber == null ||
            SupabaseService.instance.chairSerialNumber ==
                'CADEIRA-DESCONHECIDA')) {
      debugPrint('🔄 BLE conectado, atualizando ID da cadeira...');
      _chairIdUpdated = true;
      _checkChairStatus();
    }
  }

  Future<void> _checkChairStatus() async {
    final supabase = SupabaseService.instance;
    final bluetooth = context.read<BluetoothService>();
    final settingsService = context.read<SettingsService>();

    // Define o número de série da cadeira usando o MAC address do dispositivo
    final chairId = bluetooth.chairUniqueId;
    final settingsSerial = settingsService.settings.chairSerial.trim();

    String normalizedSerial(String raw) {
      final v = raw.trim();
      if (v.isEmpty) return 'CADEIRA-DESCONHECIDA';
      final up = v.toUpperCase();
      if (up.startsWith('CADEIRA-')) return up;
      return 'CADEIRA-$up';
    }

    final serialToUse = chairId != null && chairId.trim().isNotEmpty
        ? chairId.trim().toUpperCase()
        : normalizedSerial(settingsSerial);

    supabase.setChairSerialNumber(serialToUse);

    if (chairId != null && chairId.trim().isNotEmpty) {
      if (settingsSerial.toUpperCase() != chairId.trim().toUpperCase()) {
        await settingsService.setChairSerial(chairId.trim().toUpperCase());
      }
      debugPrint('📋 Cadeira registrada: $serialToUse');
    } else {
      debugPrint('📋 Cadeira registrada (config): $serialToUse');
    }

    setState(() => _isCheckingStatus = true);

    try {
      await supabase.checkChairStatus();
      if (!mounted) return;

      setState(() {
        _chairEnabled = supabase.isChairEnabled;
        _maintenanceRequired = supabase.isMaintenanceRequired;
        _maintenanceHours = supabase.maintenanceHours;
        _isCheckingStatus = false;
      });

      // Envia o horímetro atual
      if (!mounted) return;
      await supabase.sendHourMeter(
        settingsService.settings.hoursSinceMaintenance,
      );
    } catch (e) {
      if (!mounted) return;
      setState(() {
        _isCheckingStatus = false;
        // Em caso de erro, permite uso (modo offline)
        _chairEnabled = true;
      });
    }
  }

  void _showChairBlockedDialog() {
    showDialog(
      context: context,
      barrierDismissible: false,
      builder: (context) => AlertDialog(
        title: Row(
          children: [
            Icon(Icons.block, color: Colors.red.shade700, size: 32),
            const SizedBox(width: 12),
            const Expanded(
              child: Text('Cadeira Bloqueada', style: TextStyle(fontSize: 18)),
            ),
          ],
        ),
        content: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            const Text(
              'Esta cadeira está temporariamente bloqueada.',
              style: TextStyle(fontSize: 16),
            ),
            const SizedBox(height: 16),
            Container(
              padding: const EdgeInsets.all(12),
              decoration: BoxDecoration(
                color: Colors.red.shade50,
                borderRadius: BorderRadius.circular(8),
                border: Border.all(color: Colors.red.shade200),
              ),
              child: const Row(
                children: [
                  Icon(Icons.info_outline, color: Colors.red),
                  SizedBox(width: 8),
                  Expanded(
                    child: Text(
                      'Entre em contato com o suporte técnico para liberar o uso.',
                      style: TextStyle(color: Colors.red),
                    ),
                  ),
                ],
              ),
            ),
          ],
        ),
        actions: [
          TextButton(
            onPressed: () {
              Navigator.pop(context);
              _checkChairStatus(); // Tenta verificar novamente
            },
            child: const Text('TENTAR NOVAMENTE'),
          ),
        ],
      ),
    );
  }

  void _showMaintenanceWarningDialog() {
    showDialog(
      context: context,
      builder: (context) => AlertDialog(
        title: Row(
          children: [
            Icon(Icons.build, color: Colors.orange.shade700, size: 32),
            const SizedBox(width: 12),
            const Expanded(
              child: Text(
                'Manutenção Necessária',
                style: TextStyle(fontSize: 18),
              ),
            ),
          ],
        ),
        content: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(
              'Esta cadeira precisa de manutenção após $_maintenanceHours horas de uso.',
              style: const TextStyle(fontSize: 16),
            ),
            const SizedBox(height: 16),
            Container(
              padding: const EdgeInsets.all(12),
              decoration: BoxDecoration(
                color: Colors.orange.shade50,
                borderRadius: BorderRadius.circular(8),
                border: Border.all(color: Colors.orange.shade200),
              ),
              child: Row(
                children: [
                  Icon(Icons.warning, color: Colors.orange.shade700),
                  const SizedBox(width: 8),
                  const Expanded(
                    child: Text(
                      'Agende uma manutenção preventiva o mais breve possível.',
                      style: TextStyle(color: Colors.orange),
                    ),
                  ),
                ],
              ),
            ),
          ],
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: const Text('ENTENDI'),
          ),
          ElevatedButton.icon(
            onPressed: () {
              Navigator.pop(context);
              _requestMaintenance();
            },
            icon: const Icon(Icons.calendar_today),
            label: const Text('SOLICITAR MANUTENÇÃO'),
            style: ElevatedButton.styleFrom(
              backgroundColor: Colors.orange.shade700,
              foregroundColor: Colors.white,
            ),
          ),
        ],
      ),
    );
  }

  Future<void> _requestMaintenance() async {
    final supabase = SupabaseService.instance;
    final settingsService = context.read<SettingsService>();

    final success = await supabase.requestMaintenance(
      description: 'Manutenção preventiva solicitada pelo app',
      currentHours: settingsService.settings.hoursSinceMaintenance,
    );

    if (mounted) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text(
            success
                ? 'Pedido de manutenção enviado com sucesso!'
                : 'Erro ao enviar pedido. Tente novamente.',
          ),
          backgroundColor: success ? Colors.green : Colors.red,
        ),
      );
    }
  }

  void _showBluetoothWarning(BuildContext context) {
    showDialog(
      context: context,
      builder: (context) => AlertDialog(
        title: Row(
          children: [
            Icon(Icons.bluetooth_disabled, color: Colors.orange.shade700),
            const SizedBox(width: 12),
            const Expanded(child: Text('Bluetooth Desconectado')),
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
    // Verifica se a cadeira está habilitada
    if (!_chairEnabled) {
      _showChairBlockedDialog();
      return;
    }

    if (!bluetoothService.isConnected) {
      _showBluetoothWarning(context);
    } else {
      command();
    }
  }

  void _initMqtt() {
    try {
      final mqtt = context.read<MqttService>();
      if (mqtt.isConnected || mqtt.isConnecting) return;
      mqtt.connect().catchError((_) {});
    } catch (_) {}
  }

  // Botão estilizado no mesmo formato das áreas de controle da cadeira
  Widget _buildStyledButton({
    required String label,
    required IconData icon,
    required Color color,
    required VoidCallback onPressed,
    bool isActive = false,
    double height = 70,
  }) {
    const lineColor = Color(0xFF1565C0);
    const accentColor = Color(0xFF0D47A1);

    return _Pressable3D(
      onTap: onPressed,
      child: Container(
        height: height,
        decoration: BoxDecoration(
          color: isActive ? color.withValues(alpha: 0.15) : Colors.transparent,
          borderRadius: BorderRadius.circular(12),
          border: Border.all(
            color: isActive
                ? color.withValues(alpha: 0.5)
                : lineColor.withValues(alpha: 0.2),
            width: isActive ? 2 : 1,
          ),
          boxShadow: [
            BoxShadow(
              color: Colors.black.withValues(alpha: 0.10),
              blurRadius: 14,
              offset: const Offset(0, 8),
            ),
          ],
        ),
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            Icon(
              icon,
              size: 22,
              color: isActive ? color : accentColor.withValues(alpha: 0.6),
            ),
            const SizedBox(height: 4),
            Text(
              label,
              textAlign: TextAlign.center,
              style: TextStyle(
                color: isActive ? color : accentColor.withValues(alpha: 0.7),
                fontSize: 9,
                fontWeight: FontWeight.bold,
                height: 1.1,
              ),
            ),
          ],
        ),
      ),
    );
  }

  // Card de Status da Cadeira
  Widget _buildStatusCard(
    BluetoothService bluetoothService,
    SettingsService settingsService,
  ) {
    final isConnected = bluetoothService.isConnected;
    final needsMaintenance = settingsService.settings.needsMaintenance();
    final hoursUsed = settingsService.settings.hoursSinceMaintenance;
    final supabase = SupabaseService.instance;
    final isDbConnected = supabase.isInitialized && supabase.isConnected;
    final mqtt = context.watch<MqttService>();
    final mqttIsConnected = mqtt.isConnected;
    final mqttIsConnecting = mqtt.isConnecting;
    final mqttError = mqtt.lastError;

    // Determina o status geral
    String statusText;
    Color statusColor;
    IconData statusIcon;

    if (!_chairEnabled) {
      statusText = 'BLOQUEADA';
      statusColor = Colors.red;
      statusIcon = Icons.block;
    } else if (_maintenanceRequired || needsMaintenance) {
      statusText = 'MANUTENÇÃO';
      statusColor = Colors.orange;
      statusIcon = Icons.build;
    } else {
      statusText = 'OPERACIONAL';
      statusColor = Colors.green;
      statusIcon = Icons.check_circle;
    }

    return Card(
      elevation: 4,
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
      child: Padding(
        padding: const EdgeInsets.all(16.0),
        child: Column(
          children: [
            // Status principal
            Row(
              children: [
                Container(
                  padding: const EdgeInsets.all(12),
                  decoration: BoxDecoration(
                    color: statusColor.withValues(alpha: 0.1),
                    borderRadius: BorderRadius.circular(12),
                  ),
                  child: Icon(statusIcon, color: statusColor, size: 32),
                ),
                const SizedBox(width: 16),
                Expanded(
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      const Text(
                        'Status da Cadeira',
                        style: TextStyle(fontSize: 12, color: Colors.grey),
                      ),
                      Text(
                        statusText,
                        style: TextStyle(
                          fontSize: 20,
                          fontWeight: FontWeight.bold,
                          color: statusColor,
                        ),
                      ),
                    ],
                  ),
                ),
                // Botão atualizar
                IconButton(
                  onPressed: _checkChairStatus,
                  icon: const Icon(Icons.refresh),
                  tooltip: 'Atualizar status',
                ),
              ],
            ),
            const Divider(height: 24),
            // Indicadores de status - Linha 1
            Row(
              children: [
                // Bluetooth
                Expanded(
                  child: _buildStatusIndicator(
                    icon: isConnected
                        ? Icons.bluetooth_connected
                        : Icons.bluetooth_disabled,
                    label: 'Bluetooth',
                    value: isConnected ? 'Conectado' : 'Desconectado',
                    color: isConnected ? Colors.blue : Colors.grey,
                    isActive: isConnected,
                  ),
                ),
                const SizedBox(width: 8),
                // Banco de Dados
                Expanded(
                  child: _buildStatusIndicator(
                    icon: isDbConnected ? Icons.cloud_done : Icons.cloud_off,
                    label: 'Servidor',
                    value: isDbConnected ? 'Online' : 'Offline',
                    color: isDbConnected ? Colors.green : Colors.grey,
                    isActive: isDbConnected,
                  ),
                ),
              ],
            ),
            const SizedBox(height: 8),
            Row(
              children: [
                Expanded(
                  child: _buildStatusIndicator(
                    icon: mqttIsConnected
                        ? Icons.wifi_tethering
                        : (mqttIsConnecting
                              ? Icons.wifi_tethering_off
                              : Icons.wifi_off),
                    label: 'MQTT (RX)',
                    value: mqttIsConnected
                        ? 'Conectado'
                        : (mqttIsConnecting ? 'Conectando' : 'Desconectado'),
                    color: mqttIsConnected
                        ? Colors.teal
                        : (mqttIsConnecting ? Colors.orange : Colors.grey),
                    isActive: mqttIsConnected,
                  ),
                ),
              ],
            ),
            if (!mqttIsConnected &&
                !mqttIsConnecting &&
                mqttError != null &&
                mqttError.trim().isNotEmpty) ...[
              const SizedBox(height: 6),
              Align(
                alignment: Alignment.centerLeft,
                child: Text(
                  'Erro MQTT: $mqttError',
                  style: TextStyle(
                    fontSize: 11,
                    color: Colors.red.shade700,
                    fontWeight: FontWeight.w600,
                  ),
                ),
              ),
            ],
            const SizedBox(height: 8),
            // Indicadores de status - Linha 2
            Row(
              children: [
                // Horímetro
                Expanded(
                  child: _buildStatusIndicator(
                    icon: Icons.timer,
                    label: 'Horímetro',
                    value: '${hoursUsed.toStringAsFixed(1)}h',
                    color: needsMaintenance ? Colors.orange : Colors.blue,
                    isActive: true,
                  ),
                ),
                const SizedBox(width: 8),
                // Manutenção
                Expanded(
                  child: _buildStatusIndicator(
                    icon: Icons.build,
                    label: 'Manutenção',
                    value: needsMaintenance ? 'Necessária' : 'OK',
                    color: needsMaintenance ? Colors.orange : Colors.green,
                    isActive: !needsMaintenance,
                  ),
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildStatusIndicator({
    required IconData icon,
    required String label,
    required String value,
    required Color color,
    required bool isActive,
  }) {
    return Container(
      padding: const EdgeInsets.symmetric(vertical: 8, horizontal: 4),
      decoration: BoxDecoration(
        color: color.withValues(alpha: 0.1),
        borderRadius: BorderRadius.circular(8),
      ),
      child: Column(
        children: [
          Icon(icon, color: color, size: 20),
          const SizedBox(height: 4),
          Text(label, style: const TextStyle(fontSize: 10, color: Colors.grey)),
          Text(
            value,
            style: TextStyle(
              fontSize: 11,
              fontWeight: FontWeight.bold,
              color: color,
            ),
          ),
        ],
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    // Mostra loading enquanto verifica status
    if (_isCheckingStatus) {
      return Scaffold(
        appBar: AppBar(
          title: const Text('Controle da Cadeira'),
          backgroundColor: Colors.blue.shade700,
          foregroundColor: Colors.white,
        ),
        body: const Center(
          child: Column(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              CircularProgressIndicator(),
              SizedBox(height: 16),
              Text('Verificando status da cadeira...'),
            ],
          ),
        ),
      );
    }

    // Mostra tela de bloqueio se cadeira não estiver habilitada
    if (!_chairEnabled) {
      return Scaffold(
        appBar: AppBar(
          title: const Text('Controle da Cadeira'),
          backgroundColor: Colors.red.shade700,
          foregroundColor: Colors.white,
        ),
        body: Center(
          child: Padding(
            padding: const EdgeInsets.all(32.0),
            child: Column(
              mainAxisAlignment: MainAxisAlignment.center,
              children: [
                Icon(Icons.block, size: 100, color: Colors.red.shade300),
                const SizedBox(height: 24),
                const Text(
                  'CADEIRA BLOQUEADA',
                  style: TextStyle(
                    fontSize: 24,
                    fontWeight: FontWeight.bold,
                    color: Colors.red,
                  ),
                ),
                const SizedBox(height: 16),
                const Text(
                  'Esta cadeira está temporariamente bloqueada.\nEntre em contato com o suporte técnico.',
                  textAlign: TextAlign.center,
                  style: TextStyle(fontSize: 16),
                ),
                const SizedBox(height: 32),
                ElevatedButton.icon(
                  onPressed: _checkChairStatus,
                  icon: const Icon(Icons.refresh),
                  label: const Text('VERIFICAR NOVAMENTE'),
                  style: ElevatedButton.styleFrom(
                    backgroundColor: Colors.blue.shade700,
                    foregroundColor: Colors.white,
                    padding: const EdgeInsets.symmetric(
                      horizontal: 24,
                      vertical: 12,
                    ),
                  ),
                ),
              ],
            ),
          ),
        ),
      );
    }

    return Scaffold(
      appBar: AppBar(
        title: const Text('Controle da Cadeira'),
        backgroundColor: Colors.blue.shade700,
        foregroundColor: Colors.white,
        actions: [
          // Indicador de manutenção necessária
          if (_maintenanceRequired)
            IconButton(
              onPressed: _showMaintenanceWarningDialog,
              icon: Icon(Icons.build, color: Colors.orange.shade300),
              tooltip: 'Manutenção Necessária',
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
          _ensureBluetoothListener(bluetoothService);
          return SingleChildScrollView(
            padding: const EdgeInsets.all(16.0),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                // Card de Status da Cadeira
                _buildStatusCard(bluetoothService, settingsService),
                const SizedBox(height: 12),

                // Alerta de Manutenção Necessária
                if (_maintenanceRequired)
                  Padding(
                    padding: const EdgeInsets.only(bottom: 12.0),
                    child: Card(
                      color: Colors.orange.shade50,
                      child: InkWell(
                        onTap: _showMaintenanceWarningDialog,
                        child: Padding(
                          padding: const EdgeInsets.all(12.0),
                          child: Row(
                            children: [
                              Icon(
                                Icons.build,
                                color: Colors.orange.shade700,
                                size: 28,
                              ),
                              const SizedBox(width: 12),
                              Expanded(
                                child: Column(
                                  crossAxisAlignment: CrossAxisAlignment.start,
                                  children: [
                                    Text(
                                      'Manutenção Necessária',
                                      style: TextStyle(
                                        fontWeight: FontWeight.bold,
                                        color: Colors.orange.shade800,
                                      ),
                                    ),
                                    Text(
                                      'Após $_maintenanceHours horas de uso',
                                      style: TextStyle(
                                        fontSize: 12,
                                        color: Colors.orange.shade700,
                                      ),
                                    ),
                                  ],
                                ),
                              ),
                              Icon(
                                Icons.chevron_right,
                                color: Colors.orange.shade700,
                              ),
                            ],
                          ),
                        ),
                      ),
                    ),
                  ),

                // Botão Conectar Bluetooth
                if (!bluetoothService.isConnected)
                  Padding(
                    padding: const EdgeInsets.only(bottom: 12.0),
                    child: SizedBox(
                      height: 60,
                      child: ElevatedButton.icon(
                        onPressed: () =>
                            Navigator.pushNamed(context, '/bluetooth'),
                        icon: const Icon(Icons.bluetooth_searching, size: 28),
                        label: const Text(
                          'Conectar Bluetooth',
                          style: TextStyle(
                            fontSize: 18,
                            fontWeight: FontWeight.bold,
                          ),
                        ),
                        style: ElevatedButton.styleFrom(
                          backgroundColor: Colors.blue.shade700,
                          foregroundColor: Colors.white,
                          elevation: 4,
                          shape: RoundedRectangleBorder(
                            borderRadius: BorderRadius.circular(12),
                          ),
                        ),
                      ),
                    ),
                  ),

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

                // Visualização da Cadeira com Controles Integrados
                Center(
                  child: Consumer2<BluetoothService, MqttService>(
                    builder: (context, bluetoothService, mqttService, child) =>
                        ChairVisualization(
                          chairState: bluetoothService.isConnected
                              ? bluetoothService.chairState
                              : mqttService.chairState,
                          onReflectorToggle: () => _executeCommand(
                            context,
                            bluetoothService,
                            () => bluetoothService.toggleReflector(),
                          ),
                          onBackUp: () => _executeCommand(
                            context,
                            bluetoothService,
                            () => bluetoothService.moveBackUp(),
                          ),
                          onBackDown: () => _executeCommand(
                            context,
                            bluetoothService,
                            () => bluetoothService.moveBackDown(),
                          ),
                          onSeatUp: () => _executeCommand(
                            context,
                            bluetoothService,
                            () => bluetoothService.moveSeatUp(),
                          ),
                          onSeatDown: () => _executeCommand(
                            context,
                            bluetoothService,
                            () => bluetoothService.moveSeatDown(),
                          ),
                          onLegUp: () => _executeCommand(
                            context,
                            bluetoothService,
                            () => bluetoothService.toggleUpperLegs(),
                          ),
                          onLegDown: () => _executeCommand(
                            context,
                            bluetoothService,
                            () => bluetoothService.toggleLowerLegs(),
                          ),
                          onAutoZero: () => _executeCommand(
                            context,
                            bluetoothService,
                            () => bluetoothService.setGynecologicalPosition(),
                          ),
                          onWorkPosition: () => _executeCommand(
                            context,
                            bluetoothService,
                            () => bluetoothService.setBirthPosition(),
                          ),
                          onStopBackMovement: () {
                            bluetoothService.stopBackMovement();
                          },
                          onStopSeatMovement: () {
                            bluetoothService.stopSeatMovement();
                          },
                          onStopLegMovement: () {
                            bluetoothService.stopLegMovement();
                          },
                        ),
                  ),
                ),
                const SizedBox(height: 20),

                // Status dos Encoders e Limites
                if (bluetoothService.isConnected)
                  EncoderStatusWidget(chairState: bluetoothService.chairState),
                if (bluetoothService.isConnected) const SizedBox(height: 16),

                // Botões de controle rápido
                SingleChildScrollView(
                  scrollDirection: Axis.horizontal,
                  child: Row(
                    children: [
                      SizedBox(
                        width: 100,
                        child: _buildStyledButton(
                          label: 'FOCO AUX.',
                          icon: Icons.lightbulb,
                          color: Colors.amber.shade700,
                          isActive: bluetoothService.chairState.reflectorOn,
                          onPressed: () => _executeCommand(
                            context,
                            bluetoothService,
                            () => bluetoothService.toggleReflector(),
                          ),
                        ),
                      ),
                      const SizedBox(width: 6),
                      SizedBox(
                        width: 100,
                        child: _buildStyledButton(
                          label: 'PARAR\nTUDO',
                          icon: Icons.pan_tool,
                          color: Colors.red.shade700,
                          onPressed: () => _executeCommand(
                            context,
                            bluetoothService,
                            () => bluetoothService.stopAll(),
                          ),
                        ),
                      ),
                      const SizedBox(width: 6),
                      SizedBox(
                        width: 100,
                        child: _buildStyledButton(
                          label: 'MEM.\nTRAB.',
                          icon: Icons.bookmark,
                          color: Colors.blue.shade600,
                          onPressed: () =>
                              _executeCommand(context, bluetoothService, () {}),
                        ),
                      ),
                    ],
                  ),
                ),
                const SizedBox(height: 12),

                // Botões de Posições Pré-definidas
                SingleChildScrollView(
                  scrollDirection: Axis.horizontal,
                  child: Row(
                    children: [
                      SizedBox(
                        width: 160,
                        child: _buildStyledButton(
                          label: 'VOLTA\nA ZERO',
                          icon: Icons.airline_seat_recline_normal,
                          color: Colors.pink.shade700,
                          height: 70,
                          onPressed: () => _executeCommand(
                            context,
                            bluetoothService,
                            () => bluetoothService.setGynecologicalPosition(),
                          ),
                        ),
                      ),
                      const SizedBox(width: 12),
                      SizedBox(
                        width: 160,
                        child: _buildStyledButton(
                          label: 'POSIÇÃO\nDE TRABALHO',
                          icon: Icons.airline_seat_legroom_extra,
                          color: Colors.teal.shade700,
                          height: 70,
                          onPressed: () => _executeCommand(
                            context,
                            bluetoothService,
                            () => bluetoothService.setBirthPosition(),
                          ),
                        ),
                      ),
                    ],
                  ),
                ),
              ],
            ),
          );
        },
      ),
    );
  }
}

class _Pressable3D extends StatefulWidget {
  final Widget child;
  final VoidCallback onTap;

  const _Pressable3D({required this.child, required this.onTap});

  @override
  State<_Pressable3D> createState() => _Pressable3DState();
}

class _Pressable3DState extends State<_Pressable3D> {
  bool _pressed = false;

  @override
  Widget build(BuildContext context) {
    final scale = _pressed ? 0.965 : 1.0;
    final offsetY = _pressed ? 2.5 : 0.0;

    return GestureDetector(
      behavior: HitTestBehavior.opaque,
      onTap: widget.onTap,
      onTapDown: (_) => setState(() => _pressed = true),
      onTapUp: (_) => setState(() => _pressed = false),
      onTapCancel: () => setState(() => _pressed = false),
      child: AnimatedContainer(
        duration: const Duration(milliseconds: 90),
        curve: Curves.easeOut,
        transform: Matrix4.translationValues(0.0, offsetY, 0.0),
        child: AnimatedScale(
          duration: const Duration(milliseconds: 90),
          curve: Curves.easeOut,
          scale: scale,
          child: widget.child,
        ),
      ),
    );
  }
}
