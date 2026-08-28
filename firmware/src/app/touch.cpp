#include "touch.h"
#include "pins.h"
#include <Arduino.h>
#include <Preferences.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include <lvgl.h>

// Touch no seu proprio barramento SPI (VSPI) para nao colidir com o display.
static SPIClass touchSPI(VSPI);
static XPT2046_Touchscreen ts(TP_CS, TP_IRQ);

static Preferences prefs;

// Modelo de calibracao: qual eixo cru corresponde ao X da tela (swap), e o
// mapeamento linear cru->pixel por eixo (map() lida com inversao pela ordem).
struct Calib {
    bool valid = false;
    bool swap = false;   // true: X da tela usa p.y do touch
    int axA = 200, axB = 3900;   // cru para X da tela em (inset .. W-1-inset)
    int ayA = 200, ayB = 3900;   // cru para Y da tela
};
static Calib cal;

static const int Z_THRESHOLD = 400;  // pressao minima = toque valido
static const int INSET = 28;         // margem dos alvos de calibracao

// ---- NVS ------------------------------------------------------------------
static void calib_load(void) {
    prefs.begin("touchcal", true /*readonly*/);
    cal.valid = prefs.getBool("valid", false);
    if (cal.valid) {
        cal.swap = prefs.getBool("swap", false);
        cal.axA = prefs.getInt("axA", 200);
        cal.axB = prefs.getInt("axB", 3900);
        cal.ayA = prefs.getInt("ayA", 200);
        cal.ayB = prefs.getInt("ayB", 3900);
    }
    prefs.end();
}

static void calib_save(void) {
    prefs.begin("touchcal", false);
    prefs.putBool("valid", true);
    prefs.putBool("swap", cal.swap);
    prefs.putInt("axA", cal.axA);
    prefs.putInt("axB", cal.axB);
    prefs.putInt("ayA", cal.ayA);
    prefs.putInt("ayB", cal.ayB);
    prefs.end();
    cal.valid = true;
}

// ---- Leitura crua ---------------------------------------------------------
// Le uma amostra crua (media de N leituras) enquanto pressionado.
static bool read_raw(int *rx, int *ry, int *rz) {
    if (!ts.touched()) return false;
    TS_Point p = ts.getPoint();
    if (p.z < Z_THRESHOLD) return false;
    *rx = p.x;
    *ry = p.y;
    *rz = p.z;
    return true;
}

