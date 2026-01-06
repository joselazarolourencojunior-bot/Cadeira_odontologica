# Configuração de Credenciais - Template
# Configuration Credentials - Template

## ⚠️ IMPORTANTE / IMPORTANT

Este é um arquivo de TEMPLATE. As credenciais aqui são apenas exemplos.

**NÃO USE ESTAS CREDENCIAIS EM PRODUÇÃO!**

---

## 📝 Como Configurar / How to Configure

### 1. Obtenha suas credenciais do Supabase
   - Acesse: https://app.supabase.com
   - Selecione seu projeto
   - Vá em: Settings > API
   - Copie: Project URL e anon/public key

### 2. Atualize o arquivo `src/main.cpp`
   - Localize as linhas 34-35
   - Substitua pelos seus valores reais

### 3. Compile e faça upload
   - Use PlatformIO ou Arduino IDE
   - Faça upload para o ESP32

---

## 🔐 Template de Credenciais

```cpp
// ========== CONFIGURAÇÕES SUPABASE ==========
const char* SUPABASE_URL = "https://seu-projeto-unico.supabase.co";
const char* SUPABASE_KEY = "sua_chave_publica_anon_key_aqui";
const char* SENHA_AP = "12345678";  // Senha da rede WiFi de configuração (mín. 8 caracteres)
// ============================================
```

---

## 📋 Exemplo (NÃO USAR EM PRODUÇÃO)

```cpp
// ========== EXEMPLO - NÃO USAR! ==========
const char* SUPABASE_URL = "https://exemplo.supabase.co";
const char* SUPABASE_KEY = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.exemplo";
const char* SENHA_AP = "12345678";
// =========================================
```

---

## ⚠️ Segurança

- 🔒 Mantenha o repositório PRIVADO
- 🔒 NÃO compartilhe suas credenciais
- 🔒 Use credenciais diferentes para desenvolvimento e produção
- 🔒 Configure Row Level Security (RLS) no Supabase

---

## 🗂️ Arquivos que Contêm Credenciais

1. **src/main.cpp**
   - Linhas 34-36
   - Configurações principais do Supabase

---

## 📞 Dúvidas?

Consulte a documentação:
- README.md - Configuração geral
- SECURITY.md - Políticas de segurança
- Supabase Docs: https://supabase.com/docs
