// Firmware completo.
// Passo 2: UI LVGL (dados de exemplo).  Passo 3: calibracao de touch.
// Passo 4 (a seguir): BLE real substitui os dados de exemplo.
#include <Arduino.h>
#include <lvgl.h>
#include "display.h"
#include "touch.h"
#include "ui.h"

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n=== Token Meter (app) ===");

    display_begin();
    touch_begin();

    // Janela de boot: toque a tela para (re)calibrar. Calibra automaticamente
    // se ainda nao houver calibracao salva.
    Serial.println("Toque a tela agora para (re)calibrar (2s)...");
    bool recal = false;
    uint32_t until = millis() + 2000;
    while (millis() < until) {
        if (touch_pressed_now()) { recal = true; break; }
        delay(20);
    }
    if (recal || !touch_is_calibrated()) {
        touch_run_calibration();
    } else {
        Serial.println("Calibracao carregada da NVS.");
    }

    ui_build();
    ui_set_status("BLE: (exemplo)");
    Serial.println("UI pronta.");
}

void loop() {
    // Dados de EXEMPLO ate o BLE entrar (passo 4).
    uint32_t t = millis() / 50;
    int session = (t % 200 < 100) ? (t % 100) : (100 - (t % 100));
    int weekly = ((t / 2) % 100);
    ui_set_usage(session, weekly);

    lv_timer_handler();
    delay(5);
}
