# CONTEXT — onde estamos e como retomar

Arquivo de contexto do projeto Token Meter (ESP32 CYD). Leia para retomar em
outra sessão.

## Estado atual (2026-08-28)

Funciona ponta a ponta: daemon no Mac (BLE) → placa mostra o uso real do Claude.
Publicado (público) em https://github.com/lucasandradeb/token-meter-cyd.

Concluído:
- Bring-up (display ILI9341 `ips=true`, touch XPT2046 calibrado + NVS).
- UI LVGL: imagem no topo, card Sessão (grande) + Semana (compacto), cor por
  nível (verde/âmbar/vermelho), contagem "reseta em", indicador de conexão.
- BLE GATT recebendo `{session,weekly,s_reset,w_reset}`.
- Daemon Mac (Keychain → API → BLE), autostart via launchd; versão Linux
  (arquivo de credenciais) + systemd para host sempre-ligado.
- Imagem do topo customizável por imagem local do usuário (gitignored).

## O que está na placa vs no repo

- Na placa: imagem do topo = a imagem local do usuário (Fellowship), gerada por
  `tools/img_to_c.py` → `party_img_user.c/.h` (GITIGNORED, só no disco).
- No repo: arte padrão própria (`party_img.c`). Nenhum asset de terceiros.

## Pendências / próximos passos

1. **WiFi standalone (autonomia sem Mac/Pi)** — PLANEJADO, não implementado.
   Plano completo: `docs/specs/2026-08-28-wifi-standalone-plan.md`. Seguir a
   seção 8 (passo a passo). Ponto delicado: obter um token OAuth dedicado ao
   device sem deslogar o Mac (backup/restore do Keychain — seção 3 do plano).
2. **Segurança pessoal**: fazer sign-out das sessões do claude.ai (Settings) —
   um `sessionKey` apareceu num trace do navegador durante o desenvolvimento.
3. Opcional: host sempre-ligado (Pi) para autonomia sem risco — `daemon/README.md`.

## Como retomar rápido

- Diretrizes técnicas e "pega-ratões" da placa: `CLAUDE.md` (raiz).
- Build/flash: `~/.platformio/penv/bin/pio run -e app -t upload` (porta
  `/dev/cu.usbserial-110`).
- Design original: `docs/specs/2026-08-27-token-meter-cyd-design.md`.
- Plano WiFi: `docs/specs/2026-08-28-wifi-standalone-plan.md`.

## Decisões-chave (para não refazer discussão)

- Repo próprio inspirado no Clawdmeter (não fork) — Clawdmeter não tem licença
  aberta e bundla fontes/mascote proprietários. Este repo é MIT, sem esses.
- GIF animado descartado (pisca sem PSRAM); imagem estática é o caminho.
- Login OAuth na própria placa por script headless: BLOQUEADO pela atestação/
  captcha da Anthropic. Caminho viável = login oficial + isolamento (ver plano).
- Assets de terceiros nunca entram no repo; uso local do usuário é gitignored.
