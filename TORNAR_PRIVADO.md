# 🔒 Como Tornar o Repositório Privado / How to Make Repository Private

## 📋 Visão Geral / Overview

Este guia explica passo a passo como tornar este repositório (e outros repositórios seus) **PRIVADO** no GitHub.

**POR QUE É IMPORTANTE?**
- Este repositório contém credenciais de API (Supabase)
- Contém código proprietário
- Informações sensíveis sobre o sistema

---

## 🎯 Método 1: Via Interface Web do GitHub (Recomendado)

### Passo a Passo:

1. **Acesse o repositório no GitHub**
   - Vá para: `https://github.com/seu-usuario/Cadeira_odontologica`

2. **Abra as Configurações**
   - Clique na aba `Settings` (Configurações) no topo da página
   - **Nota:** Você precisa ser o proprietário ou ter permissões de administrador

3. **Role até a "Danger Zone" (Zona de Perigo)**
   - Role a página até o final
   - Procure pela seção vermelha chamada "Danger Zone"

4. **Altere a Visibilidade**
   - Encontre a opção "Change repository visibility"
   - Clique no botão "Change visibility"

5. **Selecione "Private"**
   - Uma janela popup aparecerá
   - Selecione a opção **"Make private"**
   - Digite o nome do repositório para confirmar (medida de segurança)
   - Clique em "I understand, change repository visibility"

6. **Confirme a Mudança**
   - O repositório agora está **PRIVADO** ✅
   - Um cadeado 🔒 aparecerá ao lado do nome do repositório

---

## 🎯 Método 2: Via GitHub CLI (Para Usuários Avançados)

Se você tem o GitHub CLI instalado:

```bash
# Instalar GitHub CLI (se ainda não tiver)
# Windows (via Chocolatey): choco install gh
# macOS: brew install gh
# Linux: sudo apt install gh

# Fazer login
gh auth login

# Tornar o repositório privado
gh repo edit --visibility private

# Verificar o status
gh repo view --json visibility
```

---

## 📱 Método 3: Via Aplicativo Móvel do GitHub

1. Abra o app GitHub no celular
2. Navegue até o repositório
3. Toque no ícone de ⚙️ (configurações)
4. Vá em "Settings"
5. Role até "Visibility"
6. Selecione "Private"
7. Confirme a mudança

---

## ✅ Como Verificar Se Está Privado

### Indicadores Visuais:

1. **Ícone de Cadeado** 🔒
   - Aparece ao lado do nome do repositório
   
2. **Badge "Private"**
   - Tag amarela escrita "Private" próxima ao nome

3. **Mensagem na Página**
   - "This is a private repository"

### Teste de Acesso:

1. **Abra uma aba anônima** no navegador
2. Tente acessar o repositório sem estar logado
3. Se aparecer **"404 - Not Found"** = ✅ Está privado!
4. Se você conseguir ver o código = ❌ Ainda está público

---

## 🔄 Tornar Múltiplos Repositórios Privados

### Via Interface Web:

Repita o processo acima para cada repositório.

### Via GitHub CLI (em lote):

```bash
# Listar seus repositórios públicos
gh repo list --visibility public

# Para cada repositório, execute:
gh repo edit usuario/nome-repo --visibility private
```

### Script para Automatizar (Bash):

```bash
#!/bin/bash

# Lista de repositórios para tornar privados
REPOS=(
    "usuario/Cadeira_odontologica"
    "usuario/outro-repo"
    "usuario/mais-um-repo"
)

for repo in "${REPOS[@]}"
do
    echo "Tornando $repo privado..."
    gh repo edit "$repo" --visibility private
    echo "✅ $repo agora é privado"
done

echo "Processo concluído!"
```

---

## ⚠️ Considerações Importantes

### O Que Acontece Ao Tornar Privado:

✅ **Vantagens:**
- Código não é mais visível publicamente
- Credenciais ficam protegidas
- Apenas colaboradores autorizados têm acesso
- Clones e forks públicos existentes ficam órfãos (não recebem atualizações)

❌ **Limitações:**
- Se alguém já clonou quando estava público, essa pessoa ainda tem uma cópia local
- Forks públicos existentes continuam públicos (você precisa pedir aos donos para deletá-los)
- Algumas features do GitHub podem ter limites em contas gratuitas

### Credenciais Expostas:

⚠️ **Se o repositório já foi público:**
1. As credenciais podem ter sido indexadas por buscadores
2. Alguém pode ter clonado o repositório
3. **AÇÃO NECESSÁRIA:**
   - Rotacione IMEDIATAMENTE as credenciais do Supabase
   - Gere novas chaves API
   - Atualize o código com as novas credenciais
   - Monitore logs de acesso no Supabase

---

## 🔐 Boas Práticas Após Tornar Privado

1. **Revise o Acesso**
   - Vá em `Settings` > `Manage access`
   - Remova colaboradores não autorizados
   - Use times para organizar permissões

2. **Configure Branch Protection**
   - Proteja a branch `main` ou `master`
   - Exija pull requests para mudanças
   - Ative revisão de código

3. **Ative 2FA (Autenticação de Dois Fatores)**
   - Protege sua conta GitHub
   - Reduz risco de acesso não autorizado

4. **Monitore Atividade**
   - Revise regularmente quem acessa o repositório
   - Verifique o histórico de commits
   - Use GitHub Security para alertas

---

## 📊 Checklist Final

Após tornar o repositório privado, verifique:

- [ ] Repositório mostra ícone de cadeado 🔒
- [ ] Badge "Private" está visível
- [ ] Teste em aba anônima confirma que está privado
- [ ] Removeu colaboradores desnecessários
- [ ] Ativou 2FA na conta GitHub
- [ ] Se necessário, rotacionou credenciais expostas
- [ ] Documentou quem tem acesso e por quê
- [ ] Configurou proteção de branches (opcional)
- [ ] Outros repositórios sensíveis também foram tornados privados

---

## 🆘 Precisa de Ajuda?

### Documentação Oficial GitHub:
- [Setting repository visibility](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/managing-repository-settings/setting-repository-visibility)
- [Permission levels for personal repositories](https://docs.github.com/en/account-and-profile/setting-up-and-managing-your-personal-account-on-github/managing-personal-account-settings/permission-levels-for-a-personal-account-repository)

### Suporte:
- GitHub Support: https://support.github.com/
- GitHub Community: https://github.community/

---

## 📅 Manutenção

**Revisões Periódicas:**
- Mensalmente: Revisar lista de colaboradores
- Trimestralmente: Verificar se repositório continua privado
- Anualmente: Rotacionar credenciais por precaução

---

**Última atualização:** 06 de Janeiro de 2026