// ---- LVGL indev -----------------------------------------------------------
static void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data) {
    int rx, ry, rz;
    if (cal.valid && read_raw(&rx, &ry, &rz)) {
        int forX = cal.swap ? ry : rx;
        int forY = cal.swap ? rx : ry;
        int x = map(forX, cal.axA, cal.axB, INSET, SCREEN_W - 1 - INSET);
        int y = map(forY, cal.ayA, cal.ayB, INSET, SCREEN_H - 1 - INSET);
        data->point.x = constrain(x, 0, SCREEN_W - 1);
        data->point.y = constrain(y, 0, SCREEN_H - 1);
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

void touch_begin(void) {
    touchSPI.begin(TP_SCK, TP_MISO, TP_MOSI, TP_CS);
    ts.begin(touchSPI);
    ts.setRotation(0);   // cru; a calibracao absorve orientacao/swap
    calib_load();

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touch_read_cb);
}

bool touch_is_calibrated(void) { return cal.valid; }

bool touch_pressed_now(void) {
    int rx, ry, rz;
    return read_raw(&rx, &ry, &rz);
}

// ---- Rotina de calibracao -------------------------------------------------
// Espera um toque estavel, retorna a media crua daquele toque, depois espera
// o release.
static void wait_capture(int *rx, int *ry) {
    // Espera pressionar.
    int x, y, z;
    while (!read_raw(&x, &y, &z)) {
        lv_timer_handler();
        delay(10);
    }
    // Media de 16 amostras estaveis.
    long sx = 0, sy = 0;
    int n = 0;
    for (int i = 0; i < 16; i++) {
        if (read_raw(&x, &y, &z)) {
            sx += x;
            sy += y;
            n++;
        }
        delay(10);
    }
    *rx = n ? (int)(sx / n) : x;
    *ry = n ? (int)(sy / n) : y;
    // Espera soltar (debounce).
    while (ts.touched()) {
        lv_timer_handler();
        delay(10);
    }
    delay(200);
}

void touch_run_calibration(void) {
    Serial.println("[calib] iniciando calibracao de 4 cantos");

    // Tela dedicada de calibracao.
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0E1116), 0);
    lv_screen_load(scr);

    lv_obj_t *info = lv_label_create(scr);
    lv_obj_set_style_text_color(info, lv_color_hex(0xE6EAF0), 0);
    lv_obj_align(info, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_text(info, "Calibracao\nToque nos alvos");
    lv_obj_set_style_text_align(info, LV_TEXT_ALIGN_CENTER, 0);

    // Alvo reutilizavel (circulo).
    lv_obj_t *tgt = lv_obj_create(scr);
    lv_obj_set_size(tgt, 24, 24);
    lv_obj_set_style_radius(tgt, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(tgt, lv_color_hex(0xF2A33C), 0);
    lv_obj_set_style_border_width(tgt, 2, 0);
    lv_obj_set_style_border_color(tgt, lv_color_hex(0xFFFFFF), 0);

    // 4 cantos: TL, TR, BR, BL (em pixels).
    const int px[4] = {INSET, SCREEN_W - 1 - INSET, SCREEN_W - 1 - INSET, INSET};
    const int py[4] = {INSET, INSET, SCREEN_H - 1 - INSET, SCREEN_H - 1 - INSET};
    int rx[4], ry[4];

    for (int i = 0; i < 4; i++) {
        // Posiciona o alvo (canto superior-esq do obj = centro - 12).
        lv_obj_set_pos(tgt, px[i] - 12, py[i] - 12);
        lv_label_set_text_fmt(info, "Calibracao %d/4\nToque no alvo", i + 1);
        lv_timer_handler();
        delay(300);
        wait_capture(&rx[i], &ry[i]);
        Serial.printf("[calib] canto %d px(%d,%d) raw(%d,%d)\n", i + 1, px[i],
                      py[i], rx[i], ry[i]);
    }

    // Detecta swap: qual eixo cru varia mais ao mover no X da tela (TL->TR).
    int dX_rx = abs(rx[1] - rx[0]);
    int dX_ry = abs(ry[1] - ry[0]);
    cal.swap = (dX_ry > dX_rx);

    auto forX = [&](int i) { return cal.swap ? ry[i] : rx[i]; };
    auto forY = [&](int i) { return cal.swap ? rx[i] : ry[i]; };

    // axA = cru-para-X em screenX=INSET (media TL,BL); axB em screenX=max (TR,BR).
    cal.axA = (forX(0) + forX(3)) / 2;
    cal.axB = (forX(1) + forX(2)) / 2;
    // ayA = cru-para-Y em screenY=INSET (TL,TR); ayB em screenY=max (BR,BL).
    cal.ayA = (forY(0) + forY(1)) / 2;
    cal.ayB = (forY(2) + forY(3)) / 2;

    calib_save();
    Serial.printf("[calib] swap=%d axA=%d axB=%d ayA=%d ayB=%d\n", cal.swap,
                  cal.axA, cal.axB, cal.ayA, cal.ayB);

    // Verificacao: ponto segue o dedo por ~4s.
    lv_label_set_text(info, "OK! Toque para testar\n(segue o dedo)");
    lv_obj_align(info, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_add_flag(tgt, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *dot = lv_obj_create(scr);
    lv_obj_set_size(dot, 14, 14);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, lv_color_hex(0x3DB7A8), 0);
    lv_obj_add_flag(dot, LV_OBJ_FLAG_HIDDEN);

    uint32_t end = millis() + 4000;
    while (millis() < end) {
        int a, b, z;
        if (read_raw(&a, &b, &z)) {
            int fx = cal.swap ? b : a;
            int fy = cal.swap ? a : b;
            int x = constrain(map(fx, cal.axA, cal.axB, INSET, SCREEN_W - 1 - INSET), 0, SCREEN_W - 1);
            int y = constrain(map(fy, cal.ayA, cal.ayB, INSET, SCREEN_H - 1 - INSET), 0, SCREEN_H - 1);
            lv_obj_clear_flag(dot, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_pos(dot, x - 7, y - 7);
            end = millis() + 4000;  // estende enquanto toca
        }
        lv_timer_handler();
        delay(10);
    }

    lv_obj_delete(scr);  // limpa a tela de calibracao
}
