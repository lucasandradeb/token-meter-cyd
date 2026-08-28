// Camada de UI: telas LVGL no estilo "medidor" (dois cards com % grande, barra
// e contagem de reset), inspirado no Clawdmeter mas com arte propria.
#pragma once

// Cria a tela principal. Chamar depois de display e touch.
void ui_build(void);

// Atualiza os dados. session/weekly em 0..100; s_reset/w_reset em segundos ate
// o proximo reset (0 = desconhecido). Reinicia a contagem regressiva local.
void ui_set_usage(int session, int weekly, int s_reset, int w_reset);

// Atualiza o indicador de conexao (bolinha + rodape).
void ui_set_connected(bool connected);

// Recalcula a contagem regressiva de reset. Chamar a cada iteracao do loop.
void ui_tick(void);
