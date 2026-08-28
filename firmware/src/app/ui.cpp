#include "ui.h"
#include <Arduino.h>
#include <lvgl.h>

// ---- Paleta (identidade propria, tons quentes) ----------------------------
#define COL_BG      lv_color_hex(0x0D0F0C)  // fundo quase preto
#define COL_CARD    lv_color_hex(0x181C18)  // card
#define COL_TEXT    lv_color_hex(0xF2EFE9)
#define COL_MUTED   lv_color_hex(0x8B857B)
#define COL_PILL    lv_color_hex(0x2A2F2A)
#define COL_TRACK   lv_color_hex(0x2A2F2A)  // trilho da barra
#define COL_ACCENT  lv_color_hex(0xD7663C)  // laranja (rodape/mascote)
// Cores por nivel de uso.
#define COL_OK      lv_color_hex(0x6FB07A)  // verde
#define COL_WARN    lv_color_hex(0xE0A64B)  // ambar
#define COL_DANGER  lv_color_hex(0xD9553F)  // vermelho

static lv_color_t level_color(int pct) {
    if (pct >= 80) return COL_DANGER;
    if (pct >= 50) return COL_WARN;
    return COL_OK;
}

// Um card = big %, pill, barra e linha de reset.
struct Card {
    lv_obj_t *pct;
    lv_obj_t *bar;
    lv_obj_t *reset;
};
static Card card_session;
static Card card_weekly;
static lv_obj_t *conn_dot;
static lv_obj_t *lbl_footer;

// Estado da contagem regressiva (recebido por BLE + instante de recepcao).
static int base_sreset = 0, base_wreset = 0;
static uint32_t recv_ms = 0;
static uint32_t last_tick_ms = 0;

// ---- Mascote pixel-art (desenho original, nao o Clawd) --------------------
// 'o' contorno, 'b' corpo, 'e' olho, 'm' boca, ' ' transparente. 12x10.
static const char *MASCOT[] = {
    "  oooooooo  ",
    " obbbbbbbbo ",
    "obbbbbbbbbbo",
    "obbeebbeebbo",
    "obbeebbeebbo",
    "obbbbbbbbbbo",
    "obbbmmmmbbbo",
    "obbbbbbbbbbo",
    " obbbbbbbbo ",
    "  o o  o o  ",
};
static const int MASCOT_W = 12, MASCOT_H = 10, MASCOT_S = 3;
static uint8_t mascot_buf[12 * 3 * 10 * 3 * 4];  // ARGB8888 36x30

static lv_color_t mascot_color(char c) {
    switch (c) {
        case 'o': return lv_color_hex(0x5A2E17);  // contorno marrom
        case 'b': return COL_ACCENT;               // corpo laranja
        case 'e': return lv_color_hex(0x1A0E07);   // olho escuro
        case 'm': return lv_color_hex(0x3A1D0E);   // boca
        default:  return COL_BG;
    }
}

static void paint_mascot(lv_obj_t *canvas) {
    lv_canvas_fill_bg(canvas, COL_BG, LV_OPA_TRANSP);
    for (int y = 0; y < MASCOT_H; y++) {
        for (int x = 0; x < MASCOT_W; x++) {
            char c = MASCOT[y][x];
            if (c == ' ') continue;
            lv_color_t col = mascot_color(c);
            for (int dy = 0; dy < MASCOT_S; dy++)
                for (int dx = 0; dx < MASCOT_S; dx++)
                    lv_canvas_set_px(canvas, x * MASCOT_S + dx,
                                     y * MASCOT_S + dy, col, LV_OPA_COVER);
        }
    }
}

