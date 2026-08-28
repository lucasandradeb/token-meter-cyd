#include "ui.h"
#include <Arduino.h>
#include <lvgl.h>

// ---- Paleta (identidade propria, tons quentes) ----------------------------
#define COL_BG      lv_color_hex(0x0D0F0C)
#define COL_CARD    lv_color_hex(0x181C18)
#define COL_TEXT    lv_color_hex(0xF2EFE9)
#define COL_MUTED   lv_color_hex(0x8B857B)
#define COL_PILL    lv_color_hex(0x2A2F2A)
#define COL_TRACK   lv_color_hex(0x2A2F2A)
#define COL_ACCENT  lv_color_hex(0xD7663C)
#define COL_OK      lv_color_hex(0x6FB07A)
#define COL_WARN    lv_color_hex(0xE0A64B)
#define COL_DANGER  lv_color_hex(0xD9553F)

static lv_color_t level_color(int pct) {
    if (pct >= 80) return COL_DANGER;
    if (pct >= 50) return COL_WARN;
    return COL_OK;
}

struct Card {
    lv_obj_t *pct;
    lv_obj_t *bar;
    lv_obj_t *reset;
};
static Card card_session;
static Card card_weekly;
static lv_obj_t *conn_dot;
static lv_obj_t *lbl_footer;

static int base_sreset = 0, base_wreset = 0;
static uint32_t recv_ms = 0;
static uint32_t last_tick_ms = 0;
static int cur_session = 0, cur_weekly = 0;

// ---- Silhueta da companhia (imagem ARGB8888, arte propria, uma cor so) ------
// Gerada em scratchpad/gen_party.py e embutida em party_img.c. Vetor
// rasterizado com anti-aliasing = bordas suaves, sem o serrilhado do pixel-art.
#include "party_img.h"

static void make_mascot(lv_obj_t *parent) {
    lv_obj_t *img = lv_image_create(parent);
    lv_image_set_src(img, &party_img_dsc);
    lv_obj_align(img, LV_ALIGN_TOP_MID, 0, 8);
}

// ---- Card (session = grande; weekly = compacto ~50%) ----------------------
static void make_card(lv_obj_t *parent, int y, int h, const char *pill_text,
                      const lv_font_t *pct_font, const lv_font_t *pill_font,
                      int bar_h, Card *out) {
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, 304, h);
    lv_obj_set_pos(card, 8, y);
    lv_obj_set_style_bg_color(card, COL_CARD, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_radius(card, 14, 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    out->pct = lv_label_create(card);
    lv_obj_set_style_text_font(out->pct, pct_font, 0);
    lv_obj_set_style_text_color(out->pct, COL_TEXT, 0);
    lv_label_set_text(out->pct, "--%");
    lv_obj_align(out->pct, LV_ALIGN_TOP_LEFT, 14, 2);

    lv_obj_t *pill = lv_label_create(card);
    lv_obj_set_style_text_font(pill, pill_font, 0);
    lv_obj_set_style_text_color(pill, COL_TEXT, 0);
    lv_obj_set_style_bg_color(pill, COL_PILL, 0);
    lv_obj_set_style_bg_opa(pill, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(pill, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_hor(pill, 10, 0);
    lv_obj_set_style_pad_ver(pill, 3, 0);
    lv_label_set_text(pill, pill_text);
    lv_obj_align(pill, LV_ALIGN_TOP_RIGHT, -12, 10);

    out->bar = lv_bar_create(card);
    lv_obj_set_size(out->bar, 276, bar_h);
    lv_obj_align(out->bar, LV_ALIGN_TOP_LEFT, 14, h - bar_h - 22);
    lv_bar_set_range(out->bar, 0, 100);
    lv_bar_set_value(out->bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(out->bar, COL_TRACK, LV_PART_MAIN);
    lv_obj_set_style_radius(out->bar, bar_h / 2, LV_PART_MAIN);
    lv_obj_set_style_radius(out->bar, bar_h / 2, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(out->bar, COL_OK, LV_PART_INDICATOR);

    out->reset = lv_label_create(card);
    lv_obj_set_style_text_font(out->reset, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(out->reset, COL_MUTED, 0);
    lv_label_set_text(out->reset, "Reseta em --");
    lv_obj_align(out->reset, LV_ALIGN_BOTTOM_LEFT, 14, -4);
}

void ui_build(void) {
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, COL_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    // Topo: mascote/gif (centro) + bolinha de conexao (dir).
    make_mascot(scr);
    conn_dot = lv_obj_create(scr);
    lv_obj_set_size(conn_dot, 12, 12);
    lv_obj_set_style_radius(conn_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(conn_dot, 0, 0);
    lv_obj_set_style_bg_color(conn_dot, COL_MUTED, 0);
    lv_obj_align(conn_dot, LV_ALIGN_TOP_RIGHT, -10, 8);

    // Sessao (grande) e Semana (compacto, ~metade da altura).
    make_card(scr, 74, 84, "Sessao", &lv_font_montserrat_40,
              &lv_font_montserrat_20, 10, &card_session);
    make_card(scr, 162, 52, "Semana", &lv_font_montserrat_20,
              &lv_font_montserrat_14, 6, &card_weekly);

    lbl_footer = lv_label_create(scr);
    lv_obj_set_style_text_font(lbl_footer, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_footer, COL_MUTED, 0);
    lv_label_set_text(lbl_footer, "Aguardando...");
    lv_obj_align(lbl_footer, LV_ALIGN_BOTTOM_MID, 0, -2);
}

static void fmt_reset(char *buf, size_t n, int secs) {
    if (secs <= 0) { snprintf(buf, n, "Reseta em breve"); return; }
    int d = secs / 86400, h = (secs % 86400) / 3600, m = (secs % 3600) / 60;
    if (d >= 1)      snprintf(buf, n, "Reseta em %dd %dh", d, h);
    else if (h >= 1) snprintf(buf, n, "Reseta em %dh %dm", h, m);
    else             snprintf(buf, n, "Reseta em %dm", m);
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
    uint32_t now = millis();
    if (now - last_tick_ms < 1000) return;
    last_tick_ms = now;
    if (recv_ms == 0) return;
    int elapsed = (int)((now - recv_ms) / 1000);
    char buf[32];
    fmt_reset(buf, sizeof(buf), base_sreset - elapsed);
    lv_label_set_text(card_session.reset, buf);
    fmt_reset(buf, sizeof(buf), base_wreset - elapsed);
    lv_label_set_text(card_weekly.reset, buf);
}
