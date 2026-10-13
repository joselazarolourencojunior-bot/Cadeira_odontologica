# Firmware ESP32 - Cadeira Odontológica

## Arquivos
- `esp32_bluetooth_chair.ino` - Código principal do firmware
- `platformio.ini` - Configuração do PlatformIO

## Como Gravar no ESP32

### Opção 1: Arduino IDE (Mais Simples)
1. Abra o Arduino IDE
2. Instale o suporte ao ESP32:
   - Vá em `Arquivo > Preferências`
   - Em "URLs Adicionais de Placas", adicione:
     `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
3. Vá em `Ferramentas > Placa > ESP32 Arduino` e selecione `ESP32 Dev Module`
4. Selecione a porta correta em `Ferramentas > Porta`
5. Abra o arquivo `esp32_bluetooth_chair.ino`
6. Clique em "Carregar" (Seta para direita)

### Opção 2: PlatformIO (Recomendado)
1. Instale o VS Code
2. Instale a extensão PlatformIO
3. Abra esta pasta como projeto
4. Conecte o ESP32
5. Clique em "Upload" (seta para direita na barra inferior)

## Configuração de Pinos

### Entradas (Botões)
- Pino 5: DE - Deitar (Encosto)
- Pino 34: SA - Subir Assento
- Pino 17: DA - Descer Assento
- Pino 13: SE - Sentar (Encosto)
- Pino 22: VZ - Posição Ginecológica
- Pino 18: SP - Subir Pernas
- Pino 19: DP - Descer Pernas
- Pino 21: PT - Posição de Parto
- Pino 23: RF - Refletor
- Pino 16: AD_PIC

### Saídas (Relés e Indicadores)
- Pino 33: Relé Subir Assento
- Pino 25: Relé Descer Assento
- Pino 27: Relé Sentar
- Pino 26: Relé Deitar
- Pino 12: Relé Subir Pernas
- Pino 14: Relé Descer Pernas
- Pino 4: Relé Refletor
- Pino 2: LED Indicador
- Pino 32: Buzzer

## Comandos Bluetooth

O dispositivo aparece como **"CadeiraOdontologica"** no pareamento.

### Comandos Disponíveis
- `RF` - Liga/Desliga Refletor
- `SP` - Subir Pernas
- `DP` - Descer Pernas
- `SA` - Subir Assento
- `DA` - Descer Assento
- `SE` - Sentar (Encosto para cima)
- `DE` - Deitar (Encosto para baixo)
- `VZ` - Posição Ginecológica
- `PT` - Posição de Parto
- `AT_SEG` - Parada de Emergência
- `STATUS` - Status Completo

## Notas Importantes

- Os movimentos param automaticamente após 30 segundos de ativação contínua
- Qualquer botão físico pressionado durante movimento automático para a operação
- O comando `AT_SEG` pode ser enviado via Bluetooth para parada de emergência
- LED pisca a cada 500ms indicando funcionamento
- Buzzer emite 2 bips na inicialização

## Monitor Serial

Após carregar, abra o Monitor Serial (Ctrl+Shift+M) com velocidade 115200 para ver:
- Mensagens de inicialização
- Comandos recebidos
- Status dos relés
- Mensagens de segurança
