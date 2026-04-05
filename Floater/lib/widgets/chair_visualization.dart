import 'package:flutter/material.dart';
import 'dart:math' as math;
import 'dart:async';
import '../models/chair_state.dart';

class ChairVisualization extends StatefulWidget {
  final ChairState chairState;
  final VoidCallback? onBackUp;
  final VoidCallback? onBackDown;
  final VoidCallback? onSeatUp;
  final VoidCallback? onSeatDown;
  final VoidCallback? onLegUp;
  final VoidCallback? onLegDown;
  final VoidCallback? onAutoZero;
  final VoidCallback? onWorkPosition;
  final VoidCallback? onReflectorToggle;
  final VoidCallback? onStopBackMovement;
  final VoidCallback? onStopSeatMovement;
  final VoidCallback? onStopLegMovement;

  const ChairVisualization({
    super.key,
    required this.chairState,
    this.onBackUp,
    this.onBackDown,
    this.onSeatUp,
    this.onSeatDown,
    this.onLegUp,
    this.onLegDown,
    this.onAutoZero,
    this.onWorkPosition,
    this.onReflectorToggle,
    this.onStopBackMovement,
    this.onStopSeatMovement,
    this.onStopLegMovement,
  });

  @override
  State<ChairVisualization> createState() => _ChairVisualizationState();
}

class _ChairVisualizationState extends State<ChairVisualization> {
  final Map<String, Timer?> _timers = {};
  final Map<String, bool> _activeVisuals = {};
  final PageController _pageController = PageController();
  int _currentPage = 0;

  // Cor principal do desenho técnico
  static const Color _lineColor = Color(0xFF1565C0);
  static const Color _accentColor = Color(0xFF0D47A1);
  static const Color _backgroundColor = Color(0xFFF5F9FF);

