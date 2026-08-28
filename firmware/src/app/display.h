// Camada de display: inicializa o painel (Arduino_GFX) e conecta o LVGL a ele.
// O resto do app so fala com LVGL; ninguem mais toca no driver do painel.
#pragma once

// Inicializa painel + backlight + LVGL (buffers e flush callback).
void display_begin(void);

// Ajusta o brilho do backlight (0..255).
void display_set_brightness(unsigned char level);
