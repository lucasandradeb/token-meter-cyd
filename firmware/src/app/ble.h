// Camada BLE: servidor GATT que recebe o uso enviado pelo Mac.
//
// Concorrencia: os callbacks BLE rodam em outra task. Esta camada apenas
// guarda os ultimos valores; o loop principal (task do LVGL) os consome via
// ble_get_usage(). Nunca chame LVGL a partir daqui.
#pragma once

// Inicializa NimBLE, cria o servico/caracteristica e comeca a anunciar.
void ble_begin(void);

// Se chegou uma nova leitura desde a ultima chamada, preenche session/weekly
// (0..100) e retorna true. Senao retorna false.
bool ble_get_usage(int *session, int *weekly);

// true enquanto houver um cliente (o Mac) conectado.
bool ble_is_connected(void);
