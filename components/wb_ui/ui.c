#include "ui.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "advice.h"
#include "assets/assets.h"
#include "lvgl.h"

#define DEG "\xC2\xB0"

/* ------------------------------------------------------------------ palette */

typedef struct {
    uint32_t top, bottom;   /* sky gradient */
    uint32_t text, text2;   /* main / secondary text */
    uint32_t card;          /* card fill */
    lv_opa_t card_opa;
    uint32_t bubble, bubble_text, accent;
    uint32_t dot_on, dot_off;
} theme_t;

static const theme_t TH_CLEAR = {0x63C1FF, 0xDDF3FF, 0x1B2542, 0x3B4868, 0xFFFFFF, 165, 0xFFFFFF, 0x1B2542, 0xE0457B, 0x1B2542, 0x8FB4D6};
static const theme_t TH_PARTLY = {0x7FBDEB, 0xE3F0FA, 0x1B2542, 0x3B4868, 0xFFFFFF, 165, 0xFFFFFF, 0x1B2542, 0xE0457B, 0x1B2542, 0x97B3CF};
static const theme_t TH_GREY = {0x9FB1C6, 0xE5EAF0, 0x1B2542, 0x3B4868, 0xFFFFFF, 170, 0xFFFFFF, 0x1B2542, 0xE0457B, 0x1B2542, 0x9FAEC0};
static const theme_t TH_RAIN = {0x7489A6, 0xC7D2E0, 0x1B2542, 0x2E3A57, 0xFFFFFF, 175, 0xFFFFFF, 0x1B2542, 0xE0457B, 0x1B2542, 0x8C9DB4};
static const theme_t TH_SNOW = {0xA3C7EA, 0xF2F8FD, 0x1B2542, 0x3B4868, 0xFFFFFF, 175, 0xFFFFFF, 0x1B2542, 0xE0457B, 0x1B2542, 0xA6BFD8};
static const theme_t TH_NIGHT = {0x101833, 0x2A3B6E, 0xF4F6FF, 0xB9C4E4, 0xFFFFFF, 26, 0x34467E, 0xF4F6FF, 0xFF9EC4, 0xF4F6FF, 0x55669A};
static const theme_t TH_SETUP = {0xFFD9EA, 0xFFF6FA, 0x1B2542, 0x3B4868, 0xFFFFFF, 255, 0xFFFFFF, 0x1B2542, 0xE0457B, 0x1B2542, 0xC9A8B8};

#define C(hex) lv_color_hex(hex)

static const theme_t *theme_for(const wx_t *wx)
{
    if (!wx->is_day) {
        return &TH_NIGHT;
    }
    switch (wx_kind(wx->code)) {
    case WXK_CLEAR: return &TH_CLEAR;
    case WXK_CLOUDS: return wx->code == 2 ? &TH_PARTLY : &TH_GREY;
    case WXK_FOG: return &TH_GREY;
    case WXK_DRIZZLE: case WXK_RAIN: case WXK_THUNDER: return &TH_RAIN;
    case WXK_SNOW: case WXK_ICE: return &TH_SNOW;
    }
    return &TH_PARTLY;
}

/* ------------------------------------------------------------------ state */

static const lv_image_dsc_t *ICON_L[] = WB_WX_ICONS_L;
static const lv_image_dsc_t *ICON_M[] = WB_WX_ICONS_M;
static const lv_image_dsc_t *ICON_S[] = WB_WX_ICONS_S;
static const lv_image_dsc_t *WEAR_ICONS[] = WB_WEAR_ICONS;

static char s_names[48] = "Cleo & Lila";
static ui_action_cb_t s_act_cb;
static void *s_act_user;
static ui_time_cb_t s_time_cb;
static const theme_t *s_theme = &TH_PARTLY;
static int s_last_min = -1;
static bool s_have_wx;
static advice_opts_t s_opts = {.bike_bus_wday = 5}; /* Friday */

static lv_obj_t *scr_boot, *scr_setup, *scr_main;

/* boot / connecting / error */
static lv_obj_t *boot_title, *boot_msg, *boot_detail, *boot_spinner, *boot_btn;

/* setup */
static lv_obj_t *setup_qr, *setup_steps;

/* main: top bar */
static lv_obj_t *lbl_greet, *lbl_time, *lbl_date;
/* main: pages */
static lv_obj_t *tv, *tile0, *tile1;
static lv_obj_t *img_hero, *lbl_temp, *lbl_cond, *lbl_hilo, *lbl_feels;
static lv_obj_t *bubble, *bubble_tail, *lbl_head, *lbl_cmp, *lbl_tip, *wear_row;
static lv_obj_t *wear_chip[ADVICE_MAX_WEAR], *wear_img[ADVICE_MAX_WEAR], *wear_lbl[ADVICE_MAX_WEAR];
static lv_obj_t *pcard[WX_MAX_PERIODS], *p_lbl[WX_MAX_PERIODS], *p_img[WX_MAX_PERIODS], *p_temp[WX_MAX_PERIODS],
                *p_pop[WX_MAX_PERIODS], *p_drop[WX_MAX_PERIODS];
