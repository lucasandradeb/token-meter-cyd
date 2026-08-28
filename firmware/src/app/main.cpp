// Firmware completo — passo 2: UI LVGL com dados de EXEMPLO.
// Os passos seguintes adicionam calibracao de touch (3) e BLE real (4);
// por ora os percentuais sao simulados para validar a interface.
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
    ui_build();
    ui_set_status("BLE: (exemplo)");

    Serial.println("UI pronta. Mostrando dados de exemplo.");
}

void loop() {
    // Dados de EXEMPLO: onda triangular lenta so para ver os arcos animando.
    uint32_t t = millis() / 50;             // ~20 passos/s
    int session = (t % 200 < 100) ? (t % 100) : (100 - (t % 100));
    int weekly = ((t / 2) % 100);
    ui_set_usage(session, weekly);

    lv_timer_handler();   // desenha o que mudou
    delay(5);
}
