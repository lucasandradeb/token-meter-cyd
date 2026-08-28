# Token Meter (ESP32 CYD)

Medidor físico do seu uso de tokens do **Claude Code**, rodando numa placa
ESP32 de 2.8" (a "CYD"). Um daemon no seu Mac lê o uso real da conta e empurra
por Bluetooth para o dispositivo, que mostra o percentual da janela de uso em
tempo quase-real.

Projeto **inspirado no** [Clawdmeter](https://github.com/HermannBjorgvin/Clawdmeter)
de Hermann Björgvin. Reaproveita a ideia central (ler o uso dos headers de
rate-limit da API e transmitir por BLE), mas é código próprio, escrito para uma
placa diferente, e **não** redistribui nenhum asset proprietário (fontes ou
mascote da Anthropic). Licença MIT. Veja [Créditos](#créditos).

---

## Como funciona

```
┌─────────────── Mac ───────────────┐        ┌──────── ESP32 CYD ────────┐
│ daemon Python                      │        │ firmware (PlatformIO)     │
│  1. lê o token OAuth do Keychain   │  BLE   │  - servidor GATT (NimBLE) │
│  2. faz uma chamada mínima à API   │ ─────▶ │  - faz parse do JSON      │
│  3. extrai o uso dos headers de    │  GATT  │  - LVGL: barras de %      │
│     rate-limit da resposta         │        │  - touch XPT2046          │
│  4. empurra um JSON por BLE        │        │                           │
└────────────────────────────────────┘        └───────────────────────────┘
```

1. O daemon lê o token OAuth do Claude do **Keychain do macOS**.
2. Faz uma chamada mínima a `api.anthropic.com/v1/messages`.
3. O uso vem direto dos headers de resposta
   (`anthropic-ratelimit-unified-5h-utilization` e afins) — não gasta tokens de
   verdade para medir.
4. Envia `{ session_pct, weekly_pct }` por uma característica BLE GATT.
5. O ESP32 recebe e atualiza a tela.

O ESP32 **não** fala com a API nem tem seu token — todo segredo fica no Mac.

---

## Hardware

Placa **ESP32-2432S028** ("Cheap Yellow Display" / CYD):

| Componente | Detalhe |
|---|---|
| SoC | ESP32-WROOM-32 (dual-core 240 MHz, 520 KB RAM, sem PSRAM) |
| Flash | 4 MB |
| Display | 2.8" 320×240 SPI — ILI9341 ou ST7789 |
| Touch | XPT2046 resistivo (SPI próprio) |
| USB | CH340C → aparece como `/dev/cu.usbserial-*` no Mac |
| Rádio | Wi-Fi + Bluetooth (BLE 4.2) |

### Pinagem usada

```
Display (SPI):   SCK=IO14  MOSI=IO13  MISO=IO12  CS=IO15  DC=IO2  BL=IO21
Touch (XPT2046): CLK=IO25  MOSI=IO32  MISO=IO39  CS=IO33  IRQ=IO36
```

> **Display e touch estão em barramentos SPI separados.** O touch usa uma
> instância SPI própria — é um detalhe da CYD que costuma travar quem começa.

---

## Requisitos

- **VSCode** com a extensão **PlatformIO IDE** (recomendada automaticamente ao
  abrir o repo).
- **Python 3.11+** para o daemon.
- Um Mac (o daemon lê o Keychain do macOS). Linux/Windows exigiriam adaptar a
  leitura de credenciais.

---

## Começando

### 1. Firmware

```bash
# Abra a pasta do repo no VSCode. A extensão PlatformIO é sugerida
# automaticamente; instale-a.
```

No VSCode, na barra inferior do PlatformIO:

- **Selecione o ambiente** `bringup` (teste de hardware) ou `app` (firmware
  completo).
- **Build** (ícone de check) compila.
- **Upload** (seta →) grava na placa. A porta `/dev/cu.usbserial-*` é
  detectada sozinha.
- **Serial Monitor** (tomada) mostra os logs a 115200 baud.

Também dá para usar o terminal:

```bash
cd firmware
pio run -e bringup            # compila
pio run -e bringup -t upload  # grava
pio device monitor            # logs
```

### 2. Bring-up (faça isto primeiro)

O ambiente `bringup` é um teste isolado de hardware. Ao gravar, você deve ver:

- Barras coloridas + o texto "CYD bring-up OK" no display.
- Ao tocar a tela, um ponto amarelo aparece e as coordenadas cruas do touch
  saem no Serial Monitor.

**Se a tela ficar branca ou apagada:** o driver está errado. Abra
`firmware/src/bringup/main.cpp` e troque `DISPLAY_DRIVER` de `0` (ILI9341) para
`1` (ST7789). Cores invertidas: ajuste o parâmetro `ips`.

### 3. Daemon (Mac)

Lê seu uso do Claude e transmite por BLE para a placa.

```bash
cd daemon
python3 -m venv venv && source venv/bin/activate
pip install -r requirements.txt
python daemon.py
```

Pré-requisitos:
- Estar logado no **Claude Code** (o daemon lê o token OAuth do Keychain,
  serviço `Claude Code-credentials`).
- Conceder permissão de **Bluetooth** ao terminal/Python na primeira execução
  (macOS pede em Ajustes > Privacidade e Segurança > Bluetooth).

O daemon procura o dispositivo `TokenMeter`, conecta e envia
`{"session":NN,"weekly":NN}` a cada 30s. O token nunca sai da máquina nem vai
para log.

Ver [daemon/README.md](daemon/README.md) para rodar em segundo plano
(autostart via launchd).

---

## Ligando e levando pra outro lugar

A placa é alimentada por **micro-USB**. Não tem bateria — precisa estar
espetada numa fonte USB (5V).

**Em casa, perto do Mac:**
1. Ligue a placa em qualquer carregador USB de parede (ou no próprio Mac).
2. Deixe o daemon rodando no Mac (ele sobe sozinho no login via launchd).
3. Estando **no alcance do Bluetooth** (~10 m, mesmo ambiente), a placa mostra
   o uso ao vivo: rodapé "Conectado" e os cards atualizando a cada ~30s.

**Guardou tudo e levou pro escritório / outro lugar:**
- A placa **liga e mostra a UI**, mas fica em **"Aguardando..."** (sem números
  novos) porque não há fonte de dados por perto.
- Para mostrar dados fora de casa, você precisa de **uma fonte de dados no
  alcance BLE**:
  - levar o **Mac** junto (com o daemon rodando), ou
  - deixar um **host sempre-ligado** no local (Raspberry Pi/mini-PC com o
    daemon — veja `daemon/README.md`).
- Bluetooth é **ponto-a-ponto e de curto alcance**: a placa só recebe do
  host que estiver perto. Não funciona "pela internet" sozinha.

> Resumo: perto do Mac (ou do host), funciona plugando na tomada. Longe de
> qualquer host, mostra a interface mas sem uso atualizado.

## Mascote (pixel-art)

O topo mostra um pixel-art estático — uma companhia de aventureiros, arte
própria desenhada direto no `ui.cpp` (sprites char-map em `SP_*` + `PARTY[]`).
Editar = trocar os caracteres dos sprites (`.` = transparente; cores em
`party_color()`). Estático = sem piscar e cabe em largura cheia sem PSRAM.

> GIF animado foi descartado: sem PSRAM o decoder aloca `lado*lado*4` de RAM e a
> animação pisca. Pixel-art estático é o caminho estável nesta placa.

## Estrutura do repositório

```
esp32/
├── firmware/            # projeto PlatformIO
│   ├── platformio.ini   # ambientes bringup e app
│   └── src/
│       ├── bringup/     # teste isolado de display + touch (passo 1)
│       └── app/         # firmware completo (LVGL + BLE) — em construção
├── daemon/              # daemon Python do Mac — em construção
├── docs/specs/          # documento de design
└── README.md
```

---

## Roadmap

- [x] Identificar a placa e mapear a pinagem
- [x] Bring-up: teste de display + touch
- [x] UI LVGL com dados de exemplo
- [x] Calibração do touch XPT2046 (persistida na NVS)
- [x] Servidor BLE GATT no ESP32
- [x] Daemon do Mac (Keychain + API + BLE)
- [x] Autostart do daemon (launchd no Mac / systemd no Linux)
- [x] Polimento visual (cor por nível, mascote próprio, contagem de reset)
- [x] Host sempre-ligado (Raspberry Pi/Linux) para autonomia do Mac
- [ ] (bloqueado) Login OAuth dedicado na própria placa — atestação/captcha da Anthropic impede fluxo headless

---

## Créditos

- Inspirado no [Clawdmeter](https://github.com/HermannBjorgvin/Clawdmeter) de
  Hermann Björgvin — origem da arquitetura (daemon lê o uso dos headers de
  rate-limit e transmite por BLE).
- Este projeto **não** usa as fontes proprietárias da Anthropic nem o mascote
  Clawd; usa fontes livres e visual próprio.

## Licença

[MIT](LICENSE) — apenas código próprio.