static lv_obj_t *lbl_days_title;
static lv_obj_t *dcard[3], *d_name[3], *d_img[3], *d_temp[3], *d_pop[3], *d_drop[3], *d_line[3], *d_trend[3];
/* main: footer */
static lv_obj_t *dot[2], *lbl_place, *lbl_status;

/* settings overlay */
static lv_obj_t *settings, *set_info;

/* ------------------------------------------------------------------ helpers */

static lv_obj_t *box(lv_obj_t *parent)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    return o;
}

static lv_obj_t *label(lv_obj_t *parent, const lv_font_t *f, const char *txt)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, f, 0);
    lv_label_set_text(l, txt ? txt : "");
    return l;
}

static void set_grad(lv_obj_t *o, uint32_t top, uint32_t bottom)
{
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(o, C(top), 0);
    lv_obj_set_style_bg_grad_color(o, C(bottom), 0);
    lv_obj_set_style_bg_grad_dir(o, LV_GRAD_DIR_VER, 0);
}

static lv_obj_t *card(lv_obj_t *parent, int x, int y, int w, int h)
{
    lv_obj_t *o = box(parent);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_radius(o, 24, 0);
    lv_obj_set_style_bg_opa(o, 165, 0);
    lv_obj_set_style_bg_color(o, lv_color_white(), 0);
    return o;
}

static lv_obj_t *pill_button(lv_obj_t *parent, const char *txt, bool primary, lv_event_cb_t cb, void *ud)
{
    lv_obj_t *b = lv_button_create(parent);
    lv_obj_remove_style_all(b);
    lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(b, primary ? C(0xE0457B) : C(0xEEF1F7), 0);
    lv_obj_set_style_bg_color(b, primary ? C(0xB8325F) : C(0xD5DBE6), LV_STATE_PRESSED);
    lv_obj_set_style_pad_hor(b, 26, 0);
    lv_obj_set_style_pad_ver(b, 14, 0);
    lv_obj_t *l = label(b, &font_body, txt);
    lv_obj_set_style_text_color(l, primary ? lv_color_white() : C(0x1B2542), 0);
    lv_obj_center(l);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, ud);
    return b;
}

static int iround(float v)
{
    return (int)lroundf(v);
}

static void fire(ui_action_t a)
{
    if (s_act_cb) {
        s_act_cb(a, s_act_user);
    }
}

/* ------------------------------------------------------------------ boot / status screen */

static void on_setup_btn(lv_event_t *e)
{
    (void)e;
    fire(UI_ACT_SETUP);
}

static void cats_pair(lv_obj_t *parent, int y, bool center)
{
    lv_obj_t *row = box(parent);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row, 18, 0);
    lv_obj_t *a = lv_image_create(row);
    lv_image_set_src(a, &img_cat_black);
    lv_obj_t *b = lv_image_create(row);
    lv_image_set_src(b, &img_cat_pink);
    if (center) {
        lv_obj_align(row, LV_ALIGN_TOP_MID, 0, y);
    } else {
        lv_obj_set_pos(row, 0, y);
    }
}

