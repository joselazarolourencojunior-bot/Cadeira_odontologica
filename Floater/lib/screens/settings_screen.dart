import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:provider/provider.dart';
import 'package:qr_flutter/qr_flutter.dart';
import '../services/settings_service.dart';
import 'package:intl/intl.dart';
import 'qr_scan_screen.dart';

class SettingsScreen extends StatefulWidget {
  const SettingsScreen({super.key});

  @override
  State<SettingsScreen> createState() => _SettingsScreenState();
}

class _SettingsScreenState extends State<SettingsScreen> {
  final _newPasswordController = TextEditingController();
  final _maintenanceIntervalController = TextEditingController();

  @override
  void dispose() {
    _newPasswordController.dispose();
    _maintenanceIntervalController.dispose();
    super.dispose();
  }

  void _logout() {}

  @override
  Widget build(BuildContext context) {
    return _buildSettingsScreen();
  }

  Widget _buildSettingsScreen() {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Configurações'),
        backgroundColor: Colors.blue.shade700,
        foregroundColor: Colors.white,
        actions: [
          IconButton(
            icon: const Icon(Icons.logout),
            tooltip: 'Sair',
            onPressed: _logout,
          ),
        ],
      ),
      body: Consumer<SettingsService>(
        builder: (context, settingsService, child) {
          final settings = settingsService.settings;
          final dateFormat = DateFormat('dd/MM/yyyy HH:mm');

          return SingleChildScrollView(
            padding: const EdgeInsets.all(16.0),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                // Horímetro
                Card(
                  elevation: 4,
                  child: Padding(
                    padding: const EdgeInsets.all(16.0),
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Row(
                          children: [
                            Icon(
                              Icons.schedule,
                              color: Colors.blue.shade700,
                              size: 28,
                            ),
                            const SizedBox(width: 12),
                            const Text(
                              'Horímetro',
                              style: TextStyle(
                                fontSize: 20,
                                fontWeight: FontWeight.bold,
                              ),
                            ),
                          ],
                        ),
                        const Divider(height: 24),
                        _buildInfoRow(
                          'Horas Totais de Uso',
                          '${settings.totalHours.toStringAsFixed(1)} h',
                          Colors.blue.shade700,
                        ),
                        const SizedBox(height: 12),
                        _buildInfoRow(
                          'Horas desde Manutenção',
                          '${settings.hoursSinceMaintenance.toStringAsFixed(1)} h',
                          settings.needsMaintenance()
                              ? Colors.red
                              : settings.nearMaintenance()
                              ? Colors.orange
                              : Colors.green,
                        ),
                        if (settings.lastMaintenanceDate != null) ...[
                          const SizedBox(height: 12),
                          _buildInfoRow(
                            'Última Manutenção',
                            dateFormat.format(settings.lastMaintenanceDate!),
                            Colors.grey.shade700,
                          ),
                        ],
                        const SizedBox(height: 16),
                        ElevatedButton.icon(
                          onPressed: () => _showResetTotalHoursDialog(context),
                          icon: const Icon(Icons.refresh),
                          label: const Text('Resetar Horímetro Total'),
                          style: ElevatedButton.styleFrom(
                            backgroundColor: Colors.orange.shade700,
                            foregroundColor: Colors.white,
                          ),
                        ),
                      ],
                    ),
                  ),
                ),
                const SizedBox(height: 16),

                Card(
                  elevation: 4,
                  child: Padding(
                    padding: const EdgeInsets.all(16.0),
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Row(
                          children: [
                            Icon(
                              Icons.settings_input_antenna,
                              color: Colors.teal.shade700,
                              size: 28,
                            ),
                            const SizedBox(width: 12),
                            const Text(
                              'Conexão',
                              style: TextStyle(
                                fontSize: 20,
                                fontWeight: FontWeight.bold,
                              ),
                            ),
                          ],
                        ),
                        const Divider(height: 24),
                        ListTile(
                          title: const Text('SERIAL da Cadeira'),
                          subtitle: Text(
                            settings.chairSerial.trim().isEmpty
                                ? 'Não configurado'
                                : settings.chairSerial.trim(),
                          ),
                          trailing: const Icon(Icons.qr_code_scanner),
                          onTap: () => _showSerialQrDialog(context),
                        ),
                      ],
                    ),
                  ),
                ),
                const SizedBox(height: 16),

                // Manutenção
                Card(
                  elevation: 4,
                  child: Padding(
                    padding: const EdgeInsets.all(16.0),
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Row(
                          children: [
                            Icon(
                              Icons.build,
                              color: Colors.orange.shade700,
                              size: 28,
                            ),
                            const SizedBox(width: 12),
                            const Text(
                              'Manutenção',
                              style: TextStyle(
                                fontSize: 20,
                                fontWeight: FontWeight.bold,
                              ),
                            ),
                          ],
                        ),
                        const Divider(height: 24),
                        SwitchListTile(
                          title: const Text('Alertas de Manutenção'),
                          subtitle: const Text(
                            'Ativar notificações de manutenção',
                          ),
                          value: settings.maintenanceAlertEnabled,
                          onChanged: (value) {
                            settingsService.toggleMaintenanceAlert(value);
                          },
                          activeTrackColor: Colors.blue.shade700,
                        ),
                        const SizedBox(height: 16),
                        ListTile(
                          title: const Text('Intervalo de Manutenção'),
                          subtitle: Text(
                            '${settings.maintenanceIntervalHours.toStringAsFixed(0)} horas',
                          ),
                          trailing: IconButton(
                            icon: const Icon(Icons.edit),
                            onPressed: () =>
                                _showMaintenanceIntervalDialog(context),
                          ),
                        ),
                        const SizedBox(height: 16),
                        if (settings.needsMaintenance())
                          Container(
                            padding: const EdgeInsets.all(12),
                            decoration: BoxDecoration(
                              color: Colors.red.shade50,
                              border: Border.all(
                                color: Colors.red.shade700,
                                width: 2,
                              ),
                              borderRadius: BorderRadius.circular(12),
                            ),
                            child: Row(
                              children: [
                                Icon(Icons.warning, color: Colors.red.shade700),
                                const SizedBox(width: 12),
                                Expanded(
                                  child: Text(
                                    'MANUTENÇÃO NECESSÁRIA!',
                                    style: TextStyle(
                                      color: Colors.red.shade700,
                                      fontWeight: FontWeight.bold,
                                    ),
                                  ),
                                ),
                              ],
                            ),
                          )
                        else if (settings.nearMaintenance())
                          Container(
                            padding: const EdgeInsets.all(12),
                            decoration: BoxDecoration(
                              color: Colors.orange.shade50,
                              border: Border.all(
                                color: Colors.orange.shade700,
                                width: 2,
                              ),
                              borderRadius: BorderRadius.circular(12),
                            ),
                            child: Row(
                              children: [
                                Icon(Icons.info, color: Colors.orange.shade700),
                                const SizedBox(width: 12),
                                Expanded(
                                  child: Text(
                                    'Manutenção em breve',
                                    style: TextStyle(
                                      color: Colors.orange.shade700,
                                      fontWeight: FontWeight.bold,
                                    ),
                                  ),
                                ),
                              ],
                            ),
                          ),
                        const SizedBox(height: 16),
                        SizedBox(
                          width: double.infinity,
                          child: ElevatedButton.icon(
                            onPressed: () =>
                                _showMaintenanceCompleteDialog(context),
                            icon: const Icon(Icons.check_circle),
                            label: const Text('Registrar Manutenção Concluída'),
                            style: ElevatedButton.styleFrom(
                              backgroundColor: Colors.green.shade700,
                              foregroundColor: Colors.white,
                              padding: const EdgeInsets.all(16),
                            ),
                          ),
                        ),
                      ],
                    ),
                  ),
                ),
                const SizedBox(height: 16),

                // Segurança
                Card(
                  elevation: 4,
                  child: Padding(
                    padding: const EdgeInsets.all(16.0),
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Row(
                          children: [
                            Icon(
                              Icons.security,
                              color: Colors.purple.shade700,
                              size: 28,
                            ),
                            const SizedBox(width: 12),
                            const Text(
                              'Segurança',
                              style: TextStyle(
                                fontSize: 20,
                                fontWeight: FontWeight.bold,
                              ),
                            ),
                          ],
                        ),
                        const Divider(height: 24),
                        ListTile(
                          title: const Text('Alterar Senha'),
                          subtitle: const Text(
                            'Senha de acesso às configurações',
                          ),
                          trailing: const Icon(Icons.arrow_forward_ios),
                          onTap: () => _showChangePasswordDialog(context),
                        ),
                      ],
                    ),
                  ),
                ),
              ],
            ),
          );
        },
      ),
    );
  }

  Future<void> _showSerialQrDialog(BuildContext context) async {
    final settingsService = context.read<SettingsService>();
    final controller = TextEditingController(
      text: settingsService.settings.chairSerial.trim(),
    );

    String normalizeSerial(String raw) {
      final v = raw.trim();
      if (v.isEmpty) return '';
      final up = v.toUpperCase();
      if (up.startsWith('CADEIRA-')) return up;
      return 'CADEIRA-$up';
    }

    String extractSerial(String raw) {
      final text = raw.trim();
      if (text.isEmpty) return '';
      final upper = text.toUpperCase();

      final match = RegExp(r'CADEIRA-[A-Z0-9]+').firstMatch(upper);
      if (match != null) return match.group(0) ?? '';

      if (RegExp(r'^[A-F0-9]{12}$').hasMatch(upper)) return 'CADEIRA-$upper';

      return normalizeSerial(text);
    }

    await showModalBottomSheet(
      context: context,
      isScrollControlled: true,
      builder: (context) {
        return Padding(
          padding: EdgeInsets.only(
            left: 16,
            right: 16,
            top: 16,
            bottom: MediaQuery.of(context).viewInsets.bottom + 16,
          ),
          child: Column(
            mainAxisSize: MainAxisSize.min,
            crossAxisAlignment: CrossAxisAlignment.stretch,
            children: [
              const Text(
                'SERIAL via QR Code',
                style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold),
              ),
              const SizedBox(height: 12),
              TextField(
                controller: controller,
                decoration: const InputDecoration(
                  labelText: 'SERIAL (ex: D83BDA4CF344)',
                  border: OutlineInputBorder(),
                ),
              ),
              const SizedBox(height: 12),
              Row(
                children: [
                  Expanded(
                    child: OutlinedButton.icon(
                      onPressed: () async {
                        final scanned = await Navigator.of(context).push(
                          MaterialPageRoute<String>(
                            builder: (_) => const QrScanScreen(
                              title: 'Ler QR Code do SERIAL',
                            ),
                          ),
                        );
                        if (scanned == null) return;
                        controller.text = extractSerial(scanned);
                      },
                      icon: const Icon(Icons.qr_code_scanner),
                      label: const Text('Ler QR'),
                    ),
                  ),
                  const SizedBox(width: 12),
                  Expanded(
                    child: OutlinedButton.icon(
                      onPressed: () {
                        final value = extractSerial(controller.text);
                        if (value.isEmpty) return;
                        showDialog(
                          context: context,
                          builder: (context) => AlertDialog(
                            title: const Text('QR Code do SERIAL'),
                            content: SizedBox(
                              width: 260,
                              height: 260,
                              child: QrImageView(
                                data: value,
                                backgroundColor: Colors.white,
                              ),
                            ),
                            actions: [
                              TextButton(
                                onPressed: () => Navigator.pop(context),
                                child: const Text('FECHAR'),
                              ),
                            ],
                          ),
                        );
                      },
                      icon: const Icon(Icons.qr_code),
                      label: const Text('Mostrar QR'),
                    ),
                  ),
                ],
              ),
              const SizedBox(height: 12),
              ElevatedButton(
                onPressed: () async {
                  final value = extractSerial(controller.text);
                  if (value.isEmpty) {
                    Navigator.pop(context);
                    return;
                  }
                  await settingsService.setChairSerial(value);
                  if (!context.mounted) return;
                  Navigator.pop(context);
                  ScaffoldMessenger.of(context).showSnackBar(
                    const SnackBar(
                      content: Text('SERIAL atualizado'),
                      backgroundColor: Colors.green,
                    ),
                  );
                },
                child: const Text('SALVAR'),
              ),
            ],
          ),
        );
      },
    );
  }

  Widget _buildInfoRow(String label, String value, Color valueColor) {
    return Row(
      mainAxisAlignment: MainAxisAlignment.spaceBetween,
      children: [
        Text(label, style: const TextStyle(fontSize: 16)),
        Text(
          value,
          style: TextStyle(
            fontSize: 18,
            fontWeight: FontWeight.bold,
            color: valueColor,
          ),
        ),
      ],
    );
  }

  void _showMaintenanceIntervalDialog(BuildContext context) {
    final settingsService = context.read<SettingsService>();
    _maintenanceIntervalController.text = settingsService
        .settings
        .maintenanceIntervalHours
        .toStringAsFixed(0);

    showDialog(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Intervalo de Manutenção'),
        content: TextField(
          controller: _maintenanceIntervalController,
          keyboardType: TextInputType.number,
          inputFormatters: [FilteringTextInputFormatter.digitsOnly],
          decoration: const InputDecoration(
            labelText: 'Horas',
            suffixText: 'h',
            border: OutlineInputBorder(),
          ),
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: const Text('CANCELAR'),
          ),
          ElevatedButton(
            onPressed: () {
              final hours = double.tryParse(
                _maintenanceIntervalController.text,
              );
              if (hours != null && hours > 0) {
                settingsService.setMaintenanceInterval(hours);
                Navigator.pop(context);
                ScaffoldMessenger.of(context).showSnackBar(
                  const SnackBar(
                    content: Text('Intervalo de manutenção atualizado'),
                    backgroundColor: Colors.green,
                  ),
                );
              }
            },
            child: const Text('SALVAR'),
          ),
        ],
      ),
    );
  }

  void _showMaintenanceCompleteDialog(BuildContext context) {
    showDialog(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Confirmar Manutenção'),
        content: const Text(
          'Deseja registrar que a manutenção foi realizada? '
          'O contador de horas será resetado.',
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: const Text('CANCELAR'),
          ),
          ElevatedButton(
            onPressed: () {
              context.read<SettingsService>().recordMaintenance();
              Navigator.pop(context);
              ScaffoldMessenger.of(context).showSnackBar(
                const SnackBar(
                  content: Text('Manutenção registrada com sucesso'),
                  backgroundColor: Colors.green,
                ),
              );
            },
            style: ElevatedButton.styleFrom(
              backgroundColor: Colors.green.shade700,
            ),
            child: const Text('CONFIRMAR'),
          ),
        ],
      ),
    );
  }

  void _showResetTotalHoursDialog(BuildContext context) {
    showDialog(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Resetar Horímetro Total'),
        content: const Text(
          'ATENÇÃO: Esta ação irá zerar o contador total de horas de uso. '
          'Esta operação não pode ser desfeita. Deseja continuar?',
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: const Text('CANCELAR'),
          ),
          ElevatedButton(
            onPressed: () {
              context.read<SettingsService>().resetTotalHours();
              Navigator.pop(context);
              ScaffoldMessenger.of(context).showSnackBar(
                const SnackBar(
                  content: Text('Horímetro total resetado'),
                  backgroundColor: Colors.orange,
                ),
              );
            },
            style: ElevatedButton.styleFrom(
              backgroundColor: Colors.orange.shade700,
            ),
            child: const Text('RESETAR'),
          ),
        ],
      ),
    );
  }

  void _showChangePasswordDialog(BuildContext context) {
    final currentPasswordController = TextEditingController();
    _newPasswordController.clear();
    bool obscureCurrent = true;
    bool obscureNew = true;

    showDialog(
      context: context,
      builder: (context) => StatefulBuilder(
        builder: (context, setState) => AlertDialog(
          title: const Text('Alterar Senha'),
          content: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              TextField(
                controller: currentPasswordController,
                obscureText: obscureCurrent,
                decoration: InputDecoration(
                  labelText: 'Senha Atual',
                  prefixIcon: const Icon(Icons.lock_outline),
                  suffixIcon: IconButton(
                    icon: Icon(
                      obscureCurrent ? Icons.visibility : Icons.visibility_off,
                    ),
                    onPressed: () {
                      setState(() {
                        obscureCurrent = !obscureCurrent;
                      });
                    },
                  ),
                  border: const OutlineInputBorder(),
                ),
              ),
              const SizedBox(height: 16),
              TextField(
                controller: _newPasswordController,
                obscureText: obscureNew,
                decoration: InputDecoration(
                  labelText: 'Nova Senha',
                  prefixIcon: const Icon(Icons.vpn_key),
                  suffixIcon: IconButton(
                    icon: Icon(
                      obscureNew ? Icons.visibility : Icons.visibility_off,
                    ),
                    onPressed: () {
                      setState(() {
                        obscureNew = !obscureNew;
                      });
                    },
                  ),
                  border: const OutlineInputBorder(),
                ),
              ),
            ],
          ),
          actions: [
            TextButton(
              onPressed: () {
                currentPasswordController.dispose();
                Navigator.pop(context);
              },
              child: const Text('CANCELAR'),
            ),
            ElevatedButton(
              onPressed: () {
                final settingsService = context.read<SettingsService>();
                if (settingsService.verifyPassword(
                  currentPasswordController.text,
                )) {
                  if (_newPasswordController.text.length >= 4) {
                    settingsService.changePassword(_newPasswordController.text);
                    currentPasswordController.dispose();
                    Navigator.pop(context);
                    ScaffoldMessenger.of(context).showSnackBar(
                      const SnackBar(
                        content: Text('Senha alterada com sucesso'),
                        backgroundColor: Colors.green,
                      ),
                    );
                  } else {
                    ScaffoldMessenger.of(context).showSnackBar(
                      const SnackBar(
                        content: Text(
                          'A senha deve ter pelo menos 4 caracteres',
                        ),
                        backgroundColor: Colors.red,
                      ),
                    );
                  }
                } else {
                  ScaffoldMessenger.of(context).showSnackBar(
                    const SnackBar(
                      content: Text('Senha atual incorreta'),
                      backgroundColor: Colors.red,
                    ),
                  );
                }
              },
              child: const Text('ALTERAR'),
            ),
          ],
        ),
      ),
    );
  }
}
