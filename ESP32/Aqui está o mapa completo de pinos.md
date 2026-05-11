\## Comandos (Serial/BLE/MQTT)

\### Diagnóstico / I2C / PCF8574

\- I2C\_SCAN

\- I2C\_PIN\_TEST

\- PCF8574\_PING

\- PCF8574\_TEST

\- PCF8574\_BTNS

\- GPIO\_BTNS

\- PCF8574\_INT



\### Config / Utilitários

\- PING

\- STATUS

\- HORIMETRO

\- USER\_ID=<uuid> (ou USER\_ID:<uuid> )

\- USER\_ID\_STATUS

\- GAVETA\_PIN=<gpio> (ou GAVETA\_PIN:<gpio> )  (altera GPIO da gaveta)



\### WiFi

\- WIFI\_STATUS

\- WIFI\_IP

\- WIFI\_CONFIG

\- WIFI\_RESET



\### Limites / Segurança

\- LIMITS\_OFF

\- LIMITS\_ON

\- LIMITS\_STATUS

\- AT\_SEG (ou STOP )



\### Buzzer

\- BUZZER\_TEST\_ON

\- BUZZER\_TEST\_OFF

\- BUZZER\_TEST\_STATUS



\### Teste de relés

\- TEST\_SA

\- TEST\_DA

\- TEST\_SE

\- TEST\_DE

\- TEST\_SP

\- TEST\_DP

\- TEST\_RF



\### Movimentos (dead-man via BLE/MQTT/Serial)

\- SE

\- DE

\- SA

\- DA

\- SP (bloqueia se gaveta aberta)

\- DP (bloqueia se gaveta aberta)

\- RF



\### Posições

\- VZ

\- PT

\- M1



\### Calibração

\- CAL\_RESET

\- CAL\_START



\### Memória via Supabase

\- M1\_LOAD\_FROM\_DB

\- M\_LOAD\_FROM\_DB=<slot> (ou M\_LOAD\_FROM\_DB:<slot> )



\### Percurso / Pulsos / Distância

\- TRAVEL\_STATUS

\- TRAVEL\_RESET

\- TRAVEL\_SEND

\- MM\_PER\_PULSE\_ENC=<float> (mm/pulso)

\- MM\_PER\_PULSE\_ASS=<float> (mm/pulso)

\- MM\_PER\_PULSE\_PER=<float> (mm/pulso)

\- ENC\_DEBUG\_ON

\- ENC\_DEBUG\_OFF



\## IOs (pinos) — env esp32s3\_i2c\_2\_18

\### I2C

\- SDA = GPIO2

\- SCL = GPIO18

\- PCF8574\_INT\_PIN = GPIO1



\### Entradas (botões via PCF8574)

\- M1 = PCF P0

\- SE = PCF P1

\- PT = PCF P2

\- VZ = PCF P3

\- DP = PCF P4

\- SP = PCF P5

\- DE = PCF P6

\- DA = PCF P7



\### Entradas (GPIO direto)

\- SA = GPIO14

\- RF = GPIO15

\- GAVETA = GPIO20



\### Encoders (GPIO direto)

\- ENCODER1 (ASSENTO) = GPIO4

\- ENCODER2 (PERNEIRA) = GPIO17

\- ENCODER3 (ENCOSTO) = GPIO19



\### Saídas (relés / GPIO)

\- RELE\_SA = GPIO16

\- RELE\_DA = GPIO9

\- RELE\_SE = GPIO5

\- RELE\_DE = GPIO6

\- RELE\_SP = GPIO7

\- RELE\_DP = GPIO10

\- RELE\_REFLETOR = GPIO11



\### Sinais auxiliares

\- LED = GPIO12

\- BUZZER = GPIO13