  @override
  void dispose() {
    for (final timer in _timers.values) {
      timer?.cancel();
    }
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Container(
      width: 380,
      height: 550,
      clipBehavior: Clip.hardEdge,
      decoration: BoxDecoration(
        color: _backgroundColor,
        borderRadius: BorderRadius.circular(16),
        border: Border.all(color: _lineColor.withValues(alpha: 0.3), width: 2),
        boxShadow: [
          BoxShadow(
            color: Colors.black.withValues(alpha: 0.1),
            blurRadius: 20,
            offset: const Offset(0, 10),
          ),
        ],
      ),
      child: Stack(
        children: [
          // PageView para os desenhos
          PageView(
            controller: _pageController,
            onPageChanged: (index) {
              setState(() {
                _currentPage = index;
              });
            },
            children: [_buildFirstPage(), _buildSecondPage()],
          ),

          // Indicadores de página (Dots)
          Positioned(
            bottom: 12,
            left: 0,
            right: 0,
            child: Row(
              mainAxisAlignment: MainAxisAlignment.center,
              children: List.generate(2, (index) {
                return Container(
                  margin: const EdgeInsets.symmetric(horizontal: 4),
                  width: 8,
                  height: 8,
                  decoration: BoxDecoration(
                    shape: BoxShape.circle,
                    color: _currentPage == index
                        ? _lineColor
                        : _lineColor.withValues(alpha: 0.3),
                  ),
                );
              }),
            ),
          ),

          // Instrução de uso
          Positioned(
            top: 8,
            right: 12,
            child: Container(
              padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 4),
              decoration: BoxDecoration(
                color: Colors.orange.shade50,
                borderRadius: BorderRadius.circular(4),
                border: Border.all(color: Colors.orange.shade200),
              ),
              child: Row(
                mainAxisSize: MainAxisSize.min,
                children: [
                  Icon(
                    Icons.touch_app,
                    size: 12,
                    color: Colors.orange.shade700,
                  ),
                  const SizedBox(width: 4),
                  Text(
                    'Arraste para mudar de vista',
                    style: TextStyle(
                      color: Colors.orange.shade700,
                      fontSize: 9,
                      fontWeight: FontWeight.bold,
                    ),
                  ),
                ],
              ),
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildFirstPage() {
    return Stack(
      children: [
        // Desenho técnico da cadeira - Vista Lateral
        Center(
          child: CustomPaint(
            size: const Size(340, 420),
            painter: _ChairTechnicalDrawingPainter(
              backAngle: _getBackAngle(),
              seatHeight: _getSeatHeight(),
              legRestAngle: _getLegRestAngle(),
              isReflectorOn: widget.chairState.reflectorOn,
              backColor: _getBackIndicatorColor(),
              seatColor: _getSeatIndicatorColor(),
              legRestColor: _getLegRestIndicatorColor(),
              isBackActive:
                  widget.chairState.backUpOn || widget.chairState.backDownOn,
              isSeatActive:
                  widget.chairState.seatUpOn || widget.chairState.seatDownOn,
              isLegRestActive:
                  widget.chairState.upperLegsOn ||
                  widget.chairState.lowerLegsOn,
            ),
          ),
        ),

        // ========== ENCOSTO - DIVIDIDO EM DUAS ÁREAS ==========
        // Área FRONTAL do encosto: SENTAR (DE) - Azul
        Positioned(
          top: 50,
          left: 150,
          child: _buildTouchArea(
            width: 90,
            height: 150,
            onTap: widget.onBackUp != null
                ? () => _pulseCommand(
                    'SENTAR',
                    widget.onBackUp!,
                    widget.onStopBackMovement,
                  )
                : null,
            onLongPressStart: widget.onBackUp != null
                ? () => _startTimer('SENTAR', widget.onBackUp!)
                : null,
            onLongPressEnd: widget.onStopBackMovement != null
                ? () => _stopTimer('SENTAR', widget.onStopBackMovement)
                : null,
            isActive: _activeVisuals['SENTAR'] ?? false,
            activeColor: Colors.blue.shade600,
            inactiveColor: Colors.blue.withValues(alpha: 0.15),
            label: 'SENTAR',
            icon: Icons.event_seat,
          ),
        ),

        // Área TRASEIRA do encosto: DEITAR (SE) - Laranja (na traseira)
        Positioned(
          top: 50,
          left: 10,
          child: _buildTouchArea(
            width: 115,
            height: 250,
            onTap: widget.onBackDown != null
                ? () => _pulseCommand(
                    'DEITAR',
                    widget.onBackDown!,
                    widget.onStopBackMovement,
                  )
                : null,
            onLongPressStart: widget.onBackDown != null
                ? () => _startTimer('DEITAR', widget.onBackDown!)
                : null,
            onLongPressEnd: widget.onStopBackMovement != null
                ? () => _stopTimer('DEITAR', widget.onStopBackMovement)
                : null,
            isActive: _activeVisuals['DEITAR'] ?? false,
            activeColor: Colors.orange.shade600,
            inactiveColor: Colors.orange.withValues(alpha: 0.15),
            label: 'DEITAR',
            icon: Icons.airline_seat_flat,
          ),
        ),

        // ========== ASSENTO - DIVIDIDO EM DUAS ÁREAS VERTICAIS ==========
        // Área SUPERIOR do assento: SUBIR (SA) - Verde
        Positioned(
          top: 210,
          left: 160,
          child: _buildTouchArea(
            width: 75,
            height: 75,
            onTap: widget.onSeatUp != null
                ? () => _pulseCommand(
                    'ALTO',
                    widget.onSeatUp!,
                    widget.onStopSeatMovement,
                  )
                : null,
            onLongPressStart: widget.onSeatUp != null
                ? () => _startTimer('ALTO', widget.onSeatUp!)
                : null,
            onLongPressEnd: widget.onStopSeatMovement != null
                ? () => _stopTimer('ALTO', widget.onStopSeatMovement)
                : null,
            isActive: _activeVisuals['ALTO'] ?? false,
            activeColor: Colors.green.shade600,
            inactiveColor: Colors.green.withValues(alpha: 0.15),
            label: 'ALTO',
            icon: Icons.arrow_upward,
            fontSize: 8,
          ),
        ),

        // Área INFERIOR do assento: DESCER (DA) - Vermelho
        Positioned(
          top: 310,
          left: 145,
          child: _buildTouchArea(
            width: 80,
            height: 95,
            onTap: widget.onSeatDown != null
                ? () => _pulseCommand(
                    'BAIXO',
                    widget.onSeatDown!,
                    widget.onStopSeatMovement,
                  )
                : null,
            onLongPressStart: widget.onSeatDown != null
                ? () => _startTimer('BAIXO', widget.onSeatDown!)
                : null,
            onLongPressEnd: widget.onStopSeatMovement != null
                ? () => _stopTimer('BAIXO', widget.onStopSeatMovement)
                : null,
            isActive: _activeVisuals['BAIXO'] ?? false,
            activeColor: Colors.red.shade600,
            inactiveColor: Colors.red.withValues(alpha: 0.15),
            label: 'BAIXO',
            icon: Icons.arrow_downward,
            fontSize: 8,
          ),
        ),

        // ========== PERNAS - DIVIDIDO EM DUAS ÁREAS ==========
        // Área ESQUERDA das pernas: DESCER (DP) - Roxo
        Positioned(
          top: 350,
          right: 19,
          child: _buildTouchArea(
            width: 65,
            height: 150,
            onTap: widget.onLegDown != null
                ? () => _pulseCommand(
                    'BAIXAR',
                    widget.onLegDown!,
                    widget.onStopLegMovement,
                  )
                : null,
            onLongPressStart: widget.onLegDown != null
                ? () => _startTimer('BAIXAR', widget.onLegDown!)
                : null,
            onLongPressEnd: widget.onStopLegMovement != null
                ? () => _stopTimer('BAIXAR', widget.onStopLegMovement)
                : null,
            isActive: _activeVisuals['BAIXAR'] ?? false,
            activeColor: Colors.purple.shade600,
            inactiveColor: Colors.purple.withValues(alpha: 0.15),
            label: 'BAIXAR',
            icon: Icons.arrow_downward,
          ),
        ),

        // Área DIREITA das pernas: SUBIR (SP) - Verde-água
        Positioned(
          top: 195,
          right: 19,
          child: _buildTouchArea(
            width: 65,
            height: 150,
            onTap: widget.onLegUp != null
                ? () => _pulseCommand(
                    'LEVANTAR',
                    widget.onLegUp!,
                    widget.onStopLegMovement,
                  )
                : null,
            onLongPressStart: widget.onLegUp != null
                ? () => _startTimer('LEVANTAR', widget.onLegUp!)
                : null,
            onLongPressEnd: widget.onStopLegMovement != null
                ? () => _stopTimer('LEVANTAR', widget.onStopLegMovement)
                : null,
            isActive: _activeVisuals['LEVANTAR'] ?? false,
            activeColor: Colors.teal.shade600,
            inactiveColor: Colors.teal.withValues(alpha: 0.15),
            label: 'LEVANTAR',
            icon: Icons.arrow_upward,
          ),
        ),
      ],
    );
  }

  Widget _buildSecondPage() {
    return Stack(
      children: [
        // Desenho técnico do Pedal - Vista Superior (Novo Formato)
        Center(
          child: CustomPaint(
            size: const Size(320, 320),
            painter: _PedalTechnicalDrawingPainter(
              backUpOn: widget.chairState.backUpOn == true,
              backDownOn: widget.chairState.backDownOn == true,
              seatUpOn: widget.chairState.seatUpOn == true,
              seatDownOn: widget.chairState.seatDownOn == true,
              legUpOn: widget.chairState.upperLegsOn == true,
              legDownOn: widget.chairState.lowerLegsOn == true,
              autoZeroOn: widget.chairState.isMovingToGineco == true,
              workPosOn: widget.chairState.isMovingToParto == true,
            ),
          ),
        ),

        // ========== CONTROLES DO PEDAL (Mapeados conforme a imagem) ==========

        // --- Lado Esquerdo: P (Perneira) ---
        // P Cima (Levantar)
        Positioned(
          top: 215, // Centralizado no botão (Y=250 - 35)
          left: 60, // Centralizado no botão (X=95 - 35)
          child: _buildTouchArea(
            width: 70,
            height: 70,
            onTap: widget.onLegUp != null
                ? () => _pulseCommand(
                    'PEDAL_P_UP',
                    widget.onLegUp!,
                    widget.onStopLegMovement,
                  )
                : null,
            onLongPressStart: widget.onLegUp != null
                ? () => _startTimer('PEDAL_P_UP', widget.onLegUp!)
                : null,
            onLongPressEnd: widget.onStopLegMovement != null
                ? () => _stopTimer('PEDAL_P_UP', widget.onStopLegMovement)
                : null,
            isActive: _activeVisuals['PEDAL_P_UP'] ?? false,
            activeColor: Colors.blue.shade600,
            inactiveColor: Colors.transparent,
            label: '',
            icon: Icons.circle,
          ),
        ),
        // P Baixo (Baixar)
        Positioned(
          top: 315, // Centralizado no botão (Y=350 - 35)
          left: 60,
          child: _buildTouchArea(
            width: 70,
            height: 70,
            onTap: widget.onLegDown != null
                ? () => _pulseCommand(
                    'PEDAL_P_DOWN',
                    widget.onLegDown!,
                    widget.onStopLegMovement,
                  )
                : null,
            onLongPressStart: widget.onLegDown != null
                ? () => _startTimer('PEDAL_P_DOWN', widget.onLegDown!)
                : null,
            onLongPressEnd: widget.onStopLegMovement != null
                ? () => _stopTimer('PEDAL_P_DOWN', widget.onStopLegMovement)
                : null,
            isActive: _activeVisuals['PEDAL_P_DOWN'] ?? false,
            activeColor: Colors.blue.shade600,
            inactiveColor: Colors.transparent,
            label: '',
            icon: Icons.circle,
          ),
        ),

        // --- Lado Direito: E (Encosto) ---
        // E Cima (Sentar/Subir)
        Positioned(
          top: 215,
          left: 250, // Centralizado no botão (X=285 - 35)
          child: _buildTouchArea(
            width: 70,
            height: 70,
            onTap: widget.onBackUp != null
                ? () => _pulseCommand(
                    'PEDAL_E_UP',
                    widget.onBackUp!,
                    widget.onStopBackMovement,
                  )
                : null,
            onLongPressStart: widget.onBackUp != null
                ? () => _startTimer('PEDAL_E_UP', widget.onBackUp!)
                : null,
            onLongPressEnd: widget.onStopBackMovement != null
                ? () => _stopTimer('PEDAL_E_UP', widget.onStopBackMovement)
                : null,
            isActive: _activeVisuals['PEDAL_E_UP'] ?? false,
            activeColor: Colors.blue.shade600,
            inactiveColor: Colors.transparent,
            label: '',
            icon: Icons.circle,
          ),
        ),
        // E Baixo (Deitar/Descer)
        Positioned(
          top: 315,
          left: 250,
          child: _buildTouchArea(
            width: 70,
            height: 70,
            onTap: widget.onBackDown != null
                ? () => _pulseCommand(
                    'PEDAL_E_DOWN',
                    widget.onBackDown!,
                    widget.onStopBackMovement,
                  )
                : null,
            onLongPressStart: widget.onBackDown != null
                ? () => _startTimer('PEDAL_E_DOWN', widget.onBackDown!)
                : null,
            onLongPressEnd: widget.onStopBackMovement != null
                ? () => _stopTimer('PEDAL_E_DOWN', widget.onStopBackMovement)
                : null,
            isActive: _activeVisuals['PEDAL_E_DOWN'] ?? false,
            activeColor: Colors.blue.shade600,
            inactiveColor: Colors.transparent,
            label: '',
            icon: Icons.circle,
          ),
        ),

        // --- Centro Topo: AUTOMÁTICO ---
        // Auto Esquerda (Volta a Zero)
        Positioned(
          top: 215,
          left: 120, // Centralizado no botão (X=155 - 35)
          child: _buildTouchArea(
            width: 70,
            height: 70,
            onTap: widget.onAutoZero != null
                ? () => _pulseCommand('PEDAL_AUTO_L', widget.onAutoZero!, null)
                : null,
            onLongPressStart: null,
            onLongPressEnd: null,
            isActive: _activeVisuals['PEDAL_AUTO_L'] ?? false,
            activeColor: Colors.blue.shade600,
            inactiveColor: Colors.transparent,
            label: '',
            icon: Icons.circle,
          ),
        ),
        // Auto Direita (Trabalho)
        Positioned(
          top: 215,
          left: 190, // Centralizado no botão (X=225 - 35)
          child: _buildTouchArea(
            width: 70,
            height: 70,
            onTap: widget.onWorkPosition != null
                ? () => _pulseCommand(
                    'PEDAL_AUTO_R',
                    widget.onWorkPosition!,
                    null,
                  )
                : null,
            onLongPressStart: null,
            onLongPressEnd: null,
            isActive: _activeVisuals['PEDAL_AUTO_R'] ?? false,
            activeColor: Colors.blue.shade600,
            inactiveColor: Colors.transparent,
            label: '',
            icon: Icons.circle,
          ),
        ),

        // --- Centro Baixo: A (Assento) ---
        // A Esquerda (Subir)
        Positioned(
          top: 335, // Centralizado no botão (Y=370 - 35)
          left: 120,
          child: _buildTouchArea(
            width: 70,
            height: 70,
            onTap: widget.onSeatUp != null
                ? () => _pulseCommand(
                    'PEDAL_A_UP',
                    widget.onSeatUp!,
                    widget.onStopSeatMovement,
                  )
                : null,
            onLongPressStart: widget.onSeatUp != null
                ? () => _startTimer('PEDAL_A_UP', widget.onSeatUp!)
                : null,
            onLongPressEnd: widget.onStopSeatMovement != null
                ? () => _stopTimer('PEDAL_A_UP', widget.onStopSeatMovement)
                : null,
            isActive: _activeVisuals['PEDAL_A_UP'] ?? false,
            activeColor: Colors.blue.shade600,
            inactiveColor: Colors.transparent,
            label: '',
            icon: Icons.circle,
          ),
        ),
        // A Direita (Descer)
        Positioned(
          top: 335,
          left: 190,
          child: _buildTouchArea(
            width: 70,
            height: 70,
            onTap: widget.onSeatDown != null
                ? () => _pulseCommand(
                    'PEDAL_A_DOWN',
                    widget.onSeatDown!,
                    widget.onStopSeatMovement,
                  )
                : null,
            onLongPressStart: widget.onSeatDown != null
                ? () => _startTimer('PEDAL_A_DOWN', widget.onSeatDown!)
                : null,
            onLongPressEnd: widget.onStopSeatMovement != null
                ? () => _stopTimer('PEDAL_A_DOWN', widget.onStopSeatMovement)
                : null,
            isActive: _activeVisuals['PEDAL_A_DOWN'] ?? false,
            activeColor: Colors.blue.shade600,
            inactiveColor: Colors.transparent,
            label: '',
            icon: Icons.circle,
          ),
        ),
      ],
    );
  }

  void _startTimer(String label, VoidCallback action) {
    debugPrint('🟢 Iniciando timer para $label');
    _timers[label]?.cancel();
    _activeVisuals[label] = true;
    setState(() {});
    action(); // Execute immediately
    _timers[label] = Timer.periodic(const Duration(milliseconds: 200), (_) {
      final shouldStop = widget.chairState.shouldStopAllTimers;
      if (shouldStop) {
        debugPrint('🛑 UI Timer: Parando por limite atingido');
        _stopTimer(label);
        return;
      }
      action();
    });
  }

  void _pulseCommand(
    String label,
    VoidCallback action,
    VoidCallback? stopAction,
  ) {
    debugPrint('⚡ Pulso de comando para $label');
    _timers[label]?.cancel();
    _timers[label] = null;
    _activeVisuals[label] = true;
    setState(() {});
    action();

    _timers[label] = Timer(const Duration(milliseconds: 200), () {
      if (!mounted) return;
      _stopTimer(label, stopAction);
    });
  }

  void _stopTimer(String label, [VoidCallback? stopAction]) {
    debugPrint('🔴 Parando timer para $label');
    _timers[label]?.cancel();
    _timers[label] = null;
    _activeVisuals[label] = false;
    if (mounted) {
      setState(() {});
    }
    stopAction?.call();
  }

  double _getBackAngle() {
    if (widget.chairState.backUpOn == true) {
      return 100; // Sentado (95° visual)
    } else if (widget.chairState.backDownOn == true) {
      return 85; // Deitado (80° visual)
    }
    return 90; // Vertical
  }

  double _getSeatHeight() {
    if (widget.chairState.seatUpOn == true) {
      return 75; // Alto
    } else if (widget.chairState.seatDownOn == true) {
      return 45; // Baixo
    }
    return 60; // Normal
  }

  Color _getBackIndicatorColor() {
    if (widget.chairState.backUpOn == true) {
      return Colors.orange.shade600; // SENTAR
    }
    if (widget.chairState.backDownOn == true) {
      return Colors.blue.shade600; // DEITAR
    }
    return _accentColor;
  }

  Color _getSeatIndicatorColor() {
    if (widget.chairState.seatUpOn == true) {
      return Colors.green.shade600;
    }
    if (widget.chairState.seatDownOn == true) {
      return Colors.red.shade600;
    }
    return _accentColor;
  }

  double _getLegRestAngle() {
    if (widget.chairState.upperLegsOn == true) {
      return 30; // Subindo em direção à horizontal
    } else if (widget.chairState.lowerLegsOn == true) {
      return 60; // Descendo em direção à vertical
    }
    return 45; // Posição inicial: 45 graus para fora (lado contrário ao anterior)
  }

  Color _getLegRestIndicatorColor() {
    if (widget.chairState.upperLegsOn == true) {
      return Colors.teal.shade600;
    }
    if (widget.chairState.lowerLegsOn == true) {
      return Colors.purple.shade600;
    }
    return _accentColor;
  }

  // Nova área de toque visível com cor de fundo
  Widget _buildTouchArea({
    required double width,
    required double height,
    required VoidCallback? onTap,
    required VoidCallback? onLongPressStart,
    required VoidCallback? onLongPressEnd,
    required bool isActive,
    required Color activeColor,
    required Color inactiveColor,
    required String label,
    required IconData icon,
    double fontSize = 10,
  }) {
    return _TouchAreaButton(
      key: ValueKey(label),
      width: width,
      height: height,
      onTap: onTap,
      onLongPressStart: onLongPressStart,
      onLongPressEnd: onLongPressEnd,
      isActive: isActive,
      activeColor: activeColor,
      inactiveColor: inactiveColor,
      label: label,
      icon: icon,
      fontSize: fontSize,
    );
  }
}

// Painter para o desenho técnico da cadeira
class _ChairTechnicalDrawingPainter extends CustomPainter {
  final double backAngle;
  final double seatHeight;
  final double legRestAngle;
  final bool isReflectorOn;
  final Color backColor;
  final Color seatColor;
  final Color legRestColor;
  final bool isBackActive;
  final bool isSeatActive;
  final bool isLegRestActive;

  _ChairTechnicalDrawingPainter({
    required this.backAngle,
    required this.seatHeight,
    required this.legRestAngle,
    required this.isReflectorOn,
    required this.backColor,
    required this.seatColor,
    required this.legRestColor,
    required this.isBackActive,
    required this.isSeatActive,
    required this.isLegRestActive,
  });

  @override
  void paint(Canvas canvas, Size size) {
    final centerX = size.width / 2;
    final baseY = size.height - 60;

    // Configurações de pintura
    final linePaint = Paint()
      ..color = const Color(0xFF1565C0)
      ..strokeWidth = 2.5
      ..style = PaintingStyle.stroke
      ..strokeCap = StrokeCap.round
      ..strokeJoin = StrokeJoin.round;

    final thinPaint = Paint()
      ..color = const Color(0xFF1565C0).withValues(alpha: 0.6)
      ..strokeWidth = 1.0
      ..style = PaintingStyle.stroke;

    final fillPaint = Paint()
      ..color = const Color(0xFF1565C0).withValues(alpha: 0.08)
      ..style = PaintingStyle.fill;

    final accentFillPaint = Paint()
      ..color = const Color(0xFF1565C0).withValues(alpha: 0.15)
      ..style = PaintingStyle.fill;

    // Calcular altura baseada no seatHeight (normalizado)
    final heightOffset = (60 - seatHeight) * 0.8;

    // Ponto de pivô do assento
    final seatPivotX = centerX - 20;
    final seatPivotY = baseY - 140 + heightOffset;

    // ========== BASE E COLUNA ==========

    // Base no chão (vista lateral - formato T)
    final basePath = Path();
    basePath.moveTo(centerX - 80, baseY);
    basePath.lineTo(centerX + 80, baseY);
    basePath.lineTo(centerX + 80, baseY - 12);
    basePath.lineTo(centerX + 25, baseY - 12);
    basePath.lineTo(centerX + 25, baseY - 25);
    basePath.lineTo(centerX - 25, baseY - 25);
    basePath.lineTo(centerX - 25, baseY - 12);
    basePath.lineTo(centerX - 80, baseY - 12);
    basePath.close();

    canvas.drawPath(basePath, fillPaint);
    canvas.drawPath(basePath, linePaint);

    // Rodas (círculos nas extremidades da base)
    _drawWheel(canvas, Offset(centerX - 70, baseY - 6), linePaint, fillPaint);
    _drawWheel(canvas, Offset(centerX + 70, baseY - 6), linePaint, fillPaint);

    // Coluna hidráulica
    final columnPath = Path();
    columnPath.moveTo(centerX - 12, baseY - 25);
    columnPath.lineTo(centerX - 12, seatPivotY + 20);
    columnPath.lineTo(centerX + 12, seatPivotY + 20);
    columnPath.lineTo(centerX + 12, baseY - 25);
    columnPath.close();

    canvas.drawPath(columnPath, accentFillPaint);
    canvas.drawPath(columnPath, linePaint);

    // Detalhes da coluna hidráulica (anéis)
    for (int i = 0; i < 4; i++) {
      final ringY = baseY - 35 - (i * 20);
      if (ringY > seatPivotY + 25) {
        canvas.drawLine(
          Offset(centerX - 12, ringY),
          Offset(centerX + 12, ringY),
          thinPaint,
        );
      }
    }

    // ========== ASSENTO ==========

    // Cor do assento baseada no estado - alpha maior quando ativo
    final seatAlpha = isSeatActive ? 0.45 : 0.12;
    final seatFillPaint = Paint()
      ..color = seatColor.withValues(alpha: seatAlpha)
      ..style = PaintingStyle.fill;

    final seatLinePaint = Paint()
      ..color = seatColor
      ..strokeWidth = isSeatActive ? 4.0 : 2.5
      ..style = PaintingStyle.stroke
      ..strokeCap = StrokeCap.round;

    // Assento (retângulo com cantos arredondados - vista lateral)
    final seatWidth = 130.0;
    final seatThickness = 25.0;

    final seatLeft = seatPivotX - 30;
    final seatRight = seatLeft + seatWidth;
    final seatTop = seatPivotY;
    final seatBottom = seatPivotY + seatThickness;

    // Desenhar assento com formato anatômico
    final seatPath = Path();
    seatPath.moveTo(seatLeft + 10, seatTop);
    seatPath.quadraticBezierTo(seatLeft, seatTop, seatLeft, seatTop + 10);
    seatPath.lineTo(seatLeft, seatBottom - 5);
    seatPath.quadraticBezierTo(seatLeft, seatBottom, seatLeft + 10, seatBottom);
    seatPath.lineTo(seatRight - 15, seatBottom);
    seatPath.quadraticBezierTo(
      seatRight,
      seatBottom,
      seatRight,
      seatBottom - 10,
    );
    seatPath.lineTo(seatRight, seatTop + 8);
    seatPath.quadraticBezierTo(seatRight, seatTop, seatRight - 15, seatTop);
    seatPath.close();

    canvas.drawPath(seatPath, seatFillPaint);
    canvas.drawPath(seatPath, seatLinePaint);

    // Detalhe do estofado do assento
    canvas.drawLine(
      Offset(seatLeft + 15, seatTop + 5),
      Offset(seatRight - 20, seatTop + 5),
      thinPaint,
    );

    // ========== ENCOSTO ==========

    // Cor do encosto baseada no estado - alpha maior quando ativo
    final backAlpha = isBackActive ? 0.45 : 0.12;
    final backFillPaint = Paint()
      ..color = backColor.withValues(alpha: backAlpha)
      ..style = PaintingStyle.fill;

    final backLinePaint = Paint()
      ..color = backColor
      ..strokeWidth = isBackActive ? 4.0 : 2.5
      ..style = PaintingStyle.stroke
      ..strokeCap = StrokeCap.round;

    // Calcular ângulo do encosto (convertido para radianos)
    final backAngleRad = (180 - backAngle) * math.pi / 180;

    // Ponto de conexão encosto-assento
    final backPivotX = seatLeft + 15;
    final backPivotY = seatTop;

    // Comprimento do encosto
    final backLength = 160.0;
    final backWidth = 22.0;

    // Calcular pontos do encosto
    final backEndX = backPivotX + backLength * math.cos(backAngleRad);
    final backEndY = backPivotY - backLength * math.sin(backAngleRad);

    // Vetor perpendicular para largura do encosto
    final perpX = -math.sin(backAngleRad) * backWidth / 2;
    final perpY = -math.cos(backAngleRad) * backWidth / 2;

    final backPath = Path();

    // Base do encosto (mais larga)
    backPath.moveTo(backPivotX - perpX * 1.2, backPivotY - perpY * 1.2);
    backPath.lineTo(backPivotX + perpX * 1.2, backPivotY + perpY * 1.2);

    // Lado do encosto
    backPath.quadraticBezierTo(
      backPivotX + perpX + (backEndX - backPivotX) * 0.3,
      backPivotY + perpY + (backEndY - backPivotY) * 0.3,
      backEndX + perpX * 0.7,
      backEndY + perpY * 0.7,
    );

    // Topo do encosto (arredondado)
    backPath.quadraticBezierTo(
      backEndX,
      backEndY - 8,
      backEndX - perpX * 0.7,
      backEndY - perpY * 0.7,
    );

    // Outro lado do encosto
    backPath.quadraticBezierTo(
      backPivotX - perpX + (backEndX - backPivotX) * 0.3,
      backPivotY - perpY + (backEndY - backPivotY) * 0.3,
      backPivotX - perpX * 1.2,
      backPivotY - perpY * 1.2,
    );

    backPath.close();

    canvas.drawPath(backPath, backFillPaint);
    canvas.drawPath(backPath, backLinePaint);

    // Linha central do encosto (costura)
    canvas.drawLine(
      Offset(backPivotX, backPivotY),
      Offset(backEndX, backEndY),
      thinPaint,
    );

    // ========== APOIO DE CABEÇA ==========

    final headrestLength = 35.0;
    final headrestEndX = backEndX + headrestLength * math.cos(backAngleRad);
    final headrestEndY = backEndY - headrestLength * math.sin(backAngleRad);

    final headrestPath = Path();
    headrestPath.moveTo(backEndX - perpX * 0.5, backEndY - perpY * 0.5);
    headrestPath.lineTo(backEndX + perpX * 0.5, backEndY + perpY * 0.5);
    headrestPath.quadraticBezierTo(
      headrestEndX + perpX * 0.6,
      headrestEndY + perpY * 0.6,
      headrestEndX,
      headrestEndY - 5,
    );
    headrestPath.quadraticBezierTo(
      headrestEndX - perpX * 0.6,
      headrestEndY - perpY * 0.6,
      backEndX - perpX * 0.5,
      backEndY - perpY * 0.5,
    );
    headrestPath.close();

    canvas.drawPath(headrestPath, backFillPaint);
    canvas.drawPath(headrestPath, backLinePaint);

    // ========== FOCO ==========

    if (isReflectorOn) {
      final reflectorPaint = Paint()
        ..color = Colors.amber
        ..strokeWidth = 2.0
        ..style = PaintingStyle.stroke;

      final reflectorFillPaint = Paint()
        ..color = Colors.amber.withValues(alpha: 0.3)
        ..style = PaintingStyle.fill;

      // Braço do foco
      final reflectorArmStart = Offset(centerX + 50, seatPivotY - 80);
      final reflectorArmEnd = Offset(headrestEndX + 30, headrestEndY - 20);

      canvas.drawLine(reflectorArmStart, reflectorArmEnd, linePaint);

      // Cabeça do foco
      final reflectorPath = Path();
      reflectorPath.addOval(
        Rect.fromCenter(center: reflectorArmEnd, width: 40, height: 30),
      );

      canvas.drawPath(reflectorPath, reflectorFillPaint);
      canvas.drawPath(reflectorPath, reflectorPaint);

      // Raios de luz
      final lightPaint = Paint()
        ..color = Colors.amber.withValues(alpha: 0.4)
        ..strokeWidth = 1.5
        ..style = PaintingStyle.stroke;

      for (int i = -2; i <= 2; i++) {
        final rayEndX = reflectorArmEnd.dx + 50 * math.cos(-0.3 + i * 0.15);
        final rayEndY = reflectorArmEnd.dy + 50 * math.sin(-0.3 + i * 0.15);
        canvas.drawLine(reflectorArmEnd, Offset(rayEndX, rayEndY), lightPaint);
      }
    }

    // ========== DESCANSO DE PERNA (SEMI-ABERTO) ==========

    // Cor do descanso de perna baseada no estado - alpha maior quando ativo
    final legRestAlpha = isLegRestActive ? 0.45 : 0.12;
    final legRestFillPaint = Paint()
      ..color = legRestColor.withValues(alpha: legRestAlpha)
      ..style = PaintingStyle.fill;

    final legRestLinePaint = Paint()
      ..color = legRestColor
      ..strokeWidth = isLegRestActive ? 4.0 : 2.5
      ..style = PaintingStyle.stroke
      ..strokeCap = StrokeCap.round;

    // Ponto de pivô do descanso de perna (frente do assento)
    final legPivotX = seatRight - 10;
    final legPivotY = seatTop + seatThickness / 2;

    // Ângulo do descanso de perna em radianos
    final legAngleRad = legRestAngle * math.pi / 180;

    // Comprimento do descanso de perna
    final legRestLength = 100.0;
    final legRestWidth = 18.0;

    // Calcular pontos do descanso de perna
    final legEndX = legPivotX + legRestLength * math.cos(legAngleRad);
    final legEndY = legPivotY + legRestLength * math.sin(legAngleRad);

    // Vetor perpendicular para largura
    final legPerpX = -math.sin(legAngleRad) * legRestWidth / 2;
    final legPerpY = math.cos(legAngleRad) * legRestWidth / 2;

    // Desenhar descanso de perna (duas partes para efeito semi-aberto)
    // Parte superior do descanso
    final legPath1 = Path();
    legPath1.moveTo(legPivotX - legPerpX * 0.8, legPivotY - legPerpY * 0.8);
    legPath1.lineTo(legPivotX + legPerpX * 0.3, legPivotY + legPerpY * 0.3);

    // Curva suave para a perna
    final leg1EndX =
        legPivotX + legRestLength * 0.95 * math.cos(legAngleRad - 0.05);
    final leg1EndY =
        legPivotY + legRestLength * 0.95 * math.sin(legAngleRad - 0.05);

    legPath1.quadraticBezierTo(
      legPivotX +
          legRestLength * 0.5 * math.cos(legAngleRad - 0.03) +
          legPerpX * 0.3,
      legPivotY +
          legRestLength * 0.5 * math.sin(legAngleRad - 0.03) +
          legPerpY * 0.3,
      leg1EndX + legPerpX * 0.4,
      leg1EndY + legPerpY * 0.4,
    );

    // Ponta arredondada
    legPath1.quadraticBezierTo(
      leg1EndX,
      leg1EndY - 5,
      leg1EndX - legPerpX * 0.4,
      leg1EndY - legPerpY * 0.4,
    );

    legPath1.quadraticBezierTo(
      legPivotX +
          legRestLength * 0.5 * math.cos(legAngleRad - 0.03) -
          legPerpX * 0.5,
      legPivotY +
          legRestLength * 0.5 * math.sin(legAngleRad - 0.03) -
          legPerpY * 0.5,
      legPivotX - legPerpX * 0.8,
      legPivotY - legPerpY * 0.8,
    );
    legPath1.close();

    canvas.drawPath(legPath1, legRestFillPaint);
    canvas.drawPath(legPath1, legRestLinePaint);

    // Parte inferior do descanso (para efeito semi-aberto/ginecológico)
    final legPath2 = Path();
    legPath2.moveTo(legPivotX - legPerpX * 0.3, legPivotY - legPerpY * 0.3);
    legPath2.lineTo(legPivotX + legPerpX * 0.8, legPivotY + legPerpY * 0.8);

    final leg2EndX =
        legPivotX + legRestLength * 0.95 * math.cos(legAngleRad + 0.05);
    final leg2EndY =
        legPivotY + legRestLength * 0.95 * math.sin(legAngleRad + 0.05);

    legPath2.quadraticBezierTo(
      legPivotX +
          legRestLength * 0.5 * math.cos(legAngleRad + 0.03) +
          legPerpX * 0.5,
      legPivotY +
          legRestLength * 0.5 * math.sin(legAngleRad + 0.03) +
          legPerpY * 0.5,
      leg2EndX + legPerpX * 0.4,
      leg2EndY + legPerpY * 0.4,
    );

    legPath2.quadraticBezierTo(
      leg2EndX,
      leg2EndY + 5,
      leg2EndX - legPerpX * 0.4,
      leg2EndY - legPerpY * 0.4,
    );

    legPath2.quadraticBezierTo(
      legPivotX +
          legRestLength * 0.5 * math.cos(legAngleRad + 0.03) -
          legPerpX * 0.3,
      legPivotY +
          legRestLength * 0.5 * math.sin(legAngleRad + 0.03) -
          legPerpY * 0.3,
      legPivotX - legPerpX * 0.3,
      legPivotY - legPerpY * 0.3,
    );
    legPath2.close();

    canvas.drawPath(legPath2, legRestFillPaint);
    canvas.drawPath(legPath2, legRestLinePaint);

    // Linha central do descanso (divisão)
    canvas.drawLine(
      Offset(legPivotX, legPivotY),
      Offset(legEndX, legEndY),
      thinPaint,
    );

    // Suporte/articulação do descanso de perna
    final jointPath = Path();
    jointPath.addOval(
      Rect.fromCenter(
        center: Offset(legPivotX, legPivotY),
        width: 16,
        height: 16,
      ),
    );
    canvas.drawPath(jointPath, fillPaint);
    canvas.drawPath(jointPath, linePaint);
  }

  void _drawWheel(
    Canvas canvas,
    Offset center,
    Paint linePaint,
    Paint fillPaint,
  ) {
    // Roda externa
    canvas.drawCircle(center, 10, fillPaint);
    canvas.drawCircle(center, 10, linePaint);
    // Eixo interno
    canvas.drawCircle(center, 4, linePaint);
  }

  @override
  bool shouldRepaint(covariant _ChairTechnicalDrawingPainter oldDelegate) {
    return oldDelegate.backAngle != backAngle ||
        oldDelegate.seatHeight != seatHeight ||
        oldDelegate.legRestAngle != legRestAngle ||
        oldDelegate.isReflectorOn != isReflectorOn ||
        oldDelegate.backColor != backColor ||
        oldDelegate.seatColor != seatColor ||
        oldDelegate.legRestColor != legRestColor ||
        oldDelegate.isBackActive != isBackActive ||
        oldDelegate.isSeatActive != isSeatActive ||
        oldDelegate.isLegRestActive != isLegRestActive;
  }
}

// Widget de botão arrastável com efeito 3D
class _Draggable3DButton extends StatefulWidget {
  final double width;
  final double height;
  final VoidCallback? onDragUp;
  final VoidCallback? onDragDown;
  final bool isActiveUp;
  final bool isActiveDown;
  final Color activeUpColor;
  final Color activeDownColor;

  const _Draggable3DButton({
    required this.width,
    required this.height,
    required this.onDragUp,
    required this.onDragDown,
    required this.isActiveUp,
    required this.isActiveDown,
    required this.activeUpColor,
    required this.activeDownColor,
  });

  @override
  State<_Draggable3DButton> createState() => _Draggable3DButtonState();
}

class _Draggable3DButtonState extends State<_Draggable3DButton> {
  bool _isPressed = false;
  Timer? _commandTimer;
  bool _isDraggingUp = false;
  bool _isDraggingDown = false;
  bool _isDragActive = false;
  double _accumulatedDy = 0.0; // Acumula movimento para melhor detecção

  @override
  void dispose() {
    _stopCommandTimer();
    super.dispose();
  }

  void _startCommandTimer(VoidCallback? command) {
    // Só inicia timer se o drag estiver ativo
    if (!_isDragActive) return;

    // Cancela timer anterior
    _commandTimer?.cancel();
    _commandTimer = null;

    if (command != null) {
      // Executa o comando imediatamente
      command();
      // Depois continua enviando a cada 200ms (para evitar sobrecarga BLE)
      _commandTimer = Timer.periodic(const Duration(milliseconds: 200), (_) {
        // Verifica se o drag ainda está ativo
        if (_isDragActive) {
          command();
        } else {
          // Se drag não está mais ativo, para tudo e reseta estado visual
          _commandTimer?.cancel();
          _commandTimer = null;
          debugPrint('⏹️ Timer parado automaticamente');
        }
      });
    }
  }

  void _stopCommandTimer() {
    debugPrint('🛑 Parando comando timer completo');
    // Para tudo: cancela timer E desativa flags
    _commandTimer?.cancel();
    _commandTimer = null;
    _isDraggingUp = false;
    _isDraggingDown = false;
    _isDragActive = false;
  }

  @override
  Widget build(BuildContext context) {
    return GestureDetector(
      behavior: HitTestBehavior.opaque,
      onVerticalDragStart: (_) {
        setState(() {
          _isPressed = true;
          _isDragActive = true;
          _accumulatedDy = 0.0; // Reset acumulador
        });
      },
      onVerticalDragEnd: (_) {
        debugPrint('📍 Drag finalizado');
        setState(() => _isPressed = false);
        _stopCommandTimer(); // Já chama onDragStop internamente
      },
      onVerticalDragCancel: () {
        debugPrint('❌ Drag cancelado');
        setState(() => _isPressed = false);
        _stopCommandTimer(); // Já chama onDragStop internamente
      },
      onVerticalDragUpdate: (details) {
        // Só processa updates se o drag estiver ativo
        if (!_isDragActive) return;

        // Acumula movimento para ter certeza da direção
        _accumulatedDy += details.delta.dy;

        // Threshold reduzido para melhor detecção
        const double threshold = 5.0;

        // Verifica direção acumulada (não instantânea)
        if (_accumulatedDy < -threshold &&
            widget.onDragUp != null &&
            !_isDraggingUp) {
          debugPrint(
            '🔼 SUBIR detectado (acumulado=${_accumulatedDy.toStringAsFixed(1)})',
          );
          _isDraggingDown = false;
          _isDraggingUp = true;
          _accumulatedDy = 0.0; // Reset após detectar
          _startCommandTimer(widget.onDragUp);
        } else if (_accumulatedDy > threshold &&
            widget.onDragDown != null &&
            !_isDraggingDown) {
          debugPrint(
            '🔽 DESCER detectado (acumulado=${_accumulatedDy.toStringAsFixed(1)})',
          );
          _isDraggingUp = false;
          _isDraggingDown = true;
          _accumulatedDy = 0.0; // Reset após detectar
          _startCommandTimer(widget.onDragDown);
        }
      },
      // Área de toque com ícone de orientação visível (sem fundo)
      child: SizedBox(
        width: widget.width,
        height: widget.height,
        child: Center(
          child: Icon(
            _isPressed ? Icons.touch_app : Icons.swipe_vertical,
            color: Colors.lightBlue.shade400,
            size: 24,
          ),
        ),
      ),
    );
  }
}

// Widget de área de toque com gerenciamento adequado de Timer
class _TouchAreaButton extends StatefulWidget {
  final double width;
  final double height;
  final VoidCallback? onTap;
  final VoidCallback? onLongPressStart;
  final VoidCallback? onLongPressEnd;
  final bool isActive;
  final Color activeColor;
  final Color inactiveColor;
  final String label;
  final IconData icon;
  final double fontSize;

  const _TouchAreaButton({
    super.key,
    required this.width,
    required this.height,
    required this.onTap,
    required this.onLongPressStart,
    required this.onLongPressEnd,
    required this.isActive,
    required this.activeColor,
    required this.inactiveColor,
    required this.label,
    required this.icon,
    required this.fontSize,
  });

  @override
  State<_TouchAreaButton> createState() => _TouchAreaButtonState();
}

class _TouchAreaButtonState extends State<_TouchAreaButton> {
  @override
  Widget build(BuildContext context) {
    final isActive = widget.isActive;
    debugPrint('🎨 Renderizando ${widget.label}: isActive=$isActive');

    return GestureDetector(
      onTap: () {
        debugPrint('👆 Tap em ${widget.label}');
        widget.onTap?.call();
      },
      onLongPressStart: (_) {
        debugPrint('🔘 LongPressStart em ${widget.label}');
        widget.onLongPressStart?.call();
      },
      onLongPressEnd: (_) {
        debugPrint('🔘 LongPressEnd em ${widget.label}');
        widget.onLongPressEnd?.call();
      },
      onLongPressCancel: () {
        debugPrint('🔘 LongPressCancel em ${widget.label}');
        widget.onLongPressEnd?.call();
      },
      child: Container(
        width: widget.width,
        height: widget.height,
        color: Colors.transparent, // Área de toque invisível
      ),
    );
  }
}

// Painter para o desenho técnico do Pedal (Novo Formato Semi-oval)
class _PedalTechnicalDrawingPainter extends CustomPainter {
  final bool backUpOn;
  final bool backDownOn;
  final bool seatUpOn;
  final bool seatDownOn;
  final bool legUpOn;
  final bool legDownOn;
  final bool autoZeroOn;
  final bool workPosOn;

  _PedalTechnicalDrawingPainter({
    required this.backUpOn,
    required this.backDownOn,
    required this.seatUpOn,
    required this.seatDownOn,
    required this.legUpOn,
    required this.legDownOn,
    required this.autoZeroOn,
    required this.workPosOn,
  });

  @override
  void paint(Canvas canvas, Size size) {
    final centerX = size.width / 2;
    final centerY = size.height / 2;

    final linePaint = Paint()
      ..color = const Color(0xFF1565C0)
      ..strokeWidth = 2.5
      ..style = PaintingStyle.stroke
      ..strokeCap = StrokeCap.round;

    final fillPaint = Paint()
      ..color = const Color(0xFFE0E0E0)
      ..style = PaintingStyle.fill;

    final buttonPaint = Paint()
      ..color = Colors.black87
      ..style = PaintingStyle.fill;

    final activeBtnPaint = Paint()
      ..color = Colors.blue.shade600
      ..style = PaintingStyle.fill;

    final labelStyle = const TextStyle(
      color: Colors.black87,
      fontSize: 10,
      fontWeight: FontWeight.bold,
    );

    // 1. Desenhar o corpo do pedal (Semi-oval / Trapezoidal arredondado)
    final pedalPath = Path();
    pedalPath.moveTo(
      centerX - 130,
      centerY - 80,
    ); // Topo esquerda (mais largo e alto)
    pedalPath.lineTo(centerX + 130, centerY - 80); // Topo direita
    pedalPath.quadraticBezierTo(
      centerX + 165, // Aumentado de 150 para 165
      centerY + 140, // Aumentado de 130 para 140
      centerX,
      centerY + 165, // Aumentado de 150 para 165
    ); // Curva direita p/ baixo (mais profundo)
    pedalPath.quadraticBezierTo(
      centerX - 165, // Aumentado de 150 para 165
      centerY + 140, // Aumentado de 130 para 140
      centerX - 130,
      centerY - 80,
    ); // Curva esquerda p/ baixo
    pedalPath.close();

    canvas.drawPath(pedalPath, fillPaint);
    canvas.drawPath(pedalPath, linePaint);

    // 2. Desenhar os 8 Botões Pretos
    // Lado P (Esquerda)
    _drawButton(
      canvas,
      Offset(centerX - 95, centerY - 45),
      legUpOn ? activeBtnPaint : buttonPaint,
    ); // Topo (P Subir)
    _drawButton(
      canvas,
      Offset(centerX - 95, centerY + 55),
      legDownOn ? activeBtnPaint : buttonPaint,
    ); // Baixo (P Descer)

    // Lado E (Direita)
    _drawButton(
      canvas,
      Offset(centerX + 95, centerY - 45),
      backUpOn ? activeBtnPaint : buttonPaint,
    ); // Topo (E Subir)
    _drawButton(
      canvas,
      Offset(centerX + 95, centerY + 55),
      backDownOn ? activeBtnPaint : buttonPaint,
    ); // Baixo (E Descer)

    // Automático (Centro Topo)
    _drawButton(
      canvas,
      Offset(centerX - 35, centerY - 45),
      autoZeroOn ? activeBtnPaint : buttonPaint,
    ); // Esquerda (Zero)
    _drawButton(
      canvas,
      Offset(centerX + 35, centerY - 45),
      workPosOn ? activeBtnPaint : buttonPaint,
    ); // Direita (Trabalho)

    // Assento A (Centro Baixo)
    _drawButton(
      canvas,
      Offset(centerX - 35, centerY + 95),
      seatUpOn ? activeBtnPaint : buttonPaint,
    ); // Esquerda (A Subir)
    _drawButton(
      canvas,
      Offset(centerX + 35, centerY + 95),
      seatDownOn ? activeBtnPaint : buttonPaint,
    ); // Direita (A Descer)

    // 3. Desenhar os Labels e Ícones (P, E, A, AUTOMÁTICO)
    _drawLabelWithArrows(
      canvas,
      Offset(centerX - 95, centerY + 5),
      "P",
      labelStyle,
    );
    _drawLabelWithArrows(
      canvas,
      Offset(centerX + 95, centerY + 5),
      "E",
      labelStyle,
    );
    _drawLabelWithArrows(
      canvas,
      Offset(centerX, centerY - 45),
      "AUTOMÁTICO",
      labelStyle,
    );
    _drawLabelWithArrows(
      canvas,
      Offset(centerX, centerY + 75),
      "A",
      labelStyle,
    );
  }

  void _drawButton(Canvas canvas, Offset center, Paint paint) {
    canvas.drawCircle(center, 22, paint);
    // Efeito de brilho no botão
    final shinePaint = Paint()
      ..color = Colors.white.withValues(alpha: 0.2)
      ..style = PaintingStyle.fill;
    canvas.drawCircle(center.translate(-5, -5), 8, shinePaint);

    // Se estiver ativo, adicionar uma borda externa brilhante
    if (paint.color == Colors.blue.shade600) {
      final glowPaint = Paint()
        ..color = Colors.blue.shade300.withValues(alpha: 0.5)
        ..strokeWidth = 4.0
        ..style = PaintingStyle.stroke;
      canvas.drawCircle(center, 24, glowPaint);
    }
  }

  void _drawLabelWithArrows(
    Canvas canvas,
    Offset center,
    String text,
    TextStyle style,
  ) {
    final textPainter = TextPainter(
      text: TextSpan(text: text, style: style),
      textDirection: TextDirection.ltr,
    );
    textPainter.layout();
    textPainter.paint(
      canvas,
      center.translate(-textPainter.width / 2, -textPainter.height / 2),
    );

    // Setas Vermelhas Pequenas (Up/Down)
    final arrowPaint = Paint()
      ..color = Colors.red.shade700
      ..strokeWidth = 1.5
      ..style = PaintingStyle.stroke;

    // Seta Cima
    canvas.drawLine(
      center.translate(-textPainter.width / 2 - 8, 2),
      center.translate(-textPainter.width / 2 - 8, -6),
      arrowPaint,
    );
    canvas.drawLine(
      center.translate(-textPainter.width / 2 - 8, -6),
      center.translate(-textPainter.width / 2 - 11, -3),
      arrowPaint,
    );
    canvas.drawLine(
      center.translate(-textPainter.width / 2 - 8, -6),
      center.translate(-textPainter.width / 2 - 5, -3),
      arrowPaint,
    );

    // Seta Baixo
    canvas.drawLine(
      center.translate(textPainter.width / 2 + 8, -6),
      center.translate(textPainter.width / 2 + 8, 2),
      arrowPaint,
    );
    canvas.drawLine(
      center.translate(textPainter.width / 2 + 8, 2),
      center.translate(textPainter.width / 2 + 5, -1),
      arrowPaint,
    );
    canvas.drawLine(
      center.translate(textPainter.width / 2 + 8, 2),
      center.translate(textPainter.width / 2 + 11, -1),
      arrowPaint,
    );
  }

  @override
  bool shouldRepaint(covariant CustomPainter oldDelegate) => false;
}
