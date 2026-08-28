#include "ui.h"
#include <lvgl.h>

// Paleta propria (identidade neutra, sem assets da Anthropic).
#define COL_BG        lv_color_hex(0x0E1116)  // fundo escuro
#define COL_CARD      lv_color_hex(0x1B2430)  // trilho dos arcos
#define COL_SESSION   lv_color_hex(0xF2A33C)  // ambar  — uso da sessao (5h)
#define COL_WEEKLY    lv_color_hex(0x3DB7A8)  // teal   — uso semanal
#define COL_TEXT      lv_color_hex(0xE6EAF0)
#define COL_MUTED     lv_color_hex(0x8A97A8)

static lv_obj_t *arc_session;
static lv_obj_t *arc_weekly;
static lv_obj_t *lbl_session_pct;
static lv_obj_t *lbl_weekly_pct;
static lv_obj_t *lbl_status;
static lv_obj_t *touch_dot;   // feedback visual do toque (confirma calibracao)

// Move um ponto para onde o dedo esta, para conferir a calibracao em uso real.
static void screen_touch_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSING) {
        lv_indev_t *indev = lv_indev_active();
        if (!indev) return;
        lv_point_t p;
        lv_indev_get_point(indev, &p);
        lv_obj_set_pos(touch_dot, p.x - 6, p.y - 6);
        lv_obj_clear_flag(touch_dot, LV_OBJ_FLAG_HIDDEN);
    } else if (code == LV_EVENT_RELEASED) {
        lv_obj_add_flag(touch_dot, LV_OBJ_FLAG_HIDDEN);
    }
}

// Cria um arco de progresso com rotulo de % no centro e uma legenda embaixo.
static void make_gauge(lv_obj_t *parent, int x_offset, lv_color_t color,
                       const char *caption, lv_obj_t **out_arc,
                       lv_obj_t **out_lbl) {
    lv_obj_t *arc = lv_arc_create(parent);
    lv_obj_set_size(arc, 130, 130);
    lv_obj_align(arc, LV_ALIGN_CENTER, x_offset, -10);
    lv_arc_set_range(arc, 0, 100);
    lv_arc_set_bg_angles(arc, 135, 45);   // arco em "U" aberto embaixo
    lv_arc_set_value(arc, 0);
    lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);  // sem knob arrastavel
    // Cores: trilho de fundo e indicador.
    lv_obj_set_style_arc_color(arc, COL_CARD, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, color, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc, 12, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 12, LV_PART_INDICATOR);

    // Rotulo de % no centro do arco.
    lv_obj_t *lbl = lv_label_create(arc);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(lbl, COL_TEXT, 0);
    lv_label_set_text(lbl, "0%");
    lv_obj_center(lbl);

    // Legenda abaixo do arco.
    lv_obj_t *cap = lv_label_create(parent);
    lv_obj_set_style_text_font(cap, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(cap, COL_MUTED, 0);
    lv_label_set_text(cap, caption);
    lv_obj_align_to(cap, arc, LV_ALIGN_OUT_BOTTOM_MID, 0, 4);

    *out_arc = arc;
    *out_lbl = lbl;
}

void ui_build(void) {
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, COL_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_add_flag(scr, LV_OBJ_FLAG_CLICKABLE);  // recebe eventos de toque

    // Titulo.
    lv_obj_t *title = lv_label_create(scr);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, COL_TEXT, 0);
    lv_label_set_text(title, "Claude Token Meter");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 6);

    // Dois arcos: sessao (esquerda) e semana (direita).
    make_gauge(scr, -80, COL_SESSION, "Sessao (5h)", &arc_session,
               &lbl_session_pct);
    make_gauge(scr, 80, COL_WEEKLY, "Semana", &arc_weekly, &lbl_weekly_pct);

    // Status de conexao no rodape.
    lbl_status = lv_label_create(scr);
    lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_status, COL_MUTED, 0);
    lv_label_set_text(lbl_status, "BLE: aguardando...");
    lv_obj_align(lbl_status, LV_ALIGN_BOTTOM_MID, 0, -4);

    // Ponto de feedback do toque (confirma calibracao). Escondido por padrao.
    touch_dot = lv_obj_create(scr);
    lv_obj_set_size(touch_dot, 12, 12);
    lv_obj_set_style_radius(touch_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(touch_dot, COL_TEXT, 0);
    lv_obj_set_style_border_width(touch_dot, 0, 0);
    lv_obj_add_flag(touch_dot, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(scr, screen_touch_cb, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(scr, screen_touch_cb, LV_EVENT_RELEASED, NULL);
}

void ui_set_usage(int session_pct, int weekly_pct) {
    if (session_pct < 0) session_pct = 0;
    if (session_pct > 100) session_pct = 100;
    if (weekly_pct < 0) weekly_pct = 0;
    if (weekly_pct > 100) weekly_pct = 100;

    lv_arc_set_value(arc_session, session_pct);
    lv_arc_set_value(arc_weekly, weekly_pct);

    lv_label_set_text_fmt(lbl_session_pct, "%d%%", session_pct);
    lv_label_set_text_fmt(lbl_weekly_pct, "%d%%", weekly_pct);
}

void ui_set_status(const char *text) {
    if (lbl_status) lv_label_set_text(lbl_status, text);
}
