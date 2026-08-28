// Bring-up da placa ESP32-2432S028 ("CYD").
//
// Objetivo: provar que o hardware funciona ANTES de investir no firmware
// completo (LVGL + BLE). Se algo aqui falhar, o problema e pino/driver/placa,
// nao logica de aplicacao.
//
// O que este sketch faz:
//   1. Liga o backlight e desenha barras de cor + texto no display.
//   2. Le o touch XPT2046 e imprime as coordenadas CRUAS no Serial Monitor,
//      alem de desenhar um ponto onde voce toca.
//
// Como usar no VSCode (PlatformIO):
//   - Ambiente "bringup" selecionado na barra inferior.
//   - Build (check) -> Upload (seta) -> Serial Monitor (tomada).
//
// TROUBLESHOOTING do display:
//   - Tela toda branca ou nada aparece  -> driver errado. Troque para ST7789
//     em DISPLAY_DRIVER abaixo (a CYD com USB-C usa ST7789; a de micro-USB
//     usa ILI9341).
//   - Cores invertidas (fundo deveria ser preto e esta branco, ou vermelho
//     aparece azul) -> ajuste o parametro IPS (true/false) na criacao do gfx.

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>

// ---- Escolha do driver do display -----------------------------------------
// 0 = ILI9341 (CYD micro-USB, o mais comum)
// 1 = ST7789  (CYD com USB-C)
#define DISPLAY_DRIVER 0

// ---- Pinos do display (SPI dedicado) --------------------------------------
#define TFT_SCK   14
#define TFT_MOSI  13
#define TFT_MISO  12
#define TFT_CS    15
#define TFT_DC     2
#define TFT_RST   -1   // RST ligado ao EN da placa (nao ha GPIO dedicado)
#define TFT_BL    21

// ---- Pinos do touch XPT2046 (SPI SEPARADO do display) ---------------------
#define TP_SCK    25
#define TP_MOSI   32
#define TP_MISO   39
#define TP_CS     33
#define TP_IRQ    36

// Barramento SPI do display (HSPI). O touch usa um SPIClass proprio (VSPI).
Arduino_DataBus *bus = new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCK, TFT_MOSI, TFT_MISO);

#if DISPLAY_DRIVER == 0
Arduino_GFX *gfx = new Arduino_ILI9341(bus, TFT_RST, 1 /*rotation landscape*/, true /*ips: painel desta CYD vem com cores invertidas*/);
const char *DRIVER_NAME = "ILI9341";
#else
Arduino_GFX *gfx = new Arduino_ST7789(bus, TFT_RST, 1 /*rotation landscape*/, true /*ips*/);
const char *DRIVER_NAME = "ST7789";
#endif

// Touch no seu proprio barramento SPI para nao colidir com o display.
SPIClass touchSPI(VSPI);
XPT2046_Touchscreen touch(TP_CS, TP_IRQ);

void drawTestPattern() {
    int16_t w = gfx->width();
    int16_t h = gfx->height();

    // Barras de cor para conferir o painel e as cores.
    uint16_t colors[] = {RED, GREEN, BLUE, WHITE, BLACK};
    int16_t barW = w / 5;
    for (int i = 0; i < 5; i++) {
        gfx->fillRect(i * barW, 0, barW, h / 2, colors[i]);
    }

    // Texto de status.
    gfx->setTextColor(WHITE, BLACK);
    gfx->setTextSize(2);
    gfx->setCursor(6, h / 2 + 10);
    gfx->print("CYD bring-up OK");
    gfx->setTextSize(1);
    gfx->setCursor(6, h / 2 + 40);
    gfx->printf("Driver: %s  %dx%d", DRIVER_NAME, w, h);
    gfx->setCursor(6, h / 2 + 55);
    gfx->print("Toque na tela -> ponto + Serial");
}

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println();
    Serial.println("=== CYD bring-up ===");
    Serial.printf("Display driver: %s\n", DRIVER_NAME);

    // Backlight via PWM (LEDC), API do Arduino-ESP32 core 2.x:
    // ledcSetup(canal, freq, bits) + ledcAttachPin(pino, canal).
    ledcSetup(0 /*canal*/, 5000 /*Hz*/, 8 /*bits*/);
    ledcAttachPin(TFT_BL, 0 /*canal*/);
    ledcWrite(0 /*canal*/, 255);

    if (!gfx->begin()) {
        Serial.println("ERRO: gfx->begin() falhou");
    }
    gfx->fillScreen(BLACK);
    drawTestPattern();

    // Touch no barramento VSPI com os pinos do XPT2046.
    touchSPI.begin(TP_SCK, TP_MISO, TP_MOSI, TP_CS);
    touch.begin(touchSPI);
    touch.setRotation(1);
    Serial.println("Setup completo. Toque na tela.");
}

void loop() {
    if (touch.touched()) {
        TS_Point p = touch.getPoint();
        // Coordenadas CRUAS (ADC ~200..3800). A calibracao para pixels vem
        // no passo seguinte do projeto.
        Serial.printf("touch raw: x=%4d y=%4d z=%4d\n", p.x, p.y, p.z);

        // Desenha um ponto proporcional na metade superior so para feedback
        // visual (mapeamento grosseiro, NAO calibrado).
        int16_t sx = map(p.x, 200, 3800, 0, gfx->width());
        int16_t sy = map(p.y, 200, 3800, 0, gfx->height());
        gfx->fillCircle(sx, sy, 3, YELLOW);
    }
    delay(20);
}
