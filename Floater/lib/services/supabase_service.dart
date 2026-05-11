import 'package:flutter/foundation.dart';
import 'package:flutter/scheduler.dart';
import 'package:supabase_flutter/supabase_flutter.dart';

/// Serviço para gerenciar a conexão e operações com Supabase
class SupabaseService extends ChangeNotifier {
  static SupabaseService? _instance;

  // Configurações do Supabase
  static const String _supabaseUrl = 'https://mkoqceekhnkpviixqnnk.supabase.co';
  static const String _supabaseAnonKey =
      'sb_publishable_HLUfLEw2UuIWjzd5LfqLkw_oaodzV7V';

  bool _isInitialized = false;
  bool _isConnected = false;
  String? _errorMessage;

  // Getters
  bool get isInitialized => _isInitialized;
  bool get isConnected => _isConnected;
  String? get errorMessage => _errorMessage;
  SupabaseClient get client => Supabase.instance.client;

  // Singleton
  static SupabaseService get instance {
    _instance ??= SupabaseService._();
    return _instance!;
  }

  SupabaseService._();

  /// Inicializa o Supabase
  static Future<void> initialize() async {
    if (_supabaseUrl == 'SUA_SUPABASE_URL' ||
        _supabaseAnonKey == 'SUA_SUPABASE_ANON_KEY') {
      debugPrint(
        '⚠️ ATENÇÃO: Configure as credenciais do Supabase em supabase_service.dart',
      );
      debugPrint(
        '   _supabaseUrl e _supabaseAnonKey precisam ser configurados',
      );
      return;
    }

    try {
      await Supabase.initialize(url: _supabaseUrl, anonKey: _supabaseAnonKey);
      instance._isInitialized = true;
      instance._isConnected = true;
      instance._errorMessage = null;
      debugPrint('✅ Supabase inicializado com sucesso');
    } catch (e) {
      instance._errorMessage = e.toString();
      instance._isInitialized = false;
      instance._isConnected = false;
      debugPrint('❌ Erro ao inicializar Supabase: $e');
    }
    SchedulerBinding.instance.addPostFrameCallback((_) {
      instance.notifyListeners();
    });
  }

  // ========== AUTENTICAÇÃO ==========

  /// Retorna o usuário atual (se logado)
  User? get currentUser => _isInitialized ? client.auth.currentUser : null;

  /// Verifica se há usuário logado
  bool get isLoggedIn => currentUser != null;

  /// Login com email e senha
  Future<AuthResponse?> signInWithEmail(String email, String password) async {
    if (!_isInitialized) return null;

    try {
      final response = await client.auth.signInWithPassword(
        email: email,
        password: password,
      );
      notifyListeners();
      return response;
    } catch (e) {
      _errorMessage = e.toString();
      notifyListeners();
      rethrow;
    }
  }

  /// Cadastro com email e senha
  Future<AuthResponse?> signUpWithEmail(String email, String password) async {
    if (!_isInitialized) return null;

    try {
      final response = await client.auth.signUp(
        email: email,
        password: password,
      );
      notifyListeners();
      return response;
    } catch (e) {
      _errorMessage = e.toString();
      notifyListeners();
      rethrow;
    }
  }

  /// Logout
  Future<void> signOut() async {
    if (!_isInitialized) return;

    try {
      await client.auth.signOut();
      notifyListeners();
    } catch (e) {
      _errorMessage = e.toString();
      notifyListeners();
      rethrow;
    }
  }

  // ========== OPERAÇÕES COM BANCO DE DADOS ==========

  /// Insere um registro em uma tabela
  Future<Map<String, dynamic>?> insert(
    String table,
    Map<String, dynamic> data,
  ) async {
    if (!_isInitialized) return null;

    try {
      final response = await client.from(table).insert(data).select().single();
      return response;
    } catch (e) {
      _errorMessage = e.toString();
      debugPrint('❌ Erro ao inserir em $table: $e');
      rethrow;
    }
  }

  /// Busca registros de uma tabela
  Future<List<Map<String, dynamic>>> select(
    String table, {
    String? columns,
    Map<String, dynamic>? filters,
    String? orderBy,
    bool ascending = true,
    int? limit,
  }) async {
    if (!_isInitialized) return [];

    try {
      // Construir a query de forma encadeada
      var baseQuery = client.from(table).select(columns ?? '*');

      // Aplicar filtros
      if (filters != null) {
        for (var entry in filters.entries) {
          baseQuery = baseQuery.eq(entry.key, entry.value);
        }
      }

      // Aplicar ordenação e limite
      PostgrestTransformBuilder<PostgrestList> finalQuery;

      if (orderBy != null && limit != null) {
        finalQuery = baseQuery
            .order(orderBy, ascending: ascending)
            .limit(limit);
      } else if (orderBy != null) {
        finalQuery = baseQuery.order(orderBy, ascending: ascending);
      } else if (limit != null) {
        finalQuery = baseQuery.limit(limit);
      } else {
        final response = await baseQuery;
        return List<Map<String, dynamic>>.from(response);
      }

      final response = await finalQuery;
      return List<Map<String, dynamic>>.from(response);
    } catch (e) {
      _errorMessage = e.toString();
      debugPrint('❌ Erro ao buscar em $table: $e');
      rethrow;
    }
  }

