#include "touch.h"
#include "pins.h"
#include <Arduino.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include <lvgl.h>

// Touch no seu proprio barramento SPI (VSPI) para nao colidir com o display.
static SPIClass touchSPI(VSPI);
static XPT2046_Touchscreen ts(TP_CS, TP_IRQ);

// Calibracao PROVISORIA (raw ADC -> pixel). Valores tipicos observados no
// bring-up; o passo 3 do projeto substitui por calibracao de 4 cantos.
static const int RAW_X_MIN = 200;
static const int RAW_X_MAX = 3800;
static const int RAW_Y_MIN = 200;
static const int RAW_Y_MAX = 3800;
// Pressao minima para considerar toque valido (filtra ruido/fantasma).
static const int Z_THRESHOLD = 400;

static void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data) {
    if (ts.touched()) {
        TS_Point p = ts.getPoint();
        if (p.z < Z_THRESHOLD) {
            data->state = LV_INDEV_STATE_RELEASED;
            return;
        }
        int x = map(p.x, RAW_X_MIN, RAW_X_MAX, 0, SCREEN_W - 1);
        int y = map(p.y, RAW_Y_MIN, RAW_Y_MAX, 0, SCREEN_H - 1);
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
    ts.setRotation(1);

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touch_read_cb);
}
