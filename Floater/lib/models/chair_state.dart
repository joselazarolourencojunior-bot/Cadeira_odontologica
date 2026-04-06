class ChairState {
  bool reflectorOn;
  bool upperLegsOn;
  bool lowerLegsOn;
  bool seatUpOn;
  bool seatDownOn;
  bool backUpOn;
  bool backDownOn;

  // Novos campos - limites dos encoders
  bool seatUpLimit;
  bool seatDownLimit;
  bool backUpLimit;
  bool backDownLimit;
  bool legUpLimit;
  bool legDownLimit;

  // Posições virtuais dos encoders (0-50)
  int backPosition;
  int seatPosition;
  int legPosition;

  // Status WiFi
  bool wifiConnected;
  String? wifiSsid;
  String? wifiIp;

  // Horímetro da cadeira
  double? hourMeter;

  // Estado de habilitação
  bool enabled;
  bool maintenanceRequired;

  // Flag para parar todos os timers da UI quando limite atingido
  bool shouldStopAllTimers;

  // Estado das automações
  bool isMovingToGineco; // Volta a Zero (VZ)
  bool isMovingToParto; // Posição de Trabalho (PT)

  ChairState({
    this.reflectorOn = false,
    this.upperLegsOn = false,
    this.lowerLegsOn = false,
    this.seatUpOn = false,
    this.seatDownOn = false,
    this.backUpOn = false,
    this.backDownOn = false,
    this.seatUpLimit = false,
    this.seatDownLimit = false,
    this.backUpLimit = false,
    this.backDownLimit = false,
    this.legUpLimit = false,
    this.legDownLimit = false,
    this.backPosition = 0,
    this.seatPosition = 0,
    this.legPosition = 0,
    this.wifiConnected = false,
    this.wifiSsid,
    this.wifiIp,
    this.hourMeter,
    this.enabled = true,
    this.maintenanceRequired = false,
    this.shouldStopAllTimers = false,
    this.isMovingToGineco = false,
    this.isMovingToParto = false,
  });

  Map<String, dynamic> toJson() {
    return {
      'reflectorOn': reflectorOn,
      'upperLegsOn': upperLegsOn,
      'lowerLegsOn': lowerLegsOn,
      'seatUpOn': seatUpOn,
      'seatDownOn': seatDownOn,
      'backUpOn': backUpOn,
      'backDownOn': backDownOn,
      'seatUpLimit': seatUpLimit,
      'seatDownLimit': seatDownLimit,
      'backUpLimit': backUpLimit,
      'backDownLimit': backDownLimit,
      'legUpLimit': legUpLimit,
      'legDownLimit': legDownLimit,
      'backPosition': backPosition,
      'seatPosition': seatPosition,
      'legPosition': legPosition,
      'wifiConnected': wifiConnected,
      'wifiSsid': wifiSsid,
      'wifiIp': wifiIp,
      'hourMeter': hourMeter,
      'enabled': enabled,
      'maintenanceRequired': maintenanceRequired,
      'shouldStopAllTimers': shouldStopAllTimers,
      'isMovingToGineco': isMovingToGineco,
      'isMovingToParto': isMovingToParto,
    };
  }

  factory ChairState.fromJson(Map<String, dynamic> json) {
    return ChairState(
      reflectorOn: json['reflectorOn'] ?? json['refletor'] ?? false,
      upperLegsOn: json['upperLegsOn'] ?? false,
      lowerLegsOn: json['lowerLegsOn'] ?? false,
      seatUpOn: json['seatUpOn'] ?? false,
      seatDownOn: json['seatDownOn'] ?? false,
      backUpOn: json['backUpOn'] ?? false,
      backDownOn: json['backDownOn'] ?? false,
      seatUpLimit: json['seatUpLimit'] ?? json['sa_limit'] ?? false,
      seatDownLimit: json['seatDownLimit'] ?? json['da_limit'] ?? false,
      backUpLimit: json['backUpLimit'] ?? json['se_limit'] ?? false,
      backDownLimit: json['backDownLimit'] ?? json['de_limit'] ?? false,
      legUpLimit: json['legUpLimit'] ?? json['sp_limit'] ?? false,
      legDownLimit: json['legDownLimit'] ?? json['dp_limit'] ?? false,
      backPosition: json['backPosition'] ?? json['encosto_pos'] ?? 0,
      seatPosition: json['seatPosition'] ?? json['assento_pos'] ?? 0,
      legPosition: json['legPosition'] ?? json['perneira_pos'] ?? 0,
      wifiConnected: json['wifiConnected'] ?? json['wifi'] ?? false,
      wifiSsid: json['wifiSsid'] ?? json['wifi_ssid'],
      wifiIp: json['wifiIp'],
      hourMeter: json['hourMeter']?.toDouble() ?? json['horimetro']?.toDouble(),
      enabled: json['enabled'] ?? json['habilitada'] ?? true,
      maintenanceRequired:
          json['maintenanceRequired'] ?? json['manutencao'] ?? false,
      shouldStopAllTimers: json['shouldStopAllTimers'] ?? false,
      isMovingToGineco: json['isMovingToGineco'] ?? false,
      isMovingToParto: json['isMovingToParto'] ?? false,
    );
  }
}

class ChairCommand {
  // Comandos de movimento
  static const String backUp = 'SE'; // Sentar (encosto sobe)
  static const String backDown = 'DE'; // Deitar (encosto desce)
  static const String seatUp = 'SA'; // Assento sobe
  static const String seatDown = 'DA'; // Assento desce
  static const String upperLegs = 'SP'; // Pernas sobem
  static const String lowerLegs = 'DP'; // Pernas descem

  // Posições pré-definidas
  static const String gynecologicalPosition =
      'VZ'; // Volta zero (posição inicial)
  static const String birthPosition = 'PT'; // Posição de trabalho
  static const String memoryPosition1 = 'M1'; // Memória 1

  // Controles gerais
  static const String reflector = 'RF'; // Liga/desliga refletor
  static const String stopAll = 'AT_SEG'; // Parada de emergência
  static const String stop = 'STOP'; // Comando alternativo de parada

  // Comandos de consulta
  static const String status = 'STATUS'; // Solicita status completo JSON
  static const String hourMeter = 'HORIMETRO'; // Solicita horímetro
  static const String ping = 'PING'; // Teste de conexão
}
