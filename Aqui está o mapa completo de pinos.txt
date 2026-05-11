Aqui está o mapa completo de pinos (GPIOs e portas do PCF8574) que estão configurados no projeto atual para a sua placa ESP32-S3 (no ambiente esp32s3_i2c_2_18 ):

### Conexões I2C (PCF8574)
Esses são os pinos que ligam o ESP32 ao módulo PCF8574.

- SDA: GPIO 2
- SCL: GPIO 18
- INT (Interrupção): GPIO 1
### Entradas do PCF8574 (Botões)
Os botões devem ser ligados nas portas do PCF8574 e fechar contato com o GND (acionamento em nível baixo, 0V).

- P0: Botão M1
- P1: Botão Subir Encosto (SE)
- P2: Botão Posição de Trabalho (PT) ou Cuspidor
- P3: Botão Volta a Zero (VZ)
- P4: Botão Descer Perneira (DP)
- P5: Botão Subir Perneira (SP)
- P6: Botão Descer Encosto (DE)
- P7: Botão Descer Assento (DA)
### Entradas Diretas no ESP32
Esses são os botões e sensores que estão ligados diretamente nos GPIOs do ESP32.

- Botão Subir Assento (SA): GPIO 14
- Botão Refletor (RF): GPIO 15
- Encoder 1 (Assento): GPIO 4
- Encoder 2 (Perneira): GPIO 17
- Encoder 3 (Encosto): GPIO 19
(Nota: O botão da Gaveta está desabilitado com -1 )

### Saídas no ESP32 (Relés, LEDs e Buzzer)
Esses são os pinos que controlam os drivers dos relés e os indicadores visuais/sonoros.

- Relé Subir Assento (SA): GPIO 21 (Movido do 2 para o 21 para não conflitar com I2C_SDA)
- Relé Descer Assento (DA): GPIO 3
- Relé Subir Encosto (SE): GPIO 5
- Relé Descer Encosto (DE): GPIO 6
- Relé Subir Perneira (SP): GPIO 7
- Relé Descer Perneira (DP): GPIO 10
- Relé Refletor: GPIO 11
- LED Indicador: GPIO 12
- Buzzer: GPIO 13


Ação: Solde um resistor de 10kΩ entre o GPIO 45 e o GND. Isso garante que o chip sempre selecione 3.3V para a Flash/PSRAM.

Checklist de Teste:
Meça com um multímetro: O GPIO 45 está em 0V?
Meça com um multímetro: O GPIO 0 vai para 0V quando você aperta o botão de Boot?
O seu conversor serial e o ESP32 compartilham o mesmo GND? (Obrigatório).
A fonte de 3.3V que alimenta o módulo aguenta pelo menos 500mA? (Conversores USB-Serial simples costumam falhar aqui).
Dica de Ouro: Tente colocar o GPIO 45 no GND e o GPIO 0 no GND permanentemente (com jumpers) e ligue a alimentação. Se o monitor serial mostrar waiting for download, seu circuito de botões ou a falta de pull-down no pino 45 é o culpado.
Você tem um osciloscópio ou multímetro para medir a tensão nesses pinos durante o reset?





