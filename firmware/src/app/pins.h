// Pinagem da placa ESP32-2432S028 ("CYD"). Extraida dos manuais de pinagem.
// Display e touch ficam em barramentos SPI SEPARADOS.
#pragma once

// ---- Display (SPI) ----
#define TFT_SCK   14
#define TFT_MOSI  13
#define TFT_MISO  12
#define TFT_CS    15
#define TFT_DC     2
#define TFT_RST   -1   // RST ligado ao EN da placa
#define TFT_BL    21

// ---- Touch XPT2046 (SPI proprio) ----
#define TP_SCK    25
#define TP_MOSI   32
#define TP_MISO   39
#define TP_CS     33
#define TP_IRQ    36

// ---- Geometria (rotacao 1 = landscape) ----
#define SCREEN_W  320
#define SCREEN_H  240