  /// Atualiza registros em uma tabela
  Future<List<Map<String, dynamic>>> update(
    String table,
    Map<String, dynamic> data, {
    required Map<String, dynamic> match,
  }) async {
    if (!_isInitialized) return [];

    try {
      var query = client.from(table).update(data);

      match.forEach((key, value) {
        query = query.eq(key, value);
      });

      final response = await query.select();
      return List<Map<String, dynamic>>.from(response);
    } catch (e) {
      _errorMessage = e.toString();
      debugPrint('❌ Erro ao atualizar em $table: $e');
      rethrow;
    }
  }

  /// Deleta registros de uma tabela
  Future<void> delete(
    String table, {
    required Map<String, dynamic> match,
  }) async {
    if (!_isInitialized) return;

    try {
      var query = client.from(table).delete();

      match.forEach((key, value) {
        query = query.eq(key, value);
      });

      await query;
    } catch (e) {
      _errorMessage = e.toString();
      debugPrint('❌ Erro ao deletar em $table: $e');
      rethrow;
    }
  }

  // ========== OPERAÇÕES ESPECÍFICAS DA CADEIRA ==========

  /// Salva um log de uso da cadeira
  Future<void> logChairUsage({
    required String action,
    String? deviceId,
    Map<String, dynamic>? metadata,
  }) async {
    if (!_isInitialized) return;

    try {
      await insert('chair_usage_logs', {
        'action': action,
        'device_id': deviceId,
        'user_id': currentUser?.id,
        'metadata': metadata,
        'created_at': DateTime.now().toIso8601String(),
      });
    } catch (e) {
      debugPrint('⚠️ Erro ao salvar log de uso: $e');
    }
  }

  /// Salva configurações da cadeira no banco
  Future<void> saveChairSettings({
    required String settingsName,
    required Map<String, dynamic> settings,
  }) async {
    if (!_isInitialized || currentUser == null) return;

    await insert('chair_settings', {
      'user_id': currentUser!.id,
      'name': settingsName,
      'settings': settings,
      'created_at': DateTime.now().toIso8601String(),
    });
  }

  /// Busca configurações salvas do usuário
  Future<List<Map<String, dynamic>>> getChairSettings() async {
    if (!_isInitialized || currentUser == null) return [];

    return await select(
      'chair_settings',
      filters: {'user_id': currentUser!.id},
      orderBy: 'created_at',
      ascending: false,
    );
  }

  /// Salva posição de memória
  Future<void> saveMemoryPosition({
    required int memorySlot,
    required double backAngle,
    required double seatHeight,
    required double legRestAngle,
  }) async {
    if (!_isInitialized) return;

    final userId = currentUser?.id ?? 'anonymous';

    // Verifica se já existe uma posição para este slot
    final existing = await select(
      'memory_positions',
      filters: {'user_id': userId, 'slot': memorySlot},
      limit: 1,
    );

    if (existing.isNotEmpty) {
      // Atualiza posição existente
      await update(
        'memory_positions',
        {
          'back_angle': backAngle,
          'seat_height': seatHeight,
          'leg_rest_angle': legRestAngle,
          'updated_at': DateTime.now().toIso8601String(),
        },
        match: {'id': existing.first['id']},
      );
    } else {
      // Cria nova posição
      await insert('memory_positions', {
        'user_id': userId,
        'slot': memorySlot,
        'back_angle': backAngle,
        'seat_height': seatHeight,
        'leg_rest_angle': legRestAngle,
        'created_at': DateTime.now().toIso8601String(),
      });
    }
  }

  /// Carrega posição de memória
  Future<Map<String, dynamic>?> loadMemoryPosition(int memorySlot) async {
    if (!_isInitialized) return null;

    final userId = currentUser?.id ?? 'anonymous';

    final results = await select(
      'memory_positions',
      filters: {'user_id': userId, 'slot': memorySlot},
      limit: 1,
    );

    return results.isNotEmpty ? results.first : null;
  }

  // ========== CONTROLE DA CADEIRA - HORÍMETRO E HABILITAÇÃO ==========

  /// Status da cadeira carregado do banco
  bool _chairEnabled = true;
  bool _maintenanceRequired = false;
  double _maintenanceHours = 0;
  String? _chairSerialNumber;
  DateTime? _lastStatusCheck;

  // Getters para status da cadeira
  bool get isChairEnabled => _chairEnabled;
  bool get isMaintenanceRequired => _maintenanceRequired;
  double get maintenanceHours => _maintenanceHours;
  String? get chairSerialNumber => _chairSerialNumber;
  DateTime? get lastStatusCheck => _lastStatusCheck;

