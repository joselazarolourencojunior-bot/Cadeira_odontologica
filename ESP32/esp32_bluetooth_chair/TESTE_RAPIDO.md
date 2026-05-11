# Instruções de Teste Rápido

## 1. Preparação

### ESP32
```bash
# 1. Abra o Arduino IDE
# 2. Carregue o arquivo: esp32_bluetooth_chair.ino
# 3. Selecione sua placa ESP32
# 4. Faça upload
# 5. Abra o Serial Monitor (115200 baud)
# 6. Verifique se aparece: "Dispositivo Bluetooth iniciado..."
```

### Flutter App
```bash
# No terminal, na pasta do projeto:
flutter pub get
flutter run
```

## 2. Teste de Conexão

1. **Pareamento**:
   - Vá em Configurações > Bluetooth no seu celular
   - Procure por "CadeiraOdontologica"
   - Pareie o dispositivo

2. **Conexão no App**:
   - Abra o app Flutter
   - Toque em "Buscar Dispositivos"
   - Selecione "CadeiraOdontologica"
   - Toque em "Conectar"

## 3. Teste de Comandos

### Via Serial Monitor (para depuração)
```
RF    # Testa refletor
SP    # Testa subir pernas
DP    # Testa descer pernas
AT_SEG # Testa emergência
STATUS # Verifica status atual
```

### Via App
- Teste cada botão individual
- Teste posições automáticas (VZ/PT)
- Teste botão de emergência

## 4. Verificações de Segurança

1. **Timeout**: Deixe um movimento ativo por 15s - deve parar automaticamente
2. **Emergência**: Teste o botão vermelho no app
3. **Botões físicos**: Pressione qualquer botão no ESP32 durante movimento automático
4. **Buzzer**: Deve emitir som no início/fim de operações

## 5. Solução de Problemas Rápidos

### Não conecta:
```bash
# Reinicie ESP32
# Desfaça pareamento e pareie novamente
# Reinicie Bluetooth do celular
```

### Comandos não funcionam:
```bash
# Verifique Serial Monitor para logs
# Teste comando STATUS no Serial Monitor
# Verifique LED piscando no ESP32
```

## 6. Comandos de Debug

No Serial Monitor do Arduino IDE, digite:
- `STATUS` - Ver estado atual
- `RF` - Testar refletor
- `AT_SEG` - Parar tudo

No app, use o botão de status ou observe as mensagens de feedback.

## ⚡ Teste Completo (5 minutos)

1. Upload código ESP32 ✓
2. Pareamento Bluetooth ✓
3. Conexão no app ✓
4. Teste 1 botão individual ✓
5. Teste 1 posição automática ✓
6. Teste botão emergência ✓

Se todos esses itens funcionarem, o sistema está operacional!