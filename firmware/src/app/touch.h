// Camada de touch: le o XPT2046 e registra um dispositivo de entrada no LVGL.
#pragma once

// Inicializa o XPT2046 (SPI proprio) e cria o input device do LVGL.
// Chamar depois de display_begin().
void touch_begin(void);
