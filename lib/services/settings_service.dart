import 'dart:convert';
import 'package:flutter/foundation.dart';
import 'package:shared_preferences/shared_preferences.dart';
import '../models/chair_settings.dart';

class SettingsService extends ChangeNotifier {
  ChairSettings _settings = ChairSettings();
  DateTime? _sessionStartTime;

  ChairSettings get settings => _settings;

  // Carregar configurações salvas
  Future<void> loadSettings() async {
    try {
      final prefs = await SharedPreferences.getInstance();
      final settingsJson = prefs.getString('chair_settings');

      if (settingsJson != null) {
        final Map<String, dynamic> settingsMap = json.decode(settingsJson);
        _settings = ChairSettings.fromMap(settingsMap);
        notifyListeners();
      }
    } catch (e) {
      debugPrint('Erro ao carregar configurações: $e');
    }
  }

  // Salvar configurações
  Future<void> saveSettings() async {
    try {
      final prefs = await SharedPreferences.getInstance();
      final settingsJson = json.encode(_settings.toMap());
      await prefs.setString('chair_settings', settingsJson);
    } catch (e) {
      debugPrint('Erro ao salvar configurações: $e');
    }
  }

  // Iniciar sessão de uso
  void startSession() {
    _sessionStartTime = DateTime.now();
  }

  // Finalizar sessão e atualizar horímetro
  void endSession() {
    if (_sessionStartTime != null) {
      final duration = DateTime.now().difference(_sessionStartTime!);
      final hours = duration.inMinutes / 60.0;
      _settings.addUsageHours(hours);
      _sessionStartTime = null;
      saveSettings();
      notifyListeners();
    }
  }

  // Verificar senha
  bool verifyPassword(String password) {
    return password == _settings.adminPassword;
  }

  // Alterar senha
  Future<void> changePassword(String newPassword) async {
    _settings.adminPassword = newPassword;
    await saveSettings();
    notifyListeners();
  }

  // Alterar intervalo de manutenção
  Future<void> setMaintenanceInterval(double hours) async {
    _settings.maintenanceIntervalHours = hours;
    await saveSettings();
    notifyListeners();
  }

  // Ativar/desativar alertas de manutenção
  Future<void> toggleMaintenanceAlert(bool enabled) async {
    _settings.maintenanceAlertEnabled = enabled;
    await saveSettings();
    notifyListeners();
  }

  // Registrar manutenção realizada
  Future<void> recordMaintenance() async {
    _settings.resetMaintenance();
    await saveSettings();
    notifyListeners();
  }

  // Resetar horímetro total (requer confirmação)
  Future<void> resetTotalHours() async {
    _settings.totalHours = 0.0;
    await saveSettings();
    notifyListeners();
  }
}
