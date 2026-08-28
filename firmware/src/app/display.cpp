#include "display.h"
#include "pins.h"
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <lvgl.h>

// Barramento e driver do painel. ips=true: esta CYD vem com cores invertidas
// (confirmado no bring-up).
static Arduino_DataBus *bus =
    new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCK, TFT_MOSI, TFT_MISO);
static Arduino_GFX *gfx =
    new Arduino_ILI9341(bus, TFT_RST, 1 /*landscape*/, true /*ips*/);

// Buffer parcial do LVGL, medido em BYTES (RGB565 = 2 bytes/pixel). Sem
// PSRAM, mantemos pequeno: 24 linhas de largura cheia = 320*24*2 = 15360 B.
static const uint32_t LVGL_BUF_LINES = 24;
static uint8_t lvgl_buf[SCREEN_W * LVGL_BUF_LINES * 2];

// Fonte de tempo do LVGL 9: retorna millis desde o boot.
static uint32_t lvgl_tick_cb(void) { return millis(); }

// Envia o pedaco renderizado pelo LVGL para o painel. Na ESP32 (little-endian)
// o Arduino_GFX le o buffer RGB565 na ordem nativa do LVGL, entao NAO trocamos
// os bytes (o swap deixava as cores erradas).
static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area,
                          uint8_t *px_map) {
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;
    gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)px_map, w, h);
    lv_display_flush_ready(disp);
}

void display_begin(void) {
    // Backlight via LEDC (API do core 2.x).
    ledcSetup(0 /*canal*/, 5000 /*Hz*/, 8 /*bits*/);
    ledcAttachPin(TFT_BL, 0);
    ledcWrite(0, 255);

    gfx->begin();
    gfx->fillScreen(BLACK);

    lv_init();
    lv_tick_set_cb(lvgl_tick_cb);

    lv_display_t *disp = lv_display_create(SCREEN_W, SCREEN_H);
    lv_display_set_flush_cb(disp, lvgl_flush_cb);
    lv_display_set_buffers(disp, lvgl_buf, NULL, sizeof(lvgl_buf),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
}

void display_set_brightness(unsigned char level) { ledcWrite(0, level); }
