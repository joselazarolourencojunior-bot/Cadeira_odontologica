# Instruções de Teste Rápido

## 1. Preparação

### Flutter App

```bash
# No terminal, na pasta do projeto:
flutter pub get
flutter run
```

## 2. Teste da Interface

### Conexão BLE

1. Abra o app
2. Toque em "Buscar Dispositivos"
3. Selecione o dispositivo BLE da cadeira
4. Toque em "Conectar"

### Controles Individuais

1. **Refletor**: Toque no botão do refletor
2. **Movimentos**: Pressione e segure os botões de movimento
3. **Posições**: Toque nos botões de posição automática
4. **Emergência**: Toque no botão vermelho "PARAR TUDO"

## 3. Verificação

### Logs do App

- Procure por mensagens como:
  - `📥 Dados recebidos: [resposta]`
  - `ℹ️ Confirmação recebida: [comando]`
  - `📤 Comando enviado: [comando]`

### Estados Visuais

- Botões devem mudar de cor quando ativos
- Indicadores visuais devem refletir o estado

## 4. Teste de Segurança

### Timeout Automático

1. Pressione e segure um botão de movimento
2. Aguarde o timeout (15 segundos)
3. Verifique se o movimento para automaticamente

### Botão de Emergência

1. Inicie um movimento
2. Toque no botão "PARAR TUDO"
3. Verifique se todos os movimentos param

## 5. Checklist Final

- [ ] App inicia sem erros
- [ ] BLE conecta ao dispositivo
- [ ] Comandos são enviados
- [ ] Estados são atualizados
- [ ] Timeout funciona
- [ ] Emergência funciona
- [ ] Interface responde corretamente
