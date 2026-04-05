import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import '../services/supabase_service.dart';
import 'package:intl/intl.dart';

class ManufacturerSettingsScreen extends StatefulWidget {
  const ManufacturerSettingsScreen({super.key});

  @override
  State<ManufacturerSettingsScreen> createState() =>
      _ManufacturerSettingsScreenState();
}

class _ManufacturerSettingsScreenState
    extends State<ManufacturerSettingsScreen> {
  final _passwordController = TextEditingController();
  final _serialController = TextEditingController();
  final _maintenanceHoursController = TextEditingController();
  bool _isAuthenticated = false;
  bool _obscurePassword = true;
  bool _isLoading = false;
  List<Map<String, dynamic>> _chairs = [];

  // Senha do fabricante (em produção, usar autenticação mais segura)
  static const String _manufacturerPassword = 'fab2024';

  @override
  void initState() {
    super.initState();
    _serialController.text =
        SupabaseService.instance.chairSerialNumber ?? 'CADEIRA-001';
  }

  @override
  void dispose() {
    _passwordController.dispose();
    _serialController.dispose();
    _maintenanceHoursController.dispose();
    super.dispose();
  }

  void _authenticate() {
    if (_passwordController.text == _manufacturerPassword) {
      setState(() {
        _isAuthenticated = true;
        _passwordController.clear();
      });
      _loadChairs();
    } else {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(
          content: Text('Senha do fabricante incorreta!'),
          backgroundColor: Colors.red,
        ),
      );
    }
  }

  Future<void> _loadChairs() async {
    setState(() => _isLoading = true);

    try {
      final supabase = SupabaseService.instance;
      if (!supabase.isInitialized) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(
            content: Text('Supabase não está configurado'),
            backgroundColor: Colors.orange,
          ),
        );
        setState(() => _isLoading = false);
        return;
      }

      final chairs = await supabase.select('chairs', orderBy: 'serial_number');
      setState(() {
        _chairs = chairs;
        _isLoading = false;
      });
    } catch (e) {
      setState(() => _isLoading = false);
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: Text('Erro ao carregar cadeiras: $e'),
            backgroundColor: Colors.red,
          ),
        );
      }
    }
  }

  Future<void> _toggleChairEnabled(Map<String, dynamic> chair) async {
    final supabase = SupabaseService.instance;
    final newEnabled = !(chair['enabled'] ?? true);

    try {
      await supabase.update(
        'chairs',
        {'enabled': newEnabled},
        match: {'serial_number': chair['serial_number']},
      );

      if (!mounted) return;
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text(
            newEnabled
                ? 'Cadeira ${chair['serial_number']} HABILITADA'
                : 'Cadeira ${chair['serial_number']} BLOQUEADA',
          ),
          backgroundColor: newEnabled ? Colors.green : Colors.red,
        ),
      );

      _loadChairs();
    } catch (e) {
      if (!mounted) return;
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text('Erro ao atualizar cadeira: $e'),
          backgroundColor: Colors.red,
        ),
      );
    }
  }

  Future<void> _toggleMaintenanceRequired(Map<String, dynamic> chair) async {
    final supabase = SupabaseService.instance;
    final newMaintenance = !(chair['maintenance_required'] ?? false);

    try {
      await supabase.update(
        'chairs',
        {'maintenance_required': newMaintenance},
        match: {'serial_number': chair['serial_number']},
      );

      if (!mounted) return;
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text(
            newMaintenance
                ? 'Manutenção NECESSÁRIA para ${chair['serial_number']}'
                : 'Manutenção removida para ${chair['serial_number']}',
          ),
          backgroundColor: newMaintenance ? Colors.orange : Colors.green,
        ),
      );

      _loadChairs();
    } catch (e) {
      if (!mounted) return;
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text('Erro ao atualizar manutenção: $e'),
          backgroundColor: Colors.red,
        ),
      );
    }
  }

  Future<void> _updateMaintenanceHours(Map<String, dynamic> chair) async {
    _maintenanceHoursController.text = (chair['maintenance_hours'] ?? 0)
        .toString();

    final result = await showDialog<double>(
      context: context,
      builder: (context) => AlertDialog(
        title: Text('Horas de Manutenção - ${chair['serial_number']}'),
        content: TextField(
          controller: _maintenanceHoursController,
          keyboardType: const TextInputType.numberWithOptions(decimal: true),
          inputFormatters: [
            FilteringTextInputFormatter.allow(RegExp(r'^\d*\.?\d*')),
          ],
          decoration: const InputDecoration(
            labelText: 'Horas para manutenção',
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
              final hours = double.tryParse(_maintenanceHoursController.text);
              Navigator.pop(context, hours);
            },
            child: const Text('SALVAR'),
          ),
        ],
      ),
    );

    if (result != null) {
      try {
        await SupabaseService.instance.update(
          'chairs',
          {'maintenance_hours': result},
          match: {'serial_number': chair['serial_number']},
        );
        _loadChairs();
      } catch (e) {
        if (mounted) {
          ScaffoldMessenger.of(context).showSnackBar(
            SnackBar(content: Text('Erro: $e'), backgroundColor: Colors.red),
          );
        }
      }
    }
  }

  Future<void> _registerNewChair() async {
    final serialController = TextEditingController();

    final result = await showDialog<String>(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Registrar Nova Cadeira'),
        content: TextField(
          controller: serialController,
          decoration: const InputDecoration(
            labelText: 'Número de Série',
            hintText: 'Ex: CADEIRA-002',
            border: OutlineInputBorder(),
          ),
          textCapitalization: TextCapitalization.characters,
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: const Text('CANCELAR'),
          ),
          ElevatedButton(
            onPressed: () => Navigator.pop(context, serialController.text),
            child: const Text('REGISTRAR'),
          ),
        ],
      ),
    );

    if (result != null && result.isNotEmpty) {
      try {
        await SupabaseService.instance.insert('chairs', {
          'serial_number': result.toUpperCase(),
          'hour_meter': 0,
          'enabled': true,
          'maintenance_required': false,
          'maintenance_hours': 0,
          'created_at': DateTime.now().toIso8601String(),
          'last_update': DateTime.now().toIso8601String(),
        });

        if (!mounted) return;
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: Text('Cadeira $result registrada com sucesso!'),
            backgroundColor: Colors.green,
          ),
        );

        _loadChairs();
      } catch (e) {
        if (!mounted) return;
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: Text('Erro ao registrar cadeira: $e'),
            backgroundColor: Colors.red,
          ),
        );
      }
    }
  }

  Future<void> _deleteChair(Map<String, dynamic> chair) async {
    final confirm = await showDialog<bool>(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Confirmar Exclusão'),
        content: Text(
          'Deseja realmente excluir a cadeira ${chair['serial_number']}?\n\n'
          'Esta ação não pode ser desfeita.',
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context, false),
            child: const Text('CANCELAR'),
          ),
          ElevatedButton(
            onPressed: () => Navigator.pop(context, true),
            style: ElevatedButton.styleFrom(backgroundColor: Colors.red),
            child: const Text('EXCLUIR'),
          ),
        ],
      ),
    );

    if (confirm == true) {
      try {
        await SupabaseService.instance.delete(
          'chairs',
          match: {'serial_number': chair['serial_number']},
        );

        if (!mounted) return;
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: Text('Cadeira ${chair['serial_number']} excluída'),
            backgroundColor: Colors.orange,
          ),
        );

        _loadChairs();
      } catch (e) {
        if (!mounted) return;
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: Text('Erro ao excluir cadeira: $e'),
            backgroundColor: Colors.red,
          ),
        );
      }
    }
  }

  @override
  Widget build(BuildContext context) {
    if (!_isAuthenticated) {
      return _buildLoginScreen();
    }
    return _buildManufacturerPanel();
  }

  Widget _buildLoginScreen() {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Área do Fabricante'),
        backgroundColor: Colors.indigo.shade700,
        foregroundColor: Colors.white,
      ),
      body: Center(
        child: SingleChildScrollView(
          padding: const EdgeInsets.all(24.0),
          child: Card(
            elevation: 8,
            shape: RoundedRectangleBorder(
              borderRadius: BorderRadius.circular(20),
            ),
            child: Padding(
              padding: const EdgeInsets.all(32.0),
              child: Column(
                mainAxisSize: MainAxisSize.min,
                children: [
                  Icon(Icons.factory, size: 80, color: Colors.indigo.shade700),
                  const SizedBox(height: 24),
                  const Text(
                    'Acesso Fabricante',
                    style: TextStyle(fontSize: 24, fontWeight: FontWeight.bold),
                  ),
                  const SizedBox(height: 8),
                  Text(
                    'Digite a senha de acesso do fabricante',
                    style: TextStyle(fontSize: 14, color: Colors.grey.shade600),
                  ),
                  const SizedBox(height: 32),
                  TextField(
                    controller: _passwordController,
                    obscureText: _obscurePassword,
                    decoration: InputDecoration(
                      labelText: 'Senha do Fabricante',
                      prefixIcon: const Icon(Icons.vpn_key),
                      suffixIcon: IconButton(
                        icon: Icon(
                          _obscurePassword
                              ? Icons.visibility
                              : Icons.visibility_off,
                        ),
                        onPressed: () {
                          setState(() {
                            _obscurePassword = !_obscurePassword;
                          });
                        },
                      ),
                      border: OutlineInputBorder(
                        borderRadius: BorderRadius.circular(12),
                      ),
                    ),
                    onSubmitted: (_) => _authenticate(),
                  ),
                  const SizedBox(height: 24),
                  SizedBox(
                    width: double.infinity,
                    height: 50,
                    child: ElevatedButton(
                      onPressed: _authenticate,
                      style: ElevatedButton.styleFrom(
                        backgroundColor: Colors.indigo.shade700,
                        foregroundColor: Colors.white,
                        shape: RoundedRectangleBorder(
                          borderRadius: BorderRadius.circular(12),
                        ),
                      ),
                      child: const Text(
                        'ACESSAR',
                        style: TextStyle(
                          fontSize: 16,
                          fontWeight: FontWeight.bold,
                        ),
                      ),
                    ),
                  ),
                ],
              ),
            ),
          ),
        ),
      ),
    );
  }

  Widget _buildManufacturerPanel() {
    final supabase = SupabaseService.instance;
    final dateFormat = DateFormat('dd/MM/yyyy HH:mm');

    return Scaffold(
      appBar: AppBar(
        title: const Text('Painel do Fabricante'),
        backgroundColor: Colors.indigo.shade700,
        foregroundColor: Colors.white,
        actions: [
          IconButton(
            icon: const Icon(Icons.refresh),
            tooltip: 'Atualizar',
            onPressed: _loadChairs,
          ),
          IconButton(
            icon: const Icon(Icons.logout),
            tooltip: 'Sair',
            onPressed: () => setState(() => _isAuthenticated = false),
          ),
        ],
      ),
      floatingActionButton: FloatingActionButton.extended(
        onPressed: _registerNewChair,
        backgroundColor: Colors.indigo.shade700,
        icon: const Icon(Icons.add),
        label: const Text('Nova Cadeira'),
      ),
      body: !supabase.isInitialized
          ? _buildSupabaseNotConfigured()
          : _isLoading
          ? const Center(child: CircularProgressIndicator())
          : _buildChairsList(dateFormat),
    );
  }

  Widget _buildSupabaseNotConfigured() {
    return Center(
      child: Padding(
        padding: const EdgeInsets.all(32.0),
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            Icon(Icons.cloud_off, size: 80, color: Colors.grey.shade400),
            const SizedBox(height: 24),
            const Text(
              'Supabase Não Configurado',
              style: TextStyle(fontSize: 20, fontWeight: FontWeight.bold),
            ),
            const SizedBox(height: 12),
            Text(
              'Configure as credenciais do Supabase em\nsupabase_service.dart para usar o controle remoto.',
              textAlign: TextAlign.center,
              style: TextStyle(color: Colors.grey.shade600),
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildChairsList(DateFormat dateFormat) {
    if (_chairs.isEmpty) {
      return Center(
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            Icon(Icons.event_seat, size: 80, color: Colors.grey.shade400),
            const SizedBox(height: 24),
            const Text(
              'Nenhuma cadeira registrada',
              style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold),
            ),
            const SizedBox(height: 12),
            const Text('Clique em "Nova Cadeira" para registrar'),
          ],
        ),
      );
    }

    return ListView.builder(
      padding: const EdgeInsets.all(16),
      itemCount: _chairs.length,
      itemBuilder: (context, index) {
        final chair = _chairs[index];
        final isEnabled = chair['enabled'] ?? true;
        final needsMaintenance = chair['maintenance_required'] ?? false;
        final hourMeter = (chair['hour_meter'] ?? 0).toDouble();
        final maintenanceHours = (chair['maintenance_hours'] ?? 0).toDouble();

        return Card(
          elevation: 4,
          margin: const EdgeInsets.only(bottom: 16),
          shape: RoundedRectangleBorder(
            borderRadius: BorderRadius.circular(16),
            side: BorderSide(
              color: !isEnabled
                  ? Colors.red
                  : needsMaintenance
                  ? Colors.orange
                  : Colors.green,
              width: 2,
            ),
          ),
          child: ExpansionTile(
            leading: CircleAvatar(
              backgroundColor: !isEnabled
                  ? Colors.red
                  : needsMaintenance
                  ? Colors.orange
                  : Colors.green,
              child: Icon(
                !isEnabled
                    ? Icons.block
                    : needsMaintenance
                    ? Icons.build
                    : Icons.check,
                color: Colors.white,
              ),
            ),
            title: Text(
              chair['serial_number'] ?? 'Sem número',
              style: const TextStyle(fontWeight: FontWeight.bold, fontSize: 18),
            ),
            subtitle: Text(
              !isEnabled
                  ? '🔴 BLOQUEADA'
                  : needsMaintenance
                  ? '🟠 Manutenção necessária'
                  : '🟢 Operacional',
            ),
            children: [
              Padding(
                padding: const EdgeInsets.all(16.0),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    _buildChairInfoRow(
                      Icons.schedule,
                      'Horímetro',
                      '${hourMeter.toStringAsFixed(1)} horas',
                    ),
                    const SizedBox(height: 8),
                    _buildChairInfoRow(
                      Icons.build,
                      'Horas para manutenção',
                      '${maintenanceHours.toStringAsFixed(0)} horas',
                    ),
                    if (chair['last_update'] != null) ...[
                      const SizedBox(height: 8),
                      _buildChairInfoRow(
                        Icons.update,
                        'Última atualização',
                        dateFormat.format(DateTime.parse(chair['last_update'])),
                      ),
                    ],
                    const Divider(height: 32),
                    const Text(
                      'Controles',
                      style: TextStyle(
                        fontWeight: FontWeight.bold,
                        fontSize: 16,
                      ),
                    ),
                    const SizedBox(height: 16),
                    Row(
                      children: [
                        Expanded(
                          child: ElevatedButton.icon(
                            onPressed: () => _toggleChairEnabled(chair),
                            icon: Icon(isEnabled ? Icons.block : Icons.check),
                            label: Text(isEnabled ? 'BLOQUEAR' : 'HABILITAR'),
                            style: ElevatedButton.styleFrom(
                              backgroundColor: isEnabled
                                  ? Colors.red
                                  : Colors.green,
                              foregroundColor: Colors.white,
                              padding: const EdgeInsets.all(12),
                            ),
                          ),
                        ),
                        const SizedBox(width: 12),
                        Expanded(
                          child: ElevatedButton.icon(
                            onPressed: () => _toggleMaintenanceRequired(chair),
                            icon: Icon(
                              needsMaintenance
                                  ? Icons.check_circle
                                  : Icons.build,
                            ),
                            label: Text(
                              needsMaintenance ? 'CONCLUIR' : 'MANUTENÇÃO',
                            ),
                            style: ElevatedButton.styleFrom(
                              backgroundColor: needsMaintenance
                                  ? Colors.green
                                  : Colors.orange,
                              foregroundColor: Colors.white,
                              padding: const EdgeInsets.all(12),
                            ),
                          ),
                        ),
                      ],
                    ),
                    const SizedBox(height: 12),
                    Row(
                      children: [
                        Expanded(
                          child: OutlinedButton.icon(
                            onPressed: () => _updateMaintenanceHours(chair),
                            icon: const Icon(Icons.edit),
                            label: const Text('EDITAR HORAS'),
                          ),
                        ),
                        const SizedBox(width: 12),
                        Expanded(
                          child: OutlinedButton.icon(
                            onPressed: () => _deleteChair(chair),
                            icon: const Icon(Icons.delete, color: Colors.red),
                            label: const Text(
                              'EXCLUIR',
                              style: TextStyle(color: Colors.red),
                            ),
                            style: OutlinedButton.styleFrom(
                              side: const BorderSide(color: Colors.red),
                            ),
                          ),
                        ),
                      ],
                    ),
                  ],
                ),
              ),
            ],
          ),
        );
      },
    );
  }

  Widget _buildChairInfoRow(IconData icon, String label, String value) {
    return Row(
      children: [
        Icon(icon, size: 20, color: Colors.grey.shade600),
        const SizedBox(width: 8),
        Text(label, style: TextStyle(color: Colors.grey.shade600)),
        const Spacer(),
        Text(value, style: const TextStyle(fontWeight: FontWeight.bold)),
      ],
    );
  }
}
