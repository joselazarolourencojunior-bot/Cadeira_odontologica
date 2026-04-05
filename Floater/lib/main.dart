import 'package:flutter/material.dart';
import 'dart:math' as math;
import 'package:provider/provider.dart';
import 'services/bluetooth_service.dart';
import 'services/settings_service.dart';
import 'services/supabase_service.dart';
import 'services/mqtt_service.dart';
import 'screens/bluetooth_connection_screen_ble.dart';
import 'screens/chair_control_screen.dart';
import 'screens/settings_screen.dart';

void main() async {
  WidgetsFlutterBinding.ensureInitialized();

  // Inicializar Supabase
  await SupabaseService.initialize();

  // Inicializar serviços locais
  final settingsService = SettingsService();
  await settingsService.loadSettings();

  runApp(CadeiraOdontologicaApp(settingsService: settingsService));
}

class CadeiraOdontologicaApp extends StatelessWidget {
  final SettingsService settingsService;

  const CadeiraOdontologicaApp({super.key, required this.settingsService});

  @override
  Widget build(BuildContext context) {
    return MultiProvider(
      providers: [
        ChangeNotifierProvider(create: (context) => BluetoothService()),
        ChangeNotifierProvider(
          create: (context) => MqttService(settingsService: settingsService),
        ),
        ChangeNotifierProvider.value(value: settingsService),
        ChangeNotifierProvider.value(value: SupabaseService.instance),
      ],
      child: MaterialApp(
        title: 'Cadeira Odontológica/Ginecológica',
        theme: ThemeData(
          colorScheme: ColorScheme.fromSeed(
            seedColor: Colors.blue,
            brightness: Brightness.light,
          ),
          useMaterial3: true,
        ),
        home: const SplashScreen(),
        routes: {
          '/home': (context) => const ChairControlScreen(),
          '/bluetooth': (context) => const BluetoothConnectionScreen(),
          '/settings': (context) => const SettingsScreen(),
        },
        debugShowCheckedModeBanner: false,
      ),
    );
  }
}

class SplashScreen extends StatefulWidget {
  const SplashScreen({super.key});

  @override
  State<SplashScreen> createState() => _SplashScreenState();
}

class _SplashScreenState extends State<SplashScreen>
    with SingleTickerProviderStateMixin {
  late final AnimationController _controller;

  @override
  void initState() {
    super.initState();
    _controller = AnimationController(
      vsync: this,
      duration: const Duration(milliseconds: 5000),
    )..forward();

    _controller.addStatusListener((status) {
      if (status != AnimationStatus.completed) return;
      if (!mounted) return;
      Navigator.of(context).pushReplacementNamed('/home');
    });
  }

  @override
  void dispose() {
    _controller.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final scheme = Theme.of(context).colorScheme;
    final primary = scheme.primary;

    return Scaffold(
      body: AnimatedBuilder(
        animation: _controller,
        builder: (context, _) {
          final progress = Curves.easeInOutCubic.transform(_controller.value);

          return Stack(
            children: [
              Container(
                decoration: BoxDecoration(
                  gradient: LinearGradient(
                    begin: Alignment.topLeft,
                    end: Alignment.bottomRight,
                    colors: [
                      const Color(0xFF050B1A),
                      Color.lerp(const Color(0xFF0B163A), primary, 0.25)!,
                      const Color(0xFF061022),
                    ],
                  ),
                ),
              ),
              Positioned.fill(
                child: CustomPaint(
                  painter: _TechBackdropPainter(
                    t: progress,
                    accent: Color.lerp(primary, Colors.cyanAccent, 0.35)!,
                  ),
                ),
              ),
              SafeArea(
                child: Center(
                  child: Padding(
                    padding: const EdgeInsets.symmetric(horizontal: 28),
                    child: Column(
                      mainAxisAlignment: MainAxisAlignment.center,
                      children: [
                        Container(
                          width: 108,
                          height: 108,
                          decoration: BoxDecoration(
                            shape: BoxShape.circle,
                            border: Border.all(
                              color: Colors.white.withValues(alpha: 0.18),
                              width: 2,
                            ),
                            boxShadow: [
                              BoxShadow(
                                color: primary.withValues(alpha: 0.35),
                                blurRadius: 32,
                                offset: const Offset(0, 12),
                              ),
                            ],
                            gradient: RadialGradient(
                              colors: [
                                Colors.white.withValues(alpha: 0.12),
                                Colors.white.withValues(alpha: 0.04),
                                Colors.transparent,
                              ],
                              stops: const [0.0, 0.55, 1.0],
                            ),
                          ),
                          child: ClipOval(
                            child: Padding(
                              padding: const EdgeInsets.all(16),
                              child: Image.asset(
                                'Logo Confibelle sem tagline_Principal 01.png',
                                fit: BoxFit.contain,
                              ),
                            ),
                          ),
                        ),
                        const SizedBox(height: 22),
                        Text(
                          'CADEIRA INTELIGENTE',
                          textAlign: TextAlign.center,
                          style: TextStyle(
                            color: Colors.white.withValues(alpha: 0.95),
                            fontSize: 20,
                            fontWeight: FontWeight.w800,
                            letterSpacing: 1.8,
                          ),
                        ),
                        const SizedBox(height: 6),
                        Text(
                          'Controle • Segurança • Diagnóstico',
                          textAlign: TextAlign.center,
                          style: TextStyle(
                            color: Colors.white.withValues(alpha: 0.70),
                            fontSize: 12,
                            fontWeight: FontWeight.w600,
                            letterSpacing: 0.6,
                          ),
                        ),
                        const SizedBox(height: 34),
                        ClipRRect(
                          borderRadius: BorderRadius.circular(999),
                          child: SizedBox(
                            height: 10,
                            child: LinearProgressIndicator(
                              value: progress,
                              backgroundColor: Colors.white.withValues(
                                alpha: 0.10,
                              ),
                              valueColor: AlwaysStoppedAnimation<Color>(
                                Color.lerp(primary, Colors.cyanAccent, 0.35)!,
                              ),
                            ),
                          ),
                        ),
                        const SizedBox(height: 12),
                        Text(
                          'Inicializando sistemas... ${(progress * 100).round()}%',
                          style: TextStyle(
                            color: Colors.white.withValues(alpha: 0.70),
                            fontSize: 12,
                            fontWeight: FontWeight.w600,
                          ),
                        ),
                      ],
                    ),
                  ),
                ),
              ),
            ],
          );
        },
      ),
    );
  }
}

