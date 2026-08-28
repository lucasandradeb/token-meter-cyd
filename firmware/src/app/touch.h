// Camada de touch: le o XPT2046, calibra ADC->pixel e registra um input
// device no LVGL. A calibracao persiste na NVS (flash).
#pragma once

// Inicializa o XPT2046 (SPI proprio), carrega calibracao da NVS e cria o
// input device do LVGL. Chamar depois de display_begin().
void touch_begin(void);

// true se ha calibracao valida salva na NVS.
bool touch_is_calibrated(void);

// true se o painel esta sendo tocado neste instante (usado no boot para
// decidir recalibrar).
bool touch_pressed_now(void);

// Roda a rotina de calibracao de 4 cantos (bloqueante, desenha via LVGL),
// salva na NVS e termina com uma tela de verificacao. Chamar antes de ui_build.
void touch_run_calibration(void);
