import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'services/bluetooth_service.dart';
import 'services/settings_service.dart';
import 'screens/bluetooth_connection_screen.dart';
import 'screens/chair_control_screen.dart';
import 'screens/settings_screen.dart';

void main() async {
  WidgetsFlutterBinding.ensureInitialized();

  // Inicializar serviços
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
        ChangeNotifierProvider.value(value: settingsService),
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
        initialRoute: '/',
        routes: {
          '/': (context) => const ChairControlScreen(),
          '/bluetooth': (context) => const BluetoothConnectionScreen(),
          '/settings': (context) => const SettingsScreen(),
        },
        debugShowCheckedModeBanner: false,
      ),
    );
  }
}
