// Camada de UI: telas LVGL no estilo "medidor" (dois cards com % grande, barra
// e contagem de reset), inspirado no Clawdmeter mas com arte propria.
#pragma once

// Cria a tela principal. Chamar depois de display e touch.
void ui_build(void);

// Atualiza os dados. session/weekly em 0..100; s_reset_str/w_reset_str sao o
// horario de reset ja formatado pelo Mac (ex: "15:42", "qua 09:00", "--").
void ui_set_usage(int session, int weekly, const char *s_reset_str,
                  const char *w_reset_str);

// Atualiza o indicador de conexao (bolinha + rodape).
void ui_set_connected(bool connected);

// Mostra o estado de token expirado: zera os cards e poe um aviso vermelho no
// rodape. Chamar quando o Mac sinaliza auth=0.
void ui_set_auth_expired(void);
