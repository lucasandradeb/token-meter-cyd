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

> Em construção — adicionado após o firmware completo. Vai ler o Keychain,
> consultar a API e transmitir por BLE.

---

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
- [ ] UI LVGL com dados de exemplo
- [ ] Calibração do touch XPT2046
- [ ] Servidor BLE GATT no ESP32
- [ ] Daemon do Mac (Keychain + API + BLE)
- [ ] Visual próprio (fontes livres, paleta)

---

## Créditos

- Inspirado no [Clawdmeter](https://github.com/HermannBjorgvin/Clawdmeter) de
  Hermann Björgvin — origem da arquitetura (daemon lê o uso dos headers de
  rate-limit e transmite por BLE).
- Este projeto **não** usa as fontes proprietárias da Anthropic nem o mascote
  Clawd; usa fontes livres e visual próprio.

## Licença

[MIT](LICENSE) — apenas código próprio.
