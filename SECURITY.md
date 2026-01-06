# 🔒 Política de Segurança / Security Policy

## ⚠️ Repositório Privado / Private Repository

**ESTE REPOSITÓRIO DEVE SER MANTIDO COMO PRIVADO**

Este projeto contém informações sensíveis e não deve ser tornado público.

---

## 📋 Informações Sensíveis Presentes / Sensitive Information

Este repositório contém:

1. **Credenciais do Supabase**
   - URL do projeto Supabase
   - Chave API pública (anon key)
   - Configurações de autenticação

2. **Código Proprietário**
   - Lógica de negócio da cadeira odontológica
   - Implementações específicas do sistema

3. **Configurações do Sistema**
   - Pinagem do hardware
   - Configurações de WiFi e Bluetooth
   - Parâmetros de segurança

---

## 🔐 Boas Práticas de Segurança

### 1. Manter Repositório Privado

✅ **SEMPRE mantenha este repositório como PRIVADO** nas configurações do GitHub

**Como verificar:**
1. Vá para: `Settings` > `General` > `Danger Zone`
2. Confirme que o repositório está marcado como **Private**
3. Se estiver **Public**, clique em "Change visibility" e altere para **Private**

### 2. Proteção de Credenciais

❌ **NÃO FAÇA:**
- Compartilhar credenciais do Supabase
- Fazer fork público deste repositório
- Publicar o código em outros lugares
- Compartilhar screenshots com credenciais visíveis
- Comitar arquivos de configuração com credenciais reais

✅ **FAÇA:**
- Use credenciais diferentes para desenvolvimento e produção
- Rotacione credenciais periodicamente
- Use variáveis de ambiente quando possível
- Documente apenas com credenciais de exemplo

### 3. Controle de Acesso

✅ **Recomendações:**
- Limite o acesso ao repositório apenas para desenvolvedores autorizados
- Use autenticação de dois fatores (2FA) no GitHub
- Revise regularmente quem tem acesso ao repositório
- Use branch protection rules para branches principais

### 4. Gerenciamento de Chaves API

**Localização das credenciais no código:**
- `src/main.cpp` - linhas 34-35

```cpp
const char* SUPABASE_URL = "https://mkoqceekhnkpviixqnnk.supabase.co";
const char* SUPABASE_KEY = "sb_publishable_HLUfLEw2UuIWjzd5LfqLkw_oaodzV7V";
```

**⚠️ ATENÇÃO:**
- Esta é uma chave pública (anon key) do Supabase
- Mesmo sendo "pública", deve ser protegida junto com o código
- Configure Row Level Security (RLS) no Supabase para proteção adicional
- Monitore uso da API para detectar acessos não autorizados

---

## 🚨 O Que Fazer Se O Repositório Foi Tornado Público Acidentalmente

Se o repositório foi tornado público por engano:

1. **IMEDIATAMENTE:**
   - Torne o repositório privado novamente
   - Acesse o Supabase Dashboard

2. **ROTACIONE AS CREDENCIAIS:**
   - Gere novas chaves API no Supabase
   - Atualize as credenciais no código
   - Atualize todos os dispositivos ESP32 em campo

3. **REVISE OS LOGS:**
   - Verifique logs de acesso no GitHub
   - Verifique logs de uso da API no Supabase
   - Identifique possíveis acessos não autorizados

4. **NOTIFIQUE:**
   - Informe a equipe sobre o incidente
   - Documente o ocorrido
   - Implemente medidas para prevenir recorrência

---

## 📞 Reportando Problemas de Segurança

Se você identificar problemas de segurança:

1. **NÃO crie issues públicas** sobre vulnerabilidades
2. Entre em contato diretamente com os mantenedores
3. Aguarde confirmação antes de divulgar qualquer informação

---

## ✅ Checklist de Segurança

Antes de fazer qualquer commit ou push, verifique:

- [ ] O repositório está configurado como **Private**?
- [ ] Não estou commitando credenciais de produção?
- [ ] Revisei o código para não expor informações sensíveis?
- [ ] Não estou incluindo arquivos de configuração com dados reais?
- [ ] Atualizei a documentação apenas com credenciais de exemplo?

---

## 📚 Recursos Adicionais

- [GitHub: About repository visibility](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/managing-repository-settings/setting-repository-visibility)
- [Supabase: Row Level Security](https://supabase.com/docs/guides/auth/row-level-security)
- [OWASP: API Security Top 10](https://owasp.org/www-project-api-security/)

---

## 📅 Última Atualização

**Data:** 06 de Janeiro de 2026

**Revisão:** A política de segurança deve ser revisada a cada 6 meses ou quando houver mudanças significativas no projeto.