static void build_boot(void)
{
    scr_boot = lv_obj_create(NULL);
    lv_obj_remove_flag(scr_boot, LV_OBJ_FLAG_SCROLLABLE);
    set_grad(scr_boot, TH_PARTLY.top, TH_PARTLY.bottom);

    cats_pair(scr_boot, 78, true);

    boot_title = label(scr_boot, &font_title, "Weather Besties");
    lv_obj_set_style_text_color(boot_title, C(0x1B2542), 0);
    lv_obj_align(boot_title, LV_ALIGN_TOP_MID, 0, 214);

    boot_msg = label(scr_boot, &font_body, "");
    lv_obj_set_style_text_color(boot_msg, C(0x1B2542), 0);
    lv_obj_set_style_text_align(boot_msg, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(boot_msg, 700);
    lv_obj_align(boot_msg, LV_ALIGN_TOP_MID, 0, 266);

    boot_detail = label(scr_boot, &font_small, "");
    lv_obj_set_style_text_color(boot_detail, C(0x3B4868), 0);
    lv_obj_set_style_text_align(boot_detail, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(boot_detail, 700);
    lv_obj_align(boot_detail, LV_ALIGN_TOP_MID, 0, 306);

    boot_spinner = lv_spinner_create(scr_boot);
    lv_obj_set_size(boot_spinner, 48, 48);
    lv_spinner_set_anim_params(boot_spinner, 1100, 70);
    lv_obj_set_style_arc_width(boot_spinner, 7, LV_PART_MAIN);
    lv_obj_set_style_arc_width(boot_spinner, 7, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(boot_spinner, C(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_arc_color(boot_spinner, C(0xE0457B), LV_PART_INDICATOR);
    lv_obj_align(boot_spinner, LV_ALIGN_TOP_MID, 0, 356);

    boot_btn = pill_button(scr_boot, LV_SYMBOL_WIFI "  Set up WiFi", true, on_setup_btn, NULL);
    lv_obj_align(boot_btn, LV_ALIGN_TOP_MID, 0, 360);
    lv_obj_add_flag(boot_btn, LV_OBJ_FLAG_HIDDEN);
}

static void boot_mode(const char *msg, const char *detail, bool spinner, bool button)
{
    lv_label_set_text(boot_msg, msg ? msg : "");
    lv_label_set_text(boot_detail, detail ? detail : "");
    if (spinner) {
        lv_obj_remove_flag(boot_spinner, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(boot_spinner, LV_OBJ_FLAG_HIDDEN);
    }
    if (button) {
        lv_obj_remove_flag(boot_btn, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(boot_btn, LV_OBJ_FLAG_HIDDEN);
    }
    if (lv_screen_active() != scr_boot) {
        lv_screen_load(scr_boot);
    }
}

void ui_show_boot(const char *msg)
{
    boot_mode(msg ? msg : "Waking up the cats...", "", true, false);
}

void ui_show_connecting(const char *ssid)
{
    char m[96];
    snprintf(m, sizeof(m), "Connecting to %s...", ssid && ssid[0] ? ssid : "WiFi");
    boot_mode(m, "", true, false);
}

void ui_show_wifi_error(const char *ssid, const char *detail)
{
    char m[96];
    snprintf(m, sizeof(m), "Can't reach WiFi \"%s\"", ssid && ssid[0] ? ssid : "?");
    boot_mode(m, detail ? detail : "Trying again in a minute. Wrong password? Tap below.", false, true);
}

/* ------------------------------------------------------------------ setup screen */

static void build_setup(void)
{
    scr_setup = lv_obj_create(NULL);
    lv_obj_remove_flag(scr_setup, LV_OBJ_FLAG_SCROLLABLE);
    set_grad(scr_setup, TH_SETUP.top, TH_SETUP.bottom);

    lv_obj_t *qcard = card(scr_setup, 40, 64, 300, 352);
    lv_obj_set_style_bg_opa(qcard, LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_width(qcard, 30, 0);
    lv_obj_set_style_shadow_opa(qcard, 40, 0);
    lv_obj_set_style_shadow_color(qcard, C(0xB0607F), 0);
    lv_obj_set_style_shadow_offset_y(qcard, 8, 0);

    setup_qr = lv_qrcode_create(qcard);
    lv_qrcode_set_size(setup_qr, 248);
    lv_qrcode_set_dark_color(setup_qr, C(0x1B2542));
    lv_qrcode_set_light_color(setup_qr, lv_color_white());
    lv_obj_align(setup_qr, LV_ALIGN_TOP_MID, 0, 26);

    lv_obj_t *cap = label(qcard, &font_small, "Scan with your phone camera");
    lv_obj_set_style_text_color(cap, C(0x3B4868), 0);
    lv_obj_align(cap, LV_ALIGN_BOTTOM_MID, 0, -22);

    lv_obj_t *col = box(scr_setup);
    lv_obj_set_pos(col, 380, 34);
    lv_obj_set_size(col, 390, 420);

    cats_pair(col, 0, false);

    lv_obj_t *t = label(col, &font_title, "Let's get online!");
    lv_obj_set_style_text_color(t, C(0x1B2542), 0);
    lv_obj_set_pos(t, 0, 128);

    setup_steps = label(col, &font_body, "");
    lv_obj_set_style_text_color(setup_steps, C(0x1B2542), 0);
    lv_obj_set_style_text_line_space(setup_steps, 6, 0);
    lv_obj_set_width(setup_steps, 390);
    lv_label_set_long_mode(setup_steps, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_pos(setup_steps, 0, 178);
}

void ui_show_setup(const char *ap_name, const char *url)
{
    char qr[96];
    snprintf(qr, sizeof(qr), "WIFI:T:nopass;S:%s;;", ap_name);
    lv_qrcode_update(setup_qr, qr, strlen(qr));

    char steps[320];
    snprintf(steps, sizeof(steps),
             "1. Join the WiFi called\n    %s\n"
             "2. A setup page pops up.\n    (No page? Open %s)\n"
             "3. Pick your home WiFi and town. Done!",
             ap_name, url);
    lv_label_set_text(setup_steps, steps);
    if (lv_screen_active() != scr_setup) {
        lv_screen_load(scr_setup);
    }
}

/* ------------------------------------------------------------------ settings overlay */

static void on_settings_btn(lv_event_t *e)
{
    intptr_t which = (intptr_t)lv_event_get_user_data(e);
    ui_close_settings();
    if (which == 1) {
        fire(UI_ACT_SETUP);
    } else if (which == 2) {
        fire(UI_ACT_REFRESH);
    }
}

static void on_settings_bg(lv_event_t *e)
{
    if (lv_event_get_target_obj(e) == settings) {
        ui_close_settings();
    }
}

static void build_settings(void)
{
    settings = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(settings);
    lv_obj_set_size(settings, UI_W, UI_H);
    lv_obj_set_style_bg_color(settings, C(0x0B1022), 0);
    lv_obj_set_style_bg_opa(settings, 150, 0);
    lv_obj_add_flag(settings, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(settings, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(settings, on_settings_bg, LV_EVENT_CLICKED, NULL);

    lv_obj_t *p = box(settings);
    lv_obj_add_flag(p, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(p, 600, 392);
    lv_obj_center(p);
    lv_obj_set_style_radius(p, 28, 0);
    lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(p, lv_color_white(), 0);
    lv_obj_set_style_pad_all(p, 28, 0);

    lv_obj_t *t = label(p, &font_title, LV_SYMBOL_SETTINGS "  Settings");
    lv_obj_set_style_text_color(t, C(0x1B2542), 0);
    lv_obj_set_pos(t, 0, 0);

    set_info = label(p, &font_body, "");
    lv_obj_set_style_text_color(set_info, C(0x3B4868), 0);
    lv_obj_set_style_text_line_space(set_info, 8, 0);
    lv_obj_set_width(set_info, 544);
    lv_label_set_long_mode(set_info, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_pos(set_info, 0, 54);

    lv_obj_t *row = box(p);
    lv_obj_set_size(row, 544, LV_SIZE_CONTENT);
    lv_obj_align(row, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    pill_button(row, "Change WiFi or town", true, on_settings_btn, (void *)1);
    pill_button(row, LV_SYMBOL_REFRESH, false, on_settings_btn, (void *)2);
    pill_button(row, "Close", false, on_settings_btn, (void *)0);

    lv_obj_add_flag(settings, LV_OBJ_FLAG_HIDDEN);
}

void ui_set_info(const ui_info_t *i)
{
    char buf[400];
    snprintf(buf, sizeof(buf),
             LV_SYMBOL_WIFI "  WiFi: %s\n"
             LV_SYMBOL_GPS "  Town: %s\n"
             LV_SYMBOL_REFRESH "  %s\n"
             "Address: %s     Version %s\n"
             "Weather data by Open-Meteo.com",
             i->wifi[0] ? i->wifi : "not set", i->place[0] ? i->place : "not set",
             i->updated[0] ? i->updated : "No weather yet", i->ip[0] ? i->ip : "-", i->version);
    lv_label_set_text(set_info, buf);
}

void ui_open_settings(void)
{
    lv_obj_remove_flag(settings, LV_OBJ_FLAG_HIDDEN);
}

void ui_close_settings(void)
{
    lv_obj_add_flag(settings, LV_OBJ_FLAG_HIDDEN);
}

static void on_long_press(lv_event_t *e)
{
    (void)e;
    ui_open_settings();
}

/* ------------------------------------------------------------------ main screen */

static void update_dots(void)
{
    lv_obj_t *act = lv_tileview_get_tile_active(tv);
    for (int i = 0; i < 2; i++) {
        bool on = (i == 0) ? (act == tile0) : (act == tile1);
        lv_obj_set_style_bg_color(dot[i], C(on ? s_theme->dot_on : s_theme->dot_off), 0);
        lv_obj_set_width(dot[i], on ? 26 : 10);
    }
}

static void on_tile_changed(lv_event_t *e)
{
    (void)e;
    update_dots();
}

static void build_today(lv_obj_t *t)
{
    img_hero = lv_image_create(t);
    lv_obj_set_pos(img_hero, 14, 12);

    lbl_temp = label(t, &font_temp_xl, "--" DEG);
    lv_obj_set_pos(lbl_temp, 196, 6);

    lbl_cond = label(t, &font_title, "");
    lv_obj_set_pos(lbl_cond, 204, 140);

    lbl_hilo = label(t, &font_body, "");
    lv_obj_set_pos(lbl_hilo, 204, 180);

    lbl_feels = label(t, &font_small, "");
    lv_obj_set_pos(lbl_feels, 204, 214);

    /* speech bubble: tail first so the bubble covers its lower half */
    bubble_tail = box(t);
    lv_obj_set_size(bubble_tail, 30, 30);
    lv_obj_set_pos(bubble_tail, 676, 6);
    lv_obj_set_style_bg_opa(bubble_tail, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(bubble_tail, 4, 0);
    lv_obj_set_style_transform_pivot_x(bubble_tail, 15, 0);
    lv_obj_set_style_transform_pivot_y(bubble_tail, 15, 0);
    lv_obj_set_style_transform_rotation(bubble_tail, 450, 0);

    bubble = box(t);
    lv_obj_set_pos(bubble, 444, 16);
    lv_obj_set_size(bubble, 338, 238);
    lv_obj_set_style_radius(bubble, 28, 0);
    lv_obj_set_style_bg_opa(bubble, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(bubble, 18, 0);
    lv_obj_set_style_pad_row(bubble, 4, 0);
    lv_obj_set_style_shadow_width(bubble, 28, 0);
    lv_obj_set_style_shadow_opa(bubble, 45, 0);
    lv_obj_set_style_shadow_offset_y(bubble, 8, 0);
    lv_obj_set_style_shadow_color(bubble, C(0x1B2542), 0);
    lv_obj_set_flex_flow(bubble, LV_FLEX_FLOW_COLUMN);

    lbl_head = label(bubble, &font_title, "");
    lv_obj_set_width(lbl_head, lv_pct(100));
    lv_label_set_long_mode(lbl_head, LV_LABEL_LONG_MODE_WRAP);

    lbl_cmp = label(bubble, &font_small, "");
    lv_obj_set_width(lbl_cmp, lv_pct(100));
    lv_label_set_long_mode(lbl_cmp, LV_LABEL_LONG_MODE_WRAP);

    lbl_tip = label(bubble, &font_body, "");
    lv_obj_set_width(lbl_tip, lv_pct(100));
    lv_label_set_long_mode(lbl_tip, LV_LABEL_LONG_MODE_WRAP);

    lv_obj_t *spacer = box(bubble);
    lv_obj_set_width(spacer, 1);
    lv_obj_set_flex_grow(spacer, 1);

    wear_row = box(bubble);
    lv_obj_set_size(wear_row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(wear_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(wear_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_column(wear_row, 10, 0);
    for (int i = 0; i < ADVICE_MAX_WEAR; i++) {
        wear_chip[i] = box(wear_row);
        lv_obj_set_size(wear_chip[i], 92, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(wear_chip[i], LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(wear_chip[i], LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        wear_img[i] = lv_image_create(wear_chip[i]);
        wear_lbl[i] = label(wear_chip[i], &font_tiny, "");
    }

    /* period cards */
    for (int i = 0; i < WX_MAX_PERIODS; i++) {
        lv_obj_t *c = card(t, 20 + i * 193, 262, 181, 134);
        pcard[i] = c;
        p_lbl[i] = label(c, &font_small, "");
        lv_obj_align(p_lbl[i], LV_ALIGN_TOP_MID, 0, 8);
        p_img[i] = lv_image_create(c);
        lv_obj_set_pos(p_img[i], 8, 40);
        p_temp[i] = label(c, &font_temp_l, "");
        lv_obj_set_pos(p_temp[i], 90, 40);
        p_drop[i] = lv_image_create(c);
        lv_image_set_src(p_drop[i], &img_drop);
        lv_obj_set_pos(p_drop[i], 92, 94);
        p_pop[i] = label(c, &font_small, "");
        lv_obj_set_pos(p_pop[i], 118, 92);
    }
}

static void build_days(lv_obj_t *t)
{
    lbl_days_title = label(t, &font_title, "Next few days");
    lv_obj_set_pos(lbl_days_title, 24, 2);
    for (int i = 0; i < 3; i++) {
        lv_obj_t *c = card(t, 20 + i * 260, 50, 240, 346);
        dcard[i] = c;
        d_name[i] = label(c, &font_title, "");
        lv_obj_align(d_name[i], LV_ALIGN_TOP_MID, 0, 14);
        d_img[i] = lv_image_create(c);
        lv_obj_align(d_img[i], LV_ALIGN_TOP_MID, 0, 58);
        d_temp[i] = label(c, &font_temp_l, "");
        lv_obj_align(d_temp[i], LV_ALIGN_TOP_MID, 0, 176);

        lv_obj_t *row = box(c);
        lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(row, 6, 0);
        lv_obj_align(row, LV_ALIGN_TOP_MID, 0, 228);
        d_drop[i] = lv_image_create(row);
        lv_image_set_src(d_drop[i], &img_drop);
        d_pop[i] = label(row, &font_small, "");

        d_line[i] = label(c, &font_small, "");
        lv_obj_set_width(d_line[i], 210);
        lv_label_set_long_mode(d_line[i], LV_LABEL_LONG_MODE_WRAP);
        lv_obj_set_style_text_align(d_line[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(d_line[i], LV_ALIGN_TOP_MID, 0, 296);

        d_trend[i] = label(c, &font_tiny, "");
        lv_obj_align(d_trend[i], LV_ALIGN_TOP_MID, 0, 266);
    }
}

static void build_main(void)
{
    scr_main = lv_obj_create(NULL);
    lv_obj_remove_flag(scr_main, LV_OBJ_FLAG_SCROLLABLE);
    set_grad(scr_main, s_theme->top, s_theme->bottom);

    /* top bar */
    lbl_greet = label(scr_main, &font_title, "");
    lv_obj_set_width(lbl_greet, 470);
    lv_label_set_long_mode(lbl_greet, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_pos(lbl_greet, 24, 14);
    lv_obj_add_flag(lbl_greet, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(lbl_greet, on_long_press, LV_EVENT_LONG_PRESSED, NULL);

    lbl_time = label(scr_main, &font_title, "");
    lv_obj_align(lbl_time, LV_ALIGN_TOP_RIGHT, -24, 14);
    lbl_date = label(scr_main, &font_small, "");
    lv_obj_add_flag(lbl_time, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(lbl_time, on_long_press, LV_EVENT_LONG_PRESSED, NULL);

    /* swipeable pages */
    tv = lv_tileview_create(scr_main);
    lv_obj_set_pos(tv, 0, 58);
    lv_obj_set_size(tv, UI_W, 400);
    lv_obj_set_style_bg_opa(tv, LV_OPA_TRANSP, 0);
    lv_obj_set_scrollbar_mode(tv, LV_SCROLLBAR_MODE_OFF);
    tile0 = lv_tileview_add_tile(tv, 0, 0, LV_DIR_RIGHT);
    tile1 = lv_tileview_add_tile(tv, 1, 0, LV_DIR_LEFT);
    lv_obj_set_style_bg_opa(tile0, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(tile1, LV_OPA_TRANSP, 0);
    lv_obj_add_event_cb(tv, on_tile_changed, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(tile0, on_long_press, LV_EVENT_LONG_PRESSED, NULL);
    lv_obj_add_event_cb(tile1, on_long_press, LV_EVENT_LONG_PRESSED, NULL);
    build_today(tile0);
    build_days(tile1);

    /* footer */
    lbl_place = label(scr_main, &font_tiny, "");
    lv_obj_set_pos(lbl_place, 24, 456);
    lbl_status = label(scr_main, &font_tiny, "");
    lv_obj_align(lbl_status, LV_ALIGN_TOP_RIGHT, -24, 456);

    lv_obj_t *dots = box(scr_main);
    lv_obj_set_size(dots, 80, 10);
    lv_obj_align(dots, LV_ALIGN_TOP_MID, 0, 463);
    lv_obj_set_flex_flow(dots, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(dots, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(dots, 8, 0);
    for (int i = 0; i < 2; i++) {
        dot[i] = box(dots);
        lv_obj_set_size(dot[i], 10, 10);
        lv_obj_set_style_radius(dot[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(dot[i], LV_OPA_COVER, 0);
    }
}

static void apply_theme(const theme_t *th)
{
    s_theme = th;
    set_grad(scr_main, th->top, th->bottom);
    lv_obj_t *txt[] = {lbl_greet, lbl_time, lbl_temp, lbl_cond, lbl_days_title, lbl_place, lbl_status};
    for (size_t i = 0; i < sizeof(txt) / sizeof(txt[0]); i++) {
        lv_obj_set_style_text_color(txt[i], C(th->text), 0);
    }
    lv_obj_t *txt2[] = {lbl_date, lbl_hilo, lbl_feels};
    for (size_t i = 0; i < sizeof(txt2) / sizeof(txt2[0]); i++) {
        lv_obj_set_style_text_color(txt2[i], C(th->text2), 0);
    }
    lv_obj_set_style_bg_color(bubble, C(th->bubble), 0);
    lv_obj_set_style_bg_color(bubble_tail, C(th->bubble), 0);
    lv_obj_set_style_text_color(lbl_head, C(th->accent), 0);
    lv_obj_set_style_text_color(lbl_tip, C(th->bubble_text), 0);
    lv_obj_set_style_text_color(lbl_cmp, C(th->bubble_text), 0);
    for (int i = 0; i < ADVICE_MAX_WEAR; i++) {
        lv_obj_set_style_text_color(wear_lbl[i], C(th->bubble_text), 0);
    }
    for (int i = 0; i < WX_MAX_PERIODS; i++) {
        lv_obj_set_style_bg_color(pcard[i], C(th->card), 0);
        lv_obj_set_style_bg_opa(pcard[i], th->card_opa, 0);
        lv_obj_set_style_text_color(p_lbl[i], C(th->text2), 0);
        lv_obj_set_style_text_color(p_temp[i], C(th->text), 0);
        lv_obj_set_style_text_color(p_pop[i], C(th->text2), 0);
    }
    for (int i = 0; i < 3; i++) {
        lv_obj_set_style_bg_color(dcard[i], C(th->card), 0);
        lv_obj_set_style_bg_opa(dcard[i], th->card_opa, 0);
        lv_obj_set_style_text_color(d_name[i], C(th->text), 0);
        lv_obj_set_style_text_color(d_temp[i], C(th->text), 0);
        lv_obj_set_style_text_color(d_pop[i], C(th->text2), 0);
        lv_obj_set_style_text_color(d_line[i], C(th->text2), 0);
        lv_obj_set_style_text_color(d_trend[i], C(th->text), 0);
    }
    update_dots();
}

/* ------------------------------------------------------------------ clock */

static void greet_text(int hour, char *buf, size_t n)
{
    const char *g = hour >= 5 && hour < 12 ? "Good morning" : hour >= 12 && hour < 17 ? "Good afternoon"
                    : hour >= 17 && hour < 21 ? "Good evening" : "Good night";
    if (s_names[0]) {
        snprintf(buf, n, "%s, %s!", g, s_names);
    } else {
        snprintf(buf, n, "%s!", g);
    }
}

static void set_clock(const struct tm *t)
{
    char buf[96];
    greet_text(t->tm_hour, buf, sizeof(buf));
    lv_label_set_text(lbl_greet, buf);
    int h12 = t->tm_hour % 12;
    if (h12 == 0) {
        h12 = 12;
    }
    lv_label_set_text_fmt(lbl_time, "%d:%02d %s", h12, t->tm_min, t->tm_hour < 12 ? "AM" : "PM");
    lv_label_set_text_fmt(lbl_date, "%s, %s %d", wx_wday_name(t->tm_wday, true), wx_month_name(t->tm_mon + 1, true),
                          t->tm_mday);
    lv_obj_align(lbl_time, LV_ALIGN_TOP_RIGHT, -24, 14);
    lv_obj_update_layout(lbl_time);
    lv_obj_align_to(lbl_date, lbl_time, LV_ALIGN_OUT_LEFT_BOTTOM, -14, -4);
}

void ui_refresh_clock(void)
{
    struct tm t;
    if (s_time_cb && s_time_cb(&t)) {
        s_last_min = t.tm_hour * 60 + t.tm_min;
        set_clock(&t);
    }
}

static void clock_timer(lv_timer_t *tm)
{
    (void)tm;
    struct tm t;
    if (s_time_cb && s_time_cb(&t)) {
        int m = t.tm_hour * 60 + t.tm_min;
        if (m != s_last_min) {
            s_last_min = m;
            set_clock(&t);
        }
    }
}

/* ------------------------------------------------------------------ weather */

/* Long advice: first drop the little captions under the clothes, then shrink the headline. */
static int bubble_need(void)
{
    lv_obj_update_layout(bubble);
    int gaps = lv_obj_has_flag(lbl_cmp, LV_OBJ_FLAG_HIDDEN) ? 3 : 4;
    int h = lv_obj_get_height(lbl_head) + lv_obj_get_height(lbl_tip) + lv_obj_get_height(wear_row) + gaps * 4;
    if (!lv_obj_has_flag(lbl_cmp, LV_OBJ_FLAG_HIDDEN)) {
        h += lv_obj_get_height(lbl_cmp);
    }
    return h;
}

/* Long advice: drop the captions under the clothes, then step the fonts down one at a time. */
static void fit_bubble(void)
{
    lv_obj_set_style_text_font(lbl_head, &font_title, 0);
    lv_obj_set_style_text_font(lbl_cmp, &font_small, 0);
    lv_obj_set_style_text_font(lbl_tip, &font_body, 0);
    for (int i = 0; i < ADVICE_MAX_WEAR; i++) {
        lv_obj_remove_flag(wear_lbl[i], LV_OBJ_FLAG_HIDDEN);
    }
    int avail = lv_obj_get_content_height(bubble);
    if (bubble_need() <= avail) {
        return;
    }
    for (int i = 0; i < ADVICE_MAX_WEAR; i++) {
        lv_obj_add_flag(wear_lbl[i], LV_OBJ_FLAG_HIDDEN);
    }
    if (bubble_need() <= avail) {
        return;
    }
    lv_obj_set_style_text_font(lbl_tip, &font_small, 0);
    if (bubble_need() <= avail) {
        return;
    }
    lv_obj_set_style_text_font(lbl_head, &font_body, 0);
    if (bubble_need() <= avail) {
        return;
    }
    lv_obj_set_style_text_font(lbl_cmp, &font_tiny, 0);
}

static void set_pop(lv_obj_t *drop, lv_obj_t *lbl, int pop)
{
    if (pop >= 10) {
        lv_obj_remove_flag(drop, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text_fmt(lbl, "%d%%", pop);
    } else {
        lv_obj_add_flag(drop, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(lbl, "Dry");
    }
}

void ui_show_weather(const wx_t *wx)
{
    if (!wx || !wx->valid) {
        return;
    }
    s_have_wx = true;
    apply_theme(theme_for(wx));

    /* hero */
    lv_image_set_src(img_hero, ICON_L[wx_icon_for(wx->code, wx->is_day)]);
    lv_label_set_text_fmt(lbl_temp, "%d" DEG, iround(wx->temp));
    lv_label_set_text(lbl_cond, wx_describe(wx->code, wx->is_day));
    if (wx->ndays > 0) {
        lv_label_set_text_fmt(lbl_hilo, "High %d" DEG "   Low %d" DEG, iround(wx->days[0].tmax),
                              iround(wx->days[0].tmin));
    }
    lv_label_set_text_fmt(lbl_feels, "Feels like %d" DEG, iround(wx->feels));

    /* bubble */
    advice_t a;
    advice_make(wx, &s_opts, &a);
    lv_label_set_text(lbl_head, a.headline);
    lv_label_set_text(lbl_cmp, a.compare);
    if (a.compare[0]) {
        lv_obj_remove_flag(lbl_cmp, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(lbl_cmp, LV_OBJ_FLAG_HIDDEN);
    }
    lv_label_set_text(lbl_tip, a.tip);
    for (int i = 0; i < ADVICE_MAX_WEAR; i++) {
        if (i < a.nwear) {
            lv_image_set_src(wear_img[i], WEAR_ICONS[a.wear[i]]);
            lv_label_set_text(wear_lbl[i], wear_label(a.wear[i]));
            lv_obj_remove_flag(wear_chip[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(wear_chip[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

    fit_bubble();

    /* parts of the day */
    for (int i = 0; i < WX_MAX_PERIODS; i++) {
        if (i < wx->nperiods) {
            const wx_period_t *p = &wx->periods[i];
            lv_obj_remove_flag(pcard[i], LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(p_lbl[i], p->label);
            lv_image_set_src(p_img[i], ICON_S[wx_icon_for(p->code, p->is_day)]);
            lv_label_set_text_fmt(p_temp[i], "%d" DEG, iround(p->temp));
            set_pop(p_drop[i], p_pop[i], p->pop);
        } else {
            lv_obj_add_flag(pcard[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

    /* next days */
    for (int i = 0; i < 3; i++) {
        int di = i + 1;
        if (di < wx->ndays) {
            const wx_day_t *d = &wx->days[di];
            lv_obj_remove_flag(dcard[i], LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(d_name[i], di == 1 ? "Tomorrow" : wx_wday_name(d->wday, false));
            lv_image_set_src(d_img[i], ICON_M[wx_icon_for(d->code, true)]);
            lv_label_set_text_fmt(d_temp[i], "%d" DEG " / %d" DEG, iround(d->tmax), iround(d->tmin));
            set_pop(d_drop[i], d_pop[i], d->pop);
            char line[48];
            advice_day_line(wx, di, line, sizeof(line));
            lv_label_set_text(d_line[i], line);
            advice_day_trend(wx, di, line, sizeof(line));
            lv_label_set_text(d_trend[i], line);
        } else {
            lv_obj_add_flag(dcard[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

    /* Fall back to the API's local time until the real clock is set. */
    struct tm t;
    if (s_time_cb && s_time_cb(&t)) {
        set_clock(&t);
    } else {
        memset(&t, 0, sizeof(t));
        t.tm_hour = wx->hour;
        t.tm_min = wx->minute;
        t.tm_wday = wx->wday;
        t.tm_mon = wx->month - 1;
        t.tm_mday = wx->day;
        set_clock(&t);
    }

    if (lv_screen_active() != scr_main) {
        lv_screen_load(scr_main);
    }
}

void ui_set_footer(const char *place, const char *status, bool warn)
{
    lv_label_set_text(lbl_place, place ? place : "");
    lv_label_set_text(lbl_status, status ? status : "");
    lv_obj_set_style_text_color(lbl_status, warn ? C(0xE0457B) : C(s_theme->text), 0);
    lv_obj_align(lbl_status, LV_ALIGN_TOP_RIGHT, -24, 456);
}

void ui_show_page(int page)
{
    lv_tileview_set_tile_by_index(tv, page ? 1 : 0, 0, LV_ANIM_OFF);
    update_dots();
}

/* ------------------------------------------------------------------ init */

void ui_set_names(const char *names)
{
    snprintf(s_names, sizeof(s_names), "%s", names ? names : "");
    s_last_min = -1;
    ui_refresh_clock();
}

void ui_set_bike_bus_day(int wday)
{
    s_opts.bike_bus_wday = (wday >= 0 && wday <= 6) ? wday : -1;
}

void ui_set_action_cb(ui_action_cb_t cb, void *user)
{
    s_act_cb = cb;
    s_act_user = user;
}

void ui_set_time_cb(ui_time_cb_t cb)
{
    s_time_cb = cb;
}

void ui_init(const char *names)
{
    if (names) {
        snprintf(s_names, sizeof(s_names), "%s", names);
    }
    build_boot();
    build_setup();
    build_main();
    build_settings();
    apply_theme(&TH_PARTLY);
    lv_timer_create(clock_timer, 1000, NULL);
    ui_show_boot(NULL);
}
