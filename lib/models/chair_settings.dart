class ChairSettings {
  // Horímetro (horas de uso)
  double totalHours;
  double hoursSinceMaintenance;

  // Alertas de manutenção
  double maintenanceIntervalHours;
  bool maintenanceAlertEnabled;

  // Senha de acesso
  String adminPassword;

  // Data da última manutenção
  DateTime? lastMaintenanceDate;

  ChairSettings({
    this.totalHours = 0.0,
    this.hoursSinceMaintenance = 0.0,
    this.maintenanceIntervalHours = 100.0,
    this.maintenanceAlertEnabled = true,
    this.adminPassword = '1234',
    this.lastMaintenanceDate,
  });

  // Verificar se precisa de manutenção
  bool needsMaintenance() {
    if (!maintenanceAlertEnabled) return false;
    return hoursSinceMaintenance >= maintenanceIntervalHours;
  }

  // Verificar se está próximo da manutenção (80% do intervalo)
  bool nearMaintenance() {
    if (!maintenanceAlertEnabled) return false;
    return hoursSinceMaintenance >= (maintenanceIntervalHours * 0.8);
  }

  // Resetar contador de manutenção
  void resetMaintenance() {
    hoursSinceMaintenance = 0.0;
    lastMaintenanceDate = DateTime.now();
  }

  // Adicionar horas de uso
  void addUsageHours(double hours) {
    totalHours += hours;
    hoursSinceMaintenance += hours;
  }

  // Converter para Map para salvar
  Map<String, dynamic> toMap() {
    return {
      'totalHours': totalHours,
      'hoursSinceMaintenance': hoursSinceMaintenance,
      'maintenanceIntervalHours': maintenanceIntervalHours,
      'maintenanceAlertEnabled': maintenanceAlertEnabled,
      'adminPassword': adminPassword,
      'lastMaintenanceDate': lastMaintenanceDate?.toIso8601String(),
    };
  }

  // Criar a partir de Map
  factory ChairSettings.fromMap(Map<String, dynamic> map) {
    return ChairSettings(
      totalHours: map['totalHours']?.toDouble() ?? 0.0,
      hoursSinceMaintenance: map['hoursSinceMaintenance']?.toDouble() ?? 0.0,
      maintenanceIntervalHours:
          map['maintenanceIntervalHours']?.toDouble() ?? 100.0,
      maintenanceAlertEnabled: map['maintenanceAlertEnabled'] ?? true,
      adminPassword: map['adminPassword'] ?? '1234',
      lastMaintenanceDate: map['lastMaintenanceDate'] != null
          ? DateTime.parse(map['lastMaintenanceDate'])
          : null,
    );
  }
}
