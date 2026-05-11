class ChairState {
  bool reflectorOn;
  bool upperLegsOn;
  bool lowerLegsOn;
  bool seatUpOn;
  bool seatDownOn;
  bool backUpOn;
  bool backDownOn;

  ChairState({
    this.reflectorOn = false,
    this.upperLegsOn = false,
    this.lowerLegsOn = false,
    this.seatUpOn = false,
    this.seatDownOn = false,
    this.backUpOn = false,
    this.backDownOn = false,
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
    };
  }

  factory ChairState.fromJson(Map<String, dynamic> json) {
    return ChairState(
      reflectorOn: json['reflectorOn'] ?? false,
      upperLegsOn: json['upperLegsOn'] ?? false,
      lowerLegsOn: json['lowerLegsOn'] ?? false,
      seatUpOn: json['seatUpOn'] ?? false,
      seatDownOn: json['seatDownOn'] ?? false,
      backUpOn: json['backUpOn'] ?? false,
      backDownOn: json['backDownOn'] ?? false,
    );
  }
}

class ChairCommand {
  static const String stopAll = 'AT_SEG';
  static const String gynecologicalPosition = 'VZ';
  static const String birthPosition = 'PT';
  static const String reflector = 'RF';
  static const String upperLegs = 'SP';
  static const String lowerLegs = 'DP';
  static const String seatUp = 'SA';
  static const String seatDown = 'DA';
  static const String backUp = 'SE';
  static const String backDown = 'DE';
}
