# Token Meter CYD — Design Spec

**Data:** 2026-08-27
**Autor:** Lucas Andrade
**Status:** Aprovado para planejamento

## 1. Objetivo

Medidor físico de uso de tokens do Claude Code, rodando em uma placa
**ESP32-2432S028 ("CYD" — Cheap Yellow Display)**. Um daemon no Mac lê o uso
real da conta e empurra para o dispositivo, que mostra o percentual de uso em
tempo quase-real numa tela de 2.8".

Projeto **inspirado no** [Clawdmeter](https://github.com/HermannBjorgvin/Clawdmeter),
não um fork. Reusa a arquitetura e a técnica de leitura de uso; escreve firmware
próprio para a CYD e não redistribui nenhum asset proprietário (fontes Anthropic,
mascote Clawd).

### Não-objetivos (YAGNI)
- Sem camada HAL multi-board: só existe uma placa (a CYD). Código direto.
- Sem OTA update.
- Sem áudio, SD card, LDR, LED RGB no MVP (a CYD tem, mas não são necessários).
- Sem mascote animado no MVP (decisão de visual próprio/neutro).

## 2. Hardware alvo

**ESP32-2432S028** (identificado pela pinagem dos PDFs do usuário):

| Componente | Detalhe |
|---|---|
| SoC | ESP32-WROOM-32 (dual-core LX6 240 MHz, 520 KB SRAM, **sem PSRAM**) |
| Flash | 4 MB |
| Display | 2.8" 320×240, SPI 4-wire — **ILI9341 ou ST7789** (a confirmar no bring-up) |
| Touch | XPT2046 resistivo (barramento SPI próprio) |
| USB-Serial | CH340C → `/dev/cu.usbserial-110` no Mac |
| BT | Bluetooth Classic + BLE 4.2 |

### Pinagem (extraída dos PDFs do usuário)
```
Display (SPI):   SCK=IO14  MOSI=IO13  MISO=IO12  CS=IO15  DC/RS=IO2  BL=IO21
Touch (XPT2046): CLK=IO25  MOSI=IO32  MISO=IO39  CS=IO33  IRQ=IO36
SD (não usado):  CLK=IO18  MISO=IO19  MOSI=IO23  CS=IO5
```

### Riscos de hardware
- **Driver do display**: ILI9341 (CYD micro-USB) vs ST7789 (revisões micro-USB+USB-C).
  Resolvido no bring-up testando os dois. Sintoma de driver errado: tela branca
  ou cores invertidas.
- **Touch XPT2046**: entrega ADC cru (~200–3800), não pixels. Precisa calibração
  afim (offset+escala por eixo) e possível troca/inversão de eixos. Único
  componente sem referência direta no Clawdmeter (que usa touch capacitivo I2C).
- **Sem PSRAM**: framebuffer LVGL parcial em SRAM interna (buffers pequenos).
- **Flash 4 MB**: firmware (~1.2–1.5 MB com LVGL+BLE) não cabe no layout default;
  usar partição `huge_app.csv`.

## 3. Arquitetura

```
┌─────────────── Mac ───────────────┐        ┌──────── ESP32 CYD ────────┐
│ daemon Python                      │        │ firmware (PlatformIO)     │
│  - lê token OAuth do Keychain      │  BLE   │  - GATT server (NimBLE)   │
│  - chama api.anthropic.com/v1/msg  │ ─────▶ │  - parse JSON             │
│  - extrai uso dos headers de       │  GATT  │  - LVGL: barras/arcos de %│
│    rate-limit                      │        │  - touch XPT2046          │
│  - empurra JSON por BLE            │        │                           │
└────────────────────────────────────┘        └───────────────────────────┘
```

**Fluxo de dados:** headers `anthropic-ratelimit-unified-5h-utilization` (e
similares para janela semanal) → JSON `{session_pct, weekly_pct, ts}` → BLE
characteristic (write/notify) → LVGL atualiza a UI.

### Componentes e responsabilidades

**Firmware (`firmware/`)** — projeto PlatformIO, unidades isoladas:
- `display.*` — init do painel (Arduino_GFX) + glue LVGL (flush_cb usando buffer
  parcial). Interface: `display_begin()`, `display_flush(area, pixels)`.
- `touch.*` — driver XPT2046 (SPI dedicado) + calibração → coordenadas de tela.
  Interface: `touch_read(&x, &y, &pressed)`.
- `ble.*` — GATT server NimBLE, expõe characteristic de dados; callback ao receber
  JSON. Interface: `ble_begin()`, `ble_on_data(cb)`.
- `ui.*` — telas LVGL (uso da sessão, uso semanal, status BLE). Board-agnostic o
  suficiente para rodar no simulador depois, se quisermos.
- `main.cpp` — setup + loop: `lv_timer_handler()`, `touch_read`, alimenta LVGL.

**Daemon (`daemon/`)** — Python, adaptado do Clawdmeter com atribuição:
- Leitura do Keychain (macOS) do token OAuth.
- Chamada mínima à API, extração dos headers.
- Push BLE via `bleak`.
- Config: intervalo de polling, UUID do dispositivo.

**Bring-up (`bringup/`)** — env PlatformIO separado (ou sketch) descartável:
teste isolado de display + touch antes do firmware completo.

## 4. Layout do repositório

```
esp32/
├── README.md              # descrição, setup, atribuição ao Clawdmeter
├── LICENSE                # MIT (código próprio; sem assets proprietários)
├── .gitignore             # .pio/, venv/, build/, credenciais
├── docs/
│   └── specs/             # este documento
├── firmware/
│   ├── platformio.ini     # env cyd_2432s028 + env bringup
│   ├── src/
│   │   ├── main.cpp
│   │   ├── display.cpp/.h
│   │   ├── touch.cpp/.h
│   │   ├── ble.cpp/.h
│   │   └── ui.cpp/.h
│   └── lv_conf (via build_flags)
├── daemon/
│   ├── daemon.py
│   ├── requirements.txt
│   └── README.md
└── bringup/               # sketch/env de teste isolado
```

## 5. Stack de bibliotecas (firmware)

- `moononournation/GFX Library for Arduino` — driver ILI9341/ST7789 SPI.
- `lvgl/lvgl@^9.2` — UI. Config via build_flags (`-DLV_CONF_SKIP`, `LV_COLOR_DEPTH=16`,
  `LV_USE_SNAPSHOT=0` por falta de PSRAM).
- `h2zero/NimBLE-Arduino` — BLE (leve, cabe no WROOM-32).
- `bblanchon/ArduinoJson@^7` — parse do payload.
- Touch: lib MIT (`PaulStoffregen/XPT2046_Touchscreen`) ou leitor SPI inline —
  evitar libs GPL para manter o repo permissivo (MIT).

## 6. platformio.ini (esboço do env)

```ini
[env:cyd_2432s028]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200
board_build.partitions = huge_app.csv
build_flags =
    -DLV_CONF_SKIP
    -DLV_COLOR_DEPTH=16
    -DLV_USE_SNAPSHOT=0
    -DLV_TICK_CUSTOM=1
    ; (flags LVGL de widgets: BAR, LABEL, ARC, etc.)
lib_deps =
    moononournation/GFX Library for Arduino
    lvgl/lvgl@^9.2.0
    h2zero/NimBLE-Arduino
    bblanchon/ArduinoJson@^7.0.0
```

## 7. Plano de execução (incremental, cada passo verificável)

1. **Setup VSCode + bring-up**: instalar extensão PlatformIO; env `bringup` que
   pinta cor sólida (confirma driver ILI9341 vs ST7789 e backlight) e printa
   coordenadas cruas do touch no Serial. **Critério:** tela colorida + toques
   logados.
2. **Display + UI fake**: firmware desenha barras/arcos de % com dados mockados
   via LVGL. **Critério:** UI renderiza a 320×240.
3. **Touch + calibração**: XPT2046 mapeado para pixels; interação básica.
   **Critério:** toque na tela bate com o ponto correto.
4. **BLE**: GATT server, characteristic de dados, recebe JSON de um cliente de
   teste. **Critério:** JSON enviado pelo Mac aparece na tela.
5. **Daemon Mac**: adaptar o daemon do Clawdmeter (Keychain + API + push bleak).
   **Critério:** uso real da conta reflete na tela.
6. **Polimento**: visual próprio (fonte livre, paleta), README, LICENSE, commit.

## 8. Tudo acessível pelo VSCode

Extensão **PlatformIO IDE** no VSCode: build/upload/monitor por botões da barra
inferior. Porta `/dev/cu.usbserial-110` auto-detectada. Daemon Python roda em
terminal integrado ou task do VSCode.

## 9. Considerações legais

- Repo licenciado **MIT** (código próprio).
- **Nenhum** asset proprietário da Anthropic (fontes Tiempos/Styrene, mascote
  Clawd). Fontes livres (Inter/JetBrains Mono) e visual próprio.
- README credita o Clawdmeter como inspiração + link.
- Touch driver permissivo (MIT/inline), sem contaminação GPL.
