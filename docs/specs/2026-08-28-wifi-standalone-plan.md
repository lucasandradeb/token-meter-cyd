# Plano: modo WiFi standalone (ESP32 autônoma, sem Mac/Pi por perto)

**Data:** 2026-08-28
**Status:** planejado (não implementado)
**Objetivo:** a placa, ligada só na tomada, conecta no WiFi, consulta a API do
Claude sozinha e mostra o uso — sem depender do Mac nem de um Raspberry Pi por
perto.

## 1. Contexto e bloqueios (por que não é trivial)

A métrica (uso 5h/7d da assinatura) só vem nos headers de rate-limit de uma
chamada autenticada com o **token OAuth do Claude Code** — uma API key comum
mostra limites de API pré-pago, não a assinatura.

Dois fatos duros:
- **Access token expira em ~6 h.** Um token fixo na placa para de funcionar
  depois disso.
- **Renovar exige o refresh token.** O refresh rotaciona a cada uso. Se a placa
  usar o mesmo token do Mac, ela invalida o do Mac → **desloga o Claude Code do
  Mac**.

Conclusão: a placa precisa de um **login OAuth próprio** (sessão independente),
que ela renova sozinha sem afetar o Mac.

## 2. Estratégia

1. Criar uma **sessão OAuth dedicada** ao device (independente da do Mac).
2. Gravar `access_token` + `refresh_token` na **NVS** da placa (provisionamento
   por serial, uma vez).
3. Firmware **modo WiFi**: conecta no WiFi, chama a API, e **renova** o token
   sozinho quando expira (usando o refresh token dedicado, salvando o novo na
   NVS). Rotação só afeta a sessão do device.
4. Re-provisionar ~a cada 20 dias (validade do refresh token) ou quando quebrar.

### Fatos técnicos confirmados
- Token endpoint: `https://console.anthropic.com/v1/oauth/token`
  (alternativo: `https://platform.claude.com/v1/oauth/token`).
- `client_id`: `9d1c250a-e61b-44d9-88ed-5944d1962f5e` (Claude Code, público).
- Refresh grant (JSON): `{ "grant_type":"refresh_token", "refresh_token":"…",
  "client_id":"…" }`; headers `Content-Type: application/json` e
  `User-Agent: anthropic`. Resposta traz `access_token`, `refresh_token` (novo),
  `expires_in`.
- Uso: POST `https://api.anthropic.com/v1/messages` (body mínimo,
  `max_tokens:1`, headers `anthropic-version: 2023-06-01`,
  `anthropic-beta: oauth-2025-04-20`, `Authorization: Bearer <access>`).
  Uso vem dos headers `anthropic-ratelimit-unified-5h/7d-utilization` (fração
  0..1) e `-reset` (epoch).
- Validades observadas: access ~6 h, refresh ~20 dias.
- macOS: o token do Claude Code fica **só no Keychain** (serviço
  `Claude Code-credentials`), sem arquivo — importante para o passo 3.

## 3. Aquisição do token dedicado (parte delicada)

O fluxo PKCE headless é barrado pela **atestação/captcha** da Anthropic (testado:
o approve no claude.ai falha por fora do app oficial). Então usar o **cliente
oficial** para gerar a sessão, e isolar via backup/restore do Keychain:

1. **Backup** da sessão atual do Mac (sessão A):
   `security find-generic-password -s "Claude Code-credentials" -a $USER -w`
   → salvar o blob num arquivo temporário seguro.
2. **Login dedicado**: `claude login` (navegador, passa o captcha) → o Keychain
   agora tem uma **nova sessão B**.
3. **Capturar B**: ler o Keychain de novo → guardar `access_token`/`refresh_token`
   de B para provisionar a placa.
4. **Restaurar A** no Keychain (`security add-generic-password -U …` com o blob
   do passo 1) → o Mac volta a usar a sessão A.
5. **Verificar** que o Claude Code do Mac ainda funciona.

Resultado: device com sessão B, Mac com sessão A, independentes. A placa
renovando B não toca em A.

**Risco principal:** se a conta tiver política de sessão única, criar B pode
invalidar A no servidor (o restore local não resolveria). Mitigação: o backup
permite ao menos tentar; se A morrer, refazer `claude login` no Mac (sessão C).
Fazer isso num momento em que um re-login no Mac não seja problema.