// ---- Construcao de um card ------------------------------------------------
static void make_card(lv_obj_t *parent, int y, const char *pill_text,
                      Card *out) {
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, 304, 82);
    lv_obj_set_pos(card, 8, y);
    lv_obj_set_style_bg_color(card, COL_CARD, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_radius(card, 14, 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    // % grande.
    out->pct = lv_label_create(card);
    lv_obj_set_style_text_font(out->pct, &lv_font_montserrat_40, 0);
    lv_obj_set_style_text_color(out->pct, COL_TEXT, 0);
    lv_label_set_text(out->pct, "--%");
    lv_obj_align(out->pct, LV_ALIGN_TOP_LEFT, 14, 2);

    // Pill de rotulo.
    lv_obj_t *pill = lv_label_create(card);
    lv_obj_set_style_text_font(pill, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(pill, COL_TEXT, 0);
    lv_obj_set_style_bg_color(pill, COL_PILL, 0);
    lv_obj_set_style_bg_opa(pill, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(pill, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_hor(pill, 12, 0);
    lv_obj_set_style_pad_ver(pill, 4, 0);
    lv_label_set_text(pill, pill_text);
    lv_obj_align(pill, LV_ALIGN_TOP_RIGHT, -14, 12);

    // Barra fina.
    out->bar = lv_bar_create(card);
    lv_obj_set_size(out->bar, 276, 10);
    lv_obj_align(out->bar, LV_ALIGN_TOP_LEFT, 14, 50);
    lv_bar_set_range(out->bar, 0, 100);
    lv_bar_set_value(out->bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(out->bar, COL_TRACK, LV_PART_MAIN);
    lv_obj_set_style_radius(out->bar, 5, LV_PART_MAIN);
    lv_obj_set_style_radius(out->bar, 5, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(out->bar, COL_OK, LV_PART_INDICATOR);

    // Linha "Reseta em ...".
    out->reset = lv_label_create(card);
    lv_obj_set_style_text_font(out->reset, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(out->reset, COL_MUTED, 0);
    lv_label_set_text(out->reset, "Reseta em --");
    lv_obj_align(out->reset, LV_ALIGN_TOP_LEFT, 14, 64);
}

void ui_build(void) {
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, COL_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    // Topo: mascote (esq), titulo, bolinha de conexao (dir).
    lv_obj_t *canvas = lv_canvas_create(scr);
    lv_canvas_set_buffer(canvas, mascot_buf, MASCOT_W * MASCOT_S,
                         MASCOT_H * MASCOT_S, LV_COLOR_FORMAT_ARGB8888);
    lv_obj_align(canvas, LV_ALIGN_TOP_LEFT, 8, 6);
    paint_mascot(canvas);

    lv_obj_t *title = lv_label_create(scr);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, COL_TEXT, 0);
    lv_label_set_text(title, "Token Meter");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 8, 12);

    conn_dot = lv_obj_create(scr);
    lv_obj_set_size(conn_dot, 12, 12);
    lv_obj_set_style_radius(conn_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(conn_dot, 0, 0);
    lv_obj_set_style_bg_color(conn_dot, COL_MUTED, 0);
    lv_obj_align(conn_dot, LV_ALIGN_TOP_RIGHT, -10, 14);

    // Dois cards.
    make_card(scr, 42, "Sessao", &card_session);
    make_card(scr, 128, "Semana", &card_weekly);

    // Rodape de status.
    lbl_footer = lv_label_create(scr);
    lv_obj_set_style_text_font(lbl_footer, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_footer, COL_MUTED, 0);
    lv_label_set_text(lbl_footer, "Aguardando...");
    lv_obj_align(lbl_footer, LV_ALIGN_BOTTOM_MID, 0, -4);
}

// ---- Formatacao da contagem de reset --------------------------------------
static void fmt_reset(char *buf, size_t n, int secs) {
    if (secs <= 0) {
        snprintf(buf, n, "Reseta em breve");
        return;
    }
    int d = secs / 86400;
    int h = (secs % 86400) / 3600;
    int m = (secs % 3600) / 60;
    if (d >= 1)
        snprintf(buf, n, "Reseta em %dd %dh", d, h);
    else if (h >= 1)
        snprintf(buf, n, "Reseta em %dh %dm", h, m);
    else
        snprintf(buf, n, "Reseta em %dm", m);
}

static void apply_card(Card *c, int pct, int reset_secs) {
    lv_color_t col = level_color(pct);
    lv_label_set_text_fmt(c->pct, "%d%%", pct);
    lv_bar_set_value(c->bar, pct, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(c->bar, col, LV_PART_INDICATOR);
    char buf[32];
    fmt_reset(buf, sizeof(buf), reset_secs);
    lv_label_set_text(c->reset, buf);
}

static int cur_session = 0, cur_weekly = 0;

void ui_set_usage(int session, int weekly, int s_reset, int w_reset) {
    session = session < 0 ? 0 : (session > 100 ? 100 : session);
    weekly = weekly < 0 ? 0 : (weekly > 100 ? 100 : weekly);
    cur_session = session;
    cur_weekly = weekly;
    base_sreset = s_reset;
    base_wreset = w_reset;
    recv_ms = millis();
    apply_card(&card_session, session, s_reset);
    apply_card(&card_weekly, weekly, w_reset);
}

void ui_set_connected(bool connected) {
    if (conn_dot)
        lv_obj_set_style_bg_color(conn_dot, connected ? COL_OK : COL_MUTED, 0);
    if (lbl_footer) {
        lv_label_set_text(lbl_footer, connected ? "Conectado" : "Aguardando...");
        lv_obj_set_style_text_color(lbl_footer,
                                    connected ? COL_ACCENT : COL_MUTED, 0);
    }
}

void ui_tick(void) {
    // Atualiza a contagem regressiva ~1x por segundo.
    uint32_t now = millis();
    if (now - last_tick_ms < 1000) return;
    last_tick_ms = now;
    if (recv_ms == 0) return;  // ainda sem dados
    int elapsed = (int)((now - recv_ms) / 1000);
    char buf[32];
    int s = base_sreset - elapsed;
    fmt_reset(buf, sizeof(buf), s);
    lv_label_set_text(card_session.reset, buf);
    int w = base_wreset - elapsed;
    fmt_reset(buf, sizeof(buf), w);
    lv_label_set_text(card_weekly.reset, buf);
}