class _TechBackdropPainter extends CustomPainter {
  final double t;
  final Color accent;

  _TechBackdropPainter({required this.t, required this.accent});

  @override
  void paint(Canvas canvas, Size size) {
    final gridPaint = Paint()
      ..color = Colors.white.withValues(alpha: 0.06)
      ..strokeWidth = 1
      ..style = PaintingStyle.stroke;

    final glowPaint = Paint()
      ..color = accent.withValues(alpha: 0.16)
      ..strokeWidth = 2
      ..style = PaintingStyle.stroke
      ..strokeCap = StrokeCap.round;

    final dotPaint = Paint()
      ..color = accent.withValues(alpha: 0.22)
      ..style = PaintingStyle.fill;

    final cell = math.max(26.0, size.shortestSide / 14);
    final dx = (t * cell) * 0.8;
    final dy = (t * cell) * 0.55;

    for (double x = -cell; x <= size.width + cell; x += cell) {
      canvas.drawLine(
        Offset(x + dx, 0),
        Offset(x + dx, size.height),
        gridPaint,
      );
    }
    for (double y = -cell; y <= size.height + cell; y += cell) {
      canvas.drawLine(Offset(0, y + dy), Offset(size.width, y + dy), gridPaint);
    }

    final waveY = size.height * (0.32 + 0.08 * math.sin(t * math.pi * 2));
    final wavePath = Path();
    wavePath.moveTo(0, waveY);
    for (double x = 0; x <= size.width; x += 18) {
      final y = waveY + 12 * math.sin((x / 120) + (t * math.pi * 2));
      wavePath.lineTo(x, y);
    }
    canvas.drawPath(wavePath, glowPaint);

    final seed = (t * 1000).floor();
    for (int i = 0; i < 26; i++) {
      final fx = _hash01(seed + i * 13) * size.width;
      final fy = _hash01(seed + i * 29) * size.height;
      final r = 1.2 + 2.2 * _hash01(seed + i * 41);
      canvas.drawCircle(Offset(fx, fy), r, dotPaint);
    }
  }

  double _hash01(int x) {
    var v = x;
    v = (v ^ 61) ^ (v >> 16);
    v = v + (v << 3);
    v = v ^ (v >> 4);
    v = v * 0x27d4eb2d;
    v = v ^ (v >> 15);
    return (v & 0x7fffffff) / 0x7fffffff;
  }

  @override
  bool shouldRepaint(covariant _TechBackdropPainter oldDelegate) {
    return oldDelegate.t != t || oldDelegate.accent != accent;
  }
}