## 4. Firmware — modo WiFi

Novo módulo `firmware/src/app/net.*`:
- `net_provisioned()` — há WiFi + refresh token na NVS?
- `net_begin()` — lê NVS, `WiFi.begin(ssid, pass)`.
- `net_poll_usage(&session,&weekly,&s_reset,&w_reset)`:
  - garante access token válido (renova se `expires_at` próximo ou em 401);
  - `WiFiClientSecure` (TLS) → POST `/v1/messages`; extrai headers.
- `net_refresh()` — POST no token endpoint com o refresh token; salva
  `access`, novo `refresh`, `expires_at` na NVS.

TLS: usar `WiFiClientSecure`. Início simples com `setInsecure()` (sem validar
CA) — aceitável para projeto pessoal; endurecer depois embutindo a CA raiz.

**Seleção de modo (uma firmware, dois modos):**
- Se `net_provisioned()` → **modo WiFi** (não inicia BLE — economiza RAM e evita
  coexistência WiFi+BLE).
- Senão → **modo BLE** atual (daemon).

**Memória (sem PSRAM):** TLS handshake custa ~40 KB de heap. Sem BLE no modo
WiFi, sobra folga (LVGL já roda em ~113 KB estáticos + buffers no heap).
Validar com `ESP.getFreeHeap()` após o handshake.

## 5. Provisionamento (uma vez, por serial)

Script `tools/provision_wifi.py` (roda no Mac):
- Lê as credenciais do device do passo 3 (arquivo local gitignored).
- Pergunta SSID e senha do WiFi (senha só no processo, nunca commitada).
- Abre a serial e envia uma linha `PROV {json}` com `{ssid, pass, access_token,
  refresh_token, expires_at}`.
- Firmware, num modo de provisionamento (ou escutando `PROV ` na serial no
  boot), grava tudo na NVS e reinicia.

Alternativa à serial: BLE (reusar a característica GATT) — mais trabalho.

## 6. Segurança

- O token OAuth passa a viver na **placa** (NVS não é criptografada por padrão).
  Se a placa for roubada/inspecionada, o token vaza. Aceitável para um totem
  doméstico; pensar duas vezes num escritório.
- Mitigar: habilitar **NVS encryption + flash encryption** do ESP32 (mais
  trabalho); ou aceitar o risco e revogar o login do device se sumir.
- Nunca logar tokens. Nunca commitar credenciais (já gitignored).
- O login do device é **separado** — revogá-lo não afeta o Mac.

## 7. Riscos e fallbacks

| Risco | Mitigação |
|---|---|
| Login B invalida A (sessão única) | Backup de A; se morrer, re-login no Mac |
| Endpoint/flow OAuth muda | Isolar em `net.cpp`; documentar; refazer login |
| TLS/RAM estoura sem BLE | Medir heap; reduzir buffers LVGL se preciso |
| Refresh token expira (~20 d) | Placa mostra "expirado"; re-provisionar |
| NVS token vaza | Flash/NVS encryption; revogar sessão do device |

## 8. Passo a passo (implementação futura)

1. `net.*`: WiFi + TLS + POST usage + parse headers (modo WiFi isolado).
2. `net_refresh()` + persistência de token na NVS (access/refresh/expires_at).
3. Seleção de modo em `main.cpp` (WiFi se provisionado; senão BLE).
4. `tools/provision_wifi.py` + receptor `PROV` na serial do firmware.
5. Procedimento de token dedicado (seção 3) com backup/restore do Keychain.
6. Testar: provisionar, desligar o Mac, confirmar que a placa atualiza sozinha
   por várias horas (passando por ≥1 refresh).
7. (Opcional) endurecer: validar CA no TLS; NVS/flash encryption.

## 9. Alternativa mais segura (lembrete)

Se o risco ao login do Mac incomodar: **daemon num host sempre-ligado** (Pi/
mini-PC) roda o Claude Code oficial (renova sozinho) e repassa por BLE. Zero
reverse-engineering, zero risco. Já está pronto (ver `daemon/README.md`).
