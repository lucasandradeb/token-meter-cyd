// Camada BLE: servidor GATT que recebe o uso enviado pelo Mac.
//
// Concorrencia: os callbacks BLE rodam em outra task. Esta camada apenas
// guarda os ultimos valores; o loop principal (task do LVGL) os consome via
// ble_get_usage(). Nunca chame LVGL a partir daqui.
#pragma once
#include <stddef.h>   // size_t (header incluido antes de Arduino.h em ble.cpp)

// Inicializa NimBLE, cria o servico/caracteristica e comeca a anunciar.
void ble_begin(void);

// Se chegou uma nova leitura desde a ultima chamada, preenche os valores e
// retorna true. session/weekly em 0..100; s_reset_str/w_reset_str recebem o
// horario de reset ja formatado pelo Mac (ex: "15:42", "qua 09:00", "--"),
// gravado em buffers de tamanho n.
bool ble_get_usage(int *session, int *weekly, char *s_reset_str,
                   char *w_reset_str, size_t n);

// true enquanto houver um cliente (o Mac) conectado.
bool ble_is_connected(void);

// false depois que o Mac avisa que o token OAuth expirou ({"auth":0}); volta a
// true quando chega uso valido de novo.
bool ble_auth_ok(void);
