// Camada de UI: constroi as telas LVGL e expoe funcoes para atualizar os
// dados. Nao conhece display nem touch nem BLE — so LVGL.
#pragma once

// Cria a tela principal (dois arcos: sessao e semana). Chamar depois do
// display e do touch.
void ui_build(void);

// Atualiza os percentuais mostrados (0..100).
void ui_set_usage(int session_pct, int weekly_pct);

// Atualiza o texto de status de conexao (ex: "BLE: conectado").
void ui_set_status(const char *text);