  /// Define o número de série da cadeira
  void setChairSerialNumber(String serialNumber) {
    _chairSerialNumber = serialNumber;
    SchedulerBinding.instance.addPostFrameCallback((_) {
      notifyListeners();
    });
  }

  /// Envia o horímetro (horas de uso) para o Supabase
  Future<bool> sendHourMeter(double hours) async {
    if (!_isInitialized || _chairSerialNumber == null) {
      debugPrint(
        '⚠️ Supabase não inicializado ou número de série não definido',
      );
      return false;
    }

    try {
      // Busca o registro existente da cadeira
      final existing = await select(
        'chairs',
        filters: {'serial_number': _chairSerialNumber},
        limit: 1,
      );

      if (existing.isNotEmpty) {
        // Atualiza o horímetro
        await update(
          'chairs',
          {
            'hour_meter': hours,
            'last_update': DateTime.now().toIso8601String(),
          },
          match: {'serial_number': _chairSerialNumber},
        );
        debugPrint('✅ Horímetro enviado: $hours horas');
        return true;
      } else {
        // Cadeira não registrada no banco
        debugPrint('❌ Cadeira $_chairSerialNumber não encontrada no banco');
        return false;
      }
    } catch (e) {
      debugPrint('❌ Erro ao enviar horímetro: $e');
      return false;
    }
  }

  /// Verifica o status da cadeira (habilitada, manutenção, etc.)
  /// Retorna true se a cadeira está liberada para uso
  Future<bool> checkChairStatus() async {
    if (!_isInitialized || _chairSerialNumber == null) {
      debugPrint(
        '⚠️ Supabase não inicializado ou número de série não definido',
      );
      // Se não conseguir verificar, assume que está habilitada (modo offline)
      return true;
    }

    try {
      final results = await select(
        'chairs',
        filters: {'serial_number': _chairSerialNumber},
        limit: 1,
      );

      if (results.isEmpty) {
        // Cadeira não encontrada no banco
        debugPrint('⚠️ Cadeira $_chairSerialNumber não registrada no banco');
        debugPrint('   Funcionando em modo offline');
        _chairEnabled = true;
        _maintenanceRequired = false;
        _maintenanceHours = 0;
      } else {
        final chair = results.first;
        _chairEnabled = chair['enabled'] ?? true;
        _maintenanceRequired = chair['maintenance_required'] ?? false;
        _maintenanceHours = (chair['maintenance_hours'] ?? 0).toDouble();

        debugPrint(
          '📊 Status da cadeira: habilitada=$_chairEnabled, manutenção=$_maintenanceRequired (${_maintenanceHours}h)',
        );
      }

      _lastStatusCheck = DateTime.now();

      // Adia notificação para evitar setState durante build
      SchedulerBinding.instance.addPostFrameCallback((_) {
        notifyListeners();
      });

      // Sempre retorna true para permitir uso (temporário)
      return true;
    } catch (e) {
      debugPrint('❌ Erro ao verificar status da cadeira: $e');
      // Em caso de erro, mantém o último estado conhecido
      return true;
    }
  }

  /// Busca informações completas da cadeira
  Future<Map<String, dynamic>?> getChairInfo() async {
    if (!_isInitialized || _chairSerialNumber == null) return null;

    try {
      final results = await select(
        'chairs',
        filters: {'serial_number': _chairSerialNumber},
        limit: 1,
      );

      return results.isNotEmpty ? results.first : null;
    } catch (e) {
      debugPrint('❌ Erro ao buscar informações da cadeira: $e');
      return null;
    }
  }

  /// Solicita manutenção para a cadeira
  Future<bool> requestMaintenance({
    required String description,
    double? currentHours,
  }) async {
    if (!_isInitialized || _chairSerialNumber == null) return false;

    try {
      // Cria pedido de manutenção
      await insert('maintenance_requests', {
        'chair_serial': _chairSerialNumber,
        'description': description,
        'hours_at_request': currentHours,
        'status': 'pending',
        'created_at': DateTime.now().toIso8601String(),
      });

      debugPrint('✅ Pedido de manutenção criado');
      return true;
    } catch (e) {
      debugPrint('❌ Erro ao solicitar manutenção: $e');
      return false;
    }
  }

  /// Busca pedidos de manutenção pendentes
  Future<List<Map<String, dynamic>>> getPendingMaintenanceRequests() async {
    if (!_isInitialized || _chairSerialNumber == null) return [];

    try {
      return await select(
        'maintenance_requests',
        filters: {'chair_serial': _chairSerialNumber, 'status': 'pending'},
        orderBy: 'created_at',
        ascending: false,
      );
    } catch (e) {
      debugPrint('❌ Erro ao buscar pedidos de manutenção: $e');
      return [];
    }
  }

  /// Atualiza o horímetro periodicamente (chamado a cada X minutos de uso)
  Future<void> updateHourMeterPeriodic(double totalHours) async {
    await sendHourMeter(totalHours);
    // Também verifica o status a cada atualização
    await checkChairStatus();
  }
}
