// Firmware completo.
// Passo 2: UI LVGL.  Passo 3: calibracao de touch.  Passo 4: BLE.
// O Mac (daemon, passo 5) escreve {"session":NN,"weekly":NN} por BLE.
#include <Arduino.h>
#include <lvgl.h>
#include "ble.h"
#include "display.h"
#include "touch.h"
#include "ui.h"

static bool got_real = false;   // true depois do 1o dado real via BLE

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n=== Token Meter (app) ===");

    display_begin();
    touch_begin();

    // Janela de boot: toque para (re)calibrar; calibra sozinho se nao houver
    // calibracao salva.
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
    ble_begin();
    ui_set_connected(false);
    Serial.println("UI pronta.");
}

void loop() {
    // Estado de conexao -> rodape.
    static bool last_conn = false;
    bool conn = ble_is_connected();
    if (conn != last_conn) {
        ui_set_connected(conn);
        last_conn = conn;
    }

    // Token expirado -> troca a animacao/numeros por um aviso; ao voltar,
    // restaura o rodape (o proximo dado real repovoa os cards).
    static bool last_auth_ok = true;
    bool auth_ok = ble_auth_ok();
    if (auth_ok != last_auth_ok) {
        if (!auth_ok) {
            ui_set_auth_expired();
            got_real = true;   // corta a onda de demo
        } else {
            ui_set_connected(conn);
        }
        last_auth_ok = auth_ok;
    }

    // Dados reais do BLE tem prioridade; ao chegar o 1o, para a animacao.
    int s, w, sr, wr;
    if (ble_get_usage(&s, &w, &sr, &wr)) {
        got_real = true;
        ui_set_usage(s, w, sr, wr);
    } else if (!got_real) {
        // Ate o 1o dado real: onda de exemplo so para a tela nao ficar parada.
        uint32_t t = millis() / 50;
        int session = (t % 200 < 100) ? (t % 100) : (100 - (t % 100));
        int weekly = ((t / 2) % 100);
        ui_set_usage(session, weekly, 0, 0);
    }

    ui_tick();          // atualiza a contagem regressiva de reset
    lv_timer_handler();
    delay(5);
}
