#include "ui.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "advice.h"
#include "assets/assets.h"
#include "cat.h"
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

/* Speed test: tap "Updated ..." to cycle off -> new settings -> old settings. */
static ui_speed_cb_t s_speed_cb;
static int s_speed;            /* 0 off, 1 new settings, 2 old (v1.3.0) settings */
static lv_timer_t *s_speed_timer;
static char s_footer[48];
static bool s_footer_warn;
static bool s_past[WX_MOMENTS];
static void on_footer_tap(lv_event_t *e);
static void card_fill(lv_obj_t *o, int screen_y0, int screen_y1, lv_opa_t opa);
static advice_opts_t s_opts = {.bike_bus_wday = 5}; /* Friday */

static lv_obj_t *scr_boot, *scr_setup, *scr_main;

/* boot / connecting / error */
static lv_obj_t *boot_title, *boot_msg, *boot_detail, *boot_spinner, *boot_btn;

/* setup */
static lv_obj_t *setup_qr, *setup_steps;

/* main: top bar */
static lv_obj_t *lbl_greet, *lbl_time;
/* main: pages */
static lv_obj_t *tv, *tile0, *tile1;
static lv_obj_t *img_hero, *lbl_temp, *lbl_cond;
static lv_obj_t *bubble, *bubble_drop, *bubble_tail, *lbl_head, *lbl_tip, *wear_row;
static lv_obj_t *wear_chip[ADVICE_MAX_WEAR], *wear_img[ADVICE_MAX_WEAR];
static lv_obj_t *pcard[WX_MOMENTS], *p_lbl[WX_MOMENTS], *p_img[WX_MOMENTS], *p_temp[WX_MOMENTS],
                *p_pop[WX_MOMENTS], *p_drop[WX_MOMENTS];
static lv_obj_t *lbl_days_title;
static lv_obj_t *dcard[3], *d_name[3], *d_img[3], *d_temp[3], *d_pop[3], *d_drop[3], *d_line[3], *d_trend[3];
/* main: footer */
static lv_obj_t *dot[2], *lbl_status;

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

static void fx_page(int page);

/* Swipes. LVGL's own gesture detection forgets a swipe whenever one touch reading
 * shows no movement, which slow fingers (and the touch chip's own scan rate) hit
 * often. So this just compares where the finger went down with where it came up:
 * 80+ px sideways, mostly horizontal, flips the page. A finger that moved that far
 * also doesn't count as a tap or a long press. */
#define SWIPE_MIN_PX 80
#define TAP_SLOP_PX 30

static lv_point_t s_press_at;
static bool s_swiped;

static int32_t moved_px(lv_indev_t *in, int32_t *dx, int32_t *dy)
{
    lv_point_t p;
    lv_indev_get_point(in, &p);
    *dx = p.x - s_press_at.x;
    *dy = p.y - s_press_at.y;
    return LV_ABS(*dx) > LV_ABS(*dy) ? LV_ABS(*dx) : LV_ABS(*dy);
}

static void on_indev(lv_event_t *e)
{
    lv_indev_t *in = lv_event_get_target(e);
    lv_event_code_t code = lv_event_get_code(e);
    int32_t dx, dy;
    switch (code) {
    case LV_EVENT_PRESSED:
        lv_indev_get_point(in, &s_press_at);
        s_swiped = false;
        break;
    case LV_EVENT_RELEASED:
        moved_px(in, &dx, &dy);
        if (lv_screen_active() == scr_main && lv_obj_has_flag(settings, LV_OBJ_FLAG_HIDDEN) &&
            LV_ABS(dx) >= SWIPE_MIN_PX && LV_ABS(dx) > LV_ABS(dy) * 3 / 2) {
            s_swiped = true;
            ui_show_page(dx < 0 ? 1 : 0);   /* finger moved left = next days */
        }
        break;
    case LV_EVENT_CLICKED:
    case LV_EVENT_SHORT_CLICKED:
    case LV_EVENT_LONG_PRESSED:
        if (s_swiped || moved_px(in, &dx, &dy) > TAP_SLOP_PX) {
            lv_indev_stop_processing(in);   /* a swipe, not a tap or a long press */
        }
        break;
    default:
        break;
    }
}

/* ------------------------------------------------------------------ weather effects */
/* Rain, snow, leaves and stars live in one layer at the very back of the today
 * page, so they pass behind the text, the cat, the bubble and the cards. One timer
 * moves them all 30 times a second: smooth enough to read as falling, and on this
 * chip far cheaper than an LVGL animation per sprite at the 100 Hz refresh rate. */

#define FX_MAX 22
#define FX_TICK_MS 33
/* The open sky: left of the speech bubble (x 412) and above the cards (page y 268).
 * Pieces only fall here; anything behind the bubble or the cards would be hidden
 * but still cost time to redraw. */
#define FX_W 412
#define FX_H 272

typedef struct {
    lv_obj_t *img;
    int32_t x, y;        /* 1/16 pixel */
    int32_t vx, vy;      /* 1/16 pixel per second */
    int16_t x0;          /* sway centre, pixels */
    uint16_t amp;        /* sway amplitude, pixels */
    uint16_t period;     /* sway / twinkle period, ms */
    uint16_t phase;      /* ms offset */
    uint8_t kind;        /* 0 drop, 1 flake, 2 leaf, 3 star */
    uint8_t color, frame;
    uint16_t frame_ms;
} fx_p_t;

static const lv_image_dsc_t *const LEAF_IMG[WB_FX_LEAF_COLORS][WB_FX_LEAF_FRAMES] = WB_FX_LEAVES;
static lv_obj_t *fx_box, *fx_bolt;
static lv_timer_t *fx_timer;
static fx_p_t s_fx[FX_MAX];
static int s_fx_n;
static cat_fx_t s_fx_mode = (cat_fx_t)-1;
static uint32_t s_fx_t;          /* ms since this effect started */
static uint32_t s_bolt_at;       /* next lightning, in s_fx_t ms */

/* Stars sit in the open sky: around the weather picture, left of the cat. */
static const lv_point_t FX_STAR_POS[] = {{352, 18}, {392, 104}, {214, 150}, {150, 170}, {28, 172},
                                         {96, 214}, {176, 220}, {380, 20}};
#define FX_STARS_N ((int)(sizeof(FX_STAR_POS) / sizeof(FX_STAR_POS[0])))

static void fx_spawn(fx_p_t *p, bool first)
{
    int x = (int)lv_rand(0, FX_W - 8);
    int y = first ? (int)lv_rand(0, FX_H) - 20 : -(int)lv_rand(16, 80);
    p->x = x * 16;
    p->x0 = (int16_t)x;
    p->y = y * 16;
    switch (p->kind) {
    case 0:   /* rain: fast, a little wind from the left */
        p->vy = (int32_t)lv_rand(380, 520) * 16;
        p->vx = (int32_t)lv_rand(50, 80) * 16;
        break;
    case 1:   /* snow: slow and swaying */
        p->vy = (int32_t)lv_rand(26, 48) * 16;
        p->vx = 0;
        p->amp = (uint16_t)lv_rand(6, 14);
        p->period = (uint16_t)lv_rand(2400, 4200);
        break;
    case 2:   /* leaves: a bit faster than snow, big sways, tumbling */
        p->vy = (int32_t)lv_rand(34, 62) * 16;
        p->vx = 0;
        p->amp = (uint16_t)lv_rand(26, 46);
        p->period = (uint16_t)lv_rand(3000, 5200);
        p->color = (uint8_t)lv_rand(0, WB_FX_LEAF_COLORS - 1);
        p->frame = (uint8_t)lv_rand(0, WB_FX_LEAF_FRAMES - 1);
        p->frame_ms = (uint16_t)lv_rand(180, 320);
        lv_image_set_src(p->img, LEAF_IMG[p->color][p->frame]);
        break;
    default:
        break;
    }
    p->phase = (uint16_t)lv_rand(0, 6000);
}

static void fx_add(int kind, const lv_image_dsc_t *src)
{
    if (s_fx_n >= FX_MAX) {
        return;
    }
    fx_p_t *p = &s_fx[s_fx_n++];
    p->kind = (uint8_t)kind;
    lv_image_set_src(p->img, src);
    lv_obj_remove_flag(p->img, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_image_opa(p->img, LV_OPA_COVER, 0);
    fx_spawn(p, true);
}

static int32_t fx_sin(uint32_t t_ms, uint16_t period, int32_t amp)
{
    int16_t ang = (int16_t)((t_ms % period) * 360 / period);
    return lv_trigo_sin(ang) * amp / LV_TRIGO_SIN_MAX;
}

static void fx_tick(lv_timer_t *timer)
{
    (void)timer;
    uint32_t dt = FX_TICK_MS;
    s_fx_t += dt;
    for (int i = 0; i < s_fx_n; i++) {
        fx_p_t *p = &s_fx[i];
        uint32_t t = s_fx_t + p->phase;
        if (p->kind == 3) {   /* star: twinkle in place */
            int32_t s = fx_sin(t, p->period, 100);
            lv_obj_set_style_image_opa(p->img, (lv_opa_t)(150 + (s < 0 ? -s : s)), 0);
            continue;
        }
        p->y += p->vy * (int32_t)dt / 1000;
        int px;
        if (p->kind == 0) {
            p->x += p->vx * (int32_t)dt / 1000;
            px = p->x / 16;
            if (px > FX_W) {
                p->x -= (FX_W + 24) * 16;
                px = p->x / 16;
            }
        } else {
            px = p->x0 + fx_sin(t, p->period, p->amp);
        }
        if (p->kind == 2 && (s_fx_t % p->frame_ms) < dt) {   /* tumble */
            p->frame = (uint8_t)((p->frame + 1) % WB_FX_LEAF_FRAMES);
            lv_image_set_src(p->img, LEAF_IMG[p->color][p->frame]);
        }
        if (p->y / 16 > FX_H) {
            fx_spawn(p, false);
            px = p->x / 16;
        }
        lv_obj_set_pos(p->img, px, p->y / 16);
    }

    /* Thunderstorm: a double flicker of lightning every 5-10 seconds. */
    if (s_fx_mode == FX_STORM) {
        uint32_t k = s_fx_t - s_bolt_at;
        if ((int32_t)k >= 0) {
            bool on = k < 90 || (k >= 170 && k < 260);
            if (on == lv_obj_has_flag(fx_bolt, LV_OBJ_FLAG_HIDDEN)) {
                if (on) {
                    lv_obj_remove_flag(fx_bolt, LV_OBJ_FLAG_HIDDEN);
                } else {
                    lv_obj_add_flag(fx_bolt, LV_OBJ_FLAG_HIDDEN);
                }
            }
            if (k >= 260) {
                s_bolt_at = s_fx_t + lv_rand(5000, 10000);
            }
        }
    }
}

static void fx_page(int page);

static void fx_set(cat_fx_t mode)
{
    if (mode == s_fx_mode) {
        return;
    }
    s_fx_mode = mode;
    s_fx_n = 0;
    s_fx_t = 0;
    for (int i = 0; i < FX_MAX; i++) {
        lv_obj_add_flag(s_fx[i].img, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_add_flag(fx_bolt, LV_OBJ_FLAG_HIDDEN);

    int drops = 0, small_drops = 0, flakes = 0, small_flakes = 0, leaves = 0;
    switch (mode) {
    case FX_RAIN: drops = 14; break;
    case FX_HEAVY_RAIN: drops = 20; break;
    case FX_STORM: drops = 18; s_bolt_at = 1500; break;
    case FX_DRIZZLE: small_drops = 12; break;
    case FX_SLEET: drops = 8; flakes = 5; small_flakes = 4; break;
    case FX_SNOW: flakes = 9; small_flakes = 7; break;
    case FX_LEAVES: leaves = 7; break;
    case FX_STARS:
        for (int i = 0; i < FX_STARS_N && s_fx_n < FX_MAX; i++) {
            fx_p_t *p = &s_fx[s_fx_n++];
            p->kind = 3;
            lv_image_set_src(p->img, &img_fx_star);
            lv_obj_set_pos(p->img, FX_STAR_POS[i].x, FX_STAR_POS[i].y);
            p->period = (uint16_t)lv_rand(1600, 3200);
            p->phase = (uint16_t)lv_rand(0, 3000);
            lv_obj_remove_flag(p->img, LV_OBJ_FLAG_HIDDEN);
        }
        break;
    default: break;
    }
    for (int i = 0; i < drops; i++) fx_add(0, &img_fx_drop);
    for (int i = 0; i < small_drops; i++) fx_add(0, &img_fx_drop_s);
    for (int i = 0; i < flakes; i++) fx_add(1, &img_fx_flake);
    for (int i = 0; i < small_flakes; i++) fx_add(1, &img_fx_flake_s);
    for (int i = 0; i < leaves; i++) fx_add(2, LEAF_IMG[0][0]);
    if (mode == FX_DRIZZLE) {   /* drizzle falls slower */
        for (int i = 0; i < s_fx_n; i++) s_fx[i].vy = s_fx[i].vy * 3 / 5;
    }
    fx_tick(NULL);   /* place everything before the first frame */

    if (s_fx_n) {
        lv_obj_remove_flag(fx_box, LV_OBJ_FLAG_HIDDEN);
        fx_page(lv_tileview_get_tile_active(tv) == tile1 ? 1 : 0);
    } else {
        lv_obj_add_flag(fx_box, LV_OBJ_FLAG_HIDDEN);
        lv_timer_pause(fx_timer);
    }
}

/* Only animate while the today page is the one showing. */
static void fx_page(int page)
{
    if (page == 0 && s_fx_n) {
        lv_timer_resume(fx_timer);
    } else {
        lv_timer_pause(fx_timer);
    }
}

static void build_fx(lv_obj_t *t)
{
    fx_box = box(t);   /* first child of the page = drawn behind everything */
    lv_obj_set_size(fx_box, FX_W + 30, FX_H);   /* leaves sway a little past the edge */
    lv_obj_add_flag(fx_box, LV_OBJ_FLAG_HIDDEN);
    for (int i = 0; i < FX_MAX; i++) {
        s_fx[i].img = lv_image_create(fx_box);
        lv_obj_add_flag(s_fx[i].img, LV_OBJ_FLAG_HIDDEN);
    }
    fx_bolt = lv_image_create(fx_box);
    lv_image_set_src(fx_bolt, &img_fx_bolt);
    lv_obj_set_pos(fx_bolt, 352, 4);   /* in the sky right of the weather picture */
    lv_obj_add_flag(fx_bolt, LV_OBJ_FLAG_HIDDEN);
    fx_timer = lv_timer_create(fx_tick, FX_TICK_MS, NULL);
    lv_timer_pause(fx_timer);
}

/* ------------------------------------------------------------------ the yellow-eyed cat */
/* Sits on the top edge of a forecast card. Every outfit piece is its own small
 * image stacked on the cat (see cat.h for who wears what when), so one set of
 * art covers every combination. */

static const wb_kitty_set_t *K = &wb_kitty;
static bool s_prop_left;   /* props on the cat's left instead of its right */
static int s_stage_x, s_stage_y;   /* stage position on the page */
static lv_obj_t *kitty_stage, *kitty_grp, *kitty_base, *kitty_blink, *kitty_neck, *kitty_face, *kitty_hat, *kitty_prop;
static lv_obj_t *kitty_flame[9];
static uint32_t s_blink_at, s_blink_until;

static lv_obj_t *kitty_layer(lv_obj_t *parent)
{
    lv_obj_t *o = lv_image_create(parent);
    lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
    return o;
}

static void kitty_set(lv_obj_t *o, const wb_layer_t *l)
{
    if (!l) {
        lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_image_set_src(o, l->img);
    lv_obj_set_pos(o, l->x, l->y);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
}

static void blink_now(uint32_t ms)
{
    s_blink_until = lv_tick_get() + ms;
    lv_obj_remove_flag(kitty_blink, LV_OBJ_FLAG_HIDDEN);
}

static void kitty_tick(lv_timer_t *t)
{
    (void)t;
    uint32_t now = lv_tick_get();
    if (s_blink_until && (int32_t)(now - s_blink_until) >= 0) {
        lv_obj_add_flag(kitty_blink, LV_OBJ_FLAG_HIDDEN);
        s_blink_until = 0;
        /* Next blink in 2.5-6 s; now and then a quick double blink. */
        s_blink_at = now + (lv_rand(0, 4) == 0 ? 260 : lv_rand(2500, 6000));
    } else if (!s_blink_until && (int32_t)(now - s_blink_at) >= 0) {
        blink_now(150);
    }
}

static void hop_exec(void *obj, int32_t v)
{
    lv_obj_set_style_translate_y(obj, v, 0);
}

static void on_kitty_tap(lv_event_t *e)
{
    (void)e;
    if (lv_anim_get(kitty_grp, hop_exec)) {
        return;
    }
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, kitty_grp);
    lv_anim_set_exec_cb(&a, hop_exec);
    lv_anim_set_values(&a, 0, -K->h / 8);
    lv_anim_set_duration(&a, 170);
    lv_anim_set_reverse_duration(&a, 230);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
    blink_now(220);
}

/* stage_x / card_top in page coordinates. */
static void build_kitty(lv_obj_t *t, const wb_kitty_set_t *set, int stage_x, int card_top, bool prop_left)
{
    K = set;
    s_prop_left = prop_left;
    s_stage_x = stage_x;
    s_stage_y = card_top - K->edge;

    /* Props (and the menorah's flames) go on the page itself, behind the cat. */
    kitty_prop = kitty_layer(t);
    for (int i = 0; i < 9; i++) {
        kitty_flame[i] = kitty_layer(t);
        lv_image_set_src(kitty_flame[i], K->flame);
    }

    kitty_stage = box(t);
    lv_obj_set_pos(kitty_stage, s_stage_x, s_stage_y);
    lv_obj_set_size(kitty_stage, K->w, K->h);

    kitty_grp = box(kitty_stage);
    lv_obj_set_size(kitty_grp, K->w, K->h);
    kitty_base = kitty_layer(kitty_grp);
    kitty_blink = kitty_layer(kitty_grp);
    kitty_neck = kitty_layer(kitty_grp);
    kitty_face = kitty_layer(kitty_grp);
    kitty_hat = kitty_layer(kitty_grp);
    kitty_set(kitty_base, &K->base);
    kitty_set(kitty_blink, &K->blink);
    lv_obj_add_flag(kitty_blink, LV_OBJ_FLAG_HIDDEN);

    /* Tap the cat and it hops. Long press still bubbles up to open Settings. */
    lv_obj_add_flag(kitty_base, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_event_cb(kitty_base, on_kitty_tap, LV_EVENT_CLICKED, NULL);

    s_blink_at = lv_tick_get() + 3000;
    lv_timer_create(kitty_tick, 50, NULL);
}

static void kitty_show(const wx_t *wx, const advice_t *adv)
{
    int y = wx->year, m = wx->month, d = wx->day, h = wx->hour;
    struct tm t;
    if (s_time_cb && s_time_cb(&t)) {
        y = t.tm_year + 1900;
        m = t.tm_mon + 1;
        d = t.tm_mday;
        h = t.tm_hour;
    }
    cat_look_t look;
    cat_pick_look(y, m, d, h, wx, adv, &look);

    const wb_layer_t *hat = look.hat == CAT_HAT_WITCH ? &K->hat_witch : look.hat == CAT_HAT_SANTA ? &K->hat_santa
                            : look.hat == CAT_HAT_BEANIE ? &K->hat_beanie : look.hat == CAT_HAT_RAIN ? &K->hat_rain
                                                                                                     : NULL;
    const wb_layer_t *neck = look.neck == CAT_NECK_CAPE ? &K->neck_cape : look.neck == CAT_NECK_SCARF ? &K->neck_scarf
                             : look.neck == CAT_NECK_SCARF_BLUE ? &K->neck_scarf_blue : NULL;
    const wb_layer_t *prop = look.prop == CAT_PROP_PUMPKIN ? &K->prop_pumpkin
                             : look.prop == CAT_PROP_PRESENT ? &K->prop_present
                             : look.prop == CAT_PROP_MENORAH ? &K->prop_menorah : NULL;
    kitty_set(kitty_hat, hat);
    kitty_set(kitty_neck, neck);
    kitty_set(kitty_face, look.face == CAT_FACE_SUNGLASSES ? &K->face_sunglasses : NULL);

    /* Props normally sit to the cat's right; some layouts want them on its left. */
    wb_layer_t moved;
    int dx = 0;
    if (prop) {
        moved = *prop;
        if (s_prop_left) {
            moved.x = (int16_t)(K->base.x - prop->img->header.w - 2);
        }
        dx = moved.x - prop->x;
        moved.x += s_stage_x;   /* stage -> page coordinates */
        moved.y += s_stage_y;
        prop = &moved;
    }
    kitty_set(kitty_prop, prop);

    /* Menorah: candles are added right to left, so night n lights the n rightmost
     * (skipping the shamash in the middle), plus the shamash itself. */
    static const int order[8] = {8, 7, 6, 5, 3, 2, 1, 0};
    bool lit[9] = {false};
    if (look.prop == CAT_PROP_MENORAH && look.candles > 0) {
        lit[4] = true;
        for (int i = 0; i < look.candles && i < 8; i++) {
            lit[order[i]] = true;
        }
    }
    for (int i = 0; i < 9; i++) {
        lv_obj_set_pos(kitty_flame[i], s_stage_x + K->flames[i].x + dx, s_stage_y + K->flames[i].y);
        if (lit[i]) {
            lv_obj_remove_flag(kitty_flame[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(kitty_flame[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

    fx_set(look.fx);
}

/* ------------------------------------------------------------------ today page */
/* The cat sits in the middle and the speech bubble is its advice; the weather
 * is a compact block top-left; the bottom row is the girls' school day. */
static lv_obj_t *p_sub[WX_MOMENTS];

#define TY(y) ((y) - 58)   /* screen y -> today-page y (the pages start 58 px down) */
#define KID_CARD_TOP 326    /* screen y of the moment cards */
#define KID_CARD_H 130
#define DAY_CARD_TOP 108    /* screen y of the next-days cards */
#define DAY_CARD_H 346

static void build_today(lv_obj_t *t)
{
    build_fx(t);
    img_hero = lv_image_create(t);
    lbl_temp = label(t, &font_temp_xl, "--" DEG);
    lbl_cond = label(t, &font_small, "");
    lv_obj_set_pos(lbl_temp, 20, TY(60));
    lv_obj_set_pos(img_hero, 232, TY(66));
    lv_obj_set_pos(lbl_cond, 26, TY(182));

    /* speech bubble */
    const int bx = 412, by = TY(66), bw = 364, bh = KID_CARD_TOP - 66 - 26;
    bubble_tail = box(t);
    lv_obj_set_size(bubble_tail, 30, 30);
    lv_obj_set_pos(bubble_tail, bx - 11, TY(256));   /* points left, at the cat */
    lv_obj_set_style_bg_opa(bubble_tail, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(bubble_tail, 4, 0);
    lv_obj_set_style_transform_pivot_x(bubble_tail, 15, 0);
    lv_obj_set_style_transform_pivot_y(bubble_tail, 15, 0);
    lv_obj_set_style_transform_rotation(bubble_tail, 450, 0);

    /* A flat offset "drop" under the bubble. It looks like a sticker shadow and is
     * cheap to draw; the blurred shadow it replaces was slow on this chip. */
    bubble_drop = box(t);
    lv_obj_set_pos(bubble_drop, bx, by + 7);
    lv_obj_set_size(bubble_drop, bw, bh);
    lv_obj_set_style_radius(bubble_drop, 28, 0);
    lv_obj_set_style_bg_color(bubble_drop, C(0x1B2542), 0);
    lv_obj_set_style_bg_opa(bubble_drop, 38, 0);

    bubble = box(t);
    lv_obj_set_pos(bubble, bx, by);
    lv_obj_set_size(bubble, bw, bh);
    lv_obj_set_style_radius(bubble, 28, 0);
    lv_obj_set_style_bg_opa(bubble, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_hor(bubble, 24, 0);
    lv_obj_set_style_pad_ver(bubble, 20, 0);
    lv_obj_set_style_pad_row(bubble, 8, 0);
    lv_obj_set_style_shadow_width(bubble, 0, 0);   /* blurred shadow only in speed-test "old" mode */
    lv_obj_set_style_shadow_opa(bubble, 40, 0);
    lv_obj_set_style_shadow_offset_y(bubble, 8, 0);
    lv_obj_set_style_shadow_color(bubble, C(0x1B2542), 0);
    lv_obj_set_flex_flow(bubble, LV_FLEX_FLOW_COLUMN);

    lbl_head = label(bubble, &font_title, "");
    lv_obj_set_width(lbl_head, lv_pct(100));
    lv_label_set_long_mode(lbl_head, LV_LABEL_LONG_MODE_WRAP);
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
    lv_obj_set_style_pad_column(wear_row, 18, 0);
    for (int i = 0; i < ADVICE_MAX_WEAR; i++) {
        wear_chip[i] = box(wear_row);
        lv_obj_set_size(wear_chip[i], LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        wear_img[i] = lv_image_create(wear_chip[i]);   /* no captions: the pictures say it */
    }

    /* walk to school / recess / going home */
    for (int i = 0; i < WX_MOMENTS; i++) {
        lv_obj_t *c = card(t, 24 + i * 256, TY(KID_CARD_TOP), 240, KID_CARD_H);
        pcard[i] = c;
        p_lbl[i] = label(c, &font_small, "");
        lv_obj_align(p_lbl[i], LV_ALIGN_TOP_MID, 0, 10);
        p_sub[i] = label(c, &font_tiny, "");
        lv_obj_align(p_sub[i], LV_ALIGN_TOP_MID, 0, 36);
        lv_obj_t *row = box(c);
        lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(row, 6, 0);
        lv_obj_align(row, LV_ALIGN_TOP_MID, 0, 56);
        p_img[i] = lv_image_create(row);
        p_temp[i] = label(row, &font_temp_l, "");
        p_drop[i] = lv_image_create(row);
        lv_image_set_src(p_drop[i], &img_drop);
        lv_obj_set_style_margin_left(p_drop[i], 6, 0);
        p_pop[i] = label(row, &font_small, "");
    }

    /* After the cards, so the paws sit on top of them. The cat stands beside the
     * bubble it's talking with; props go on its left. */
    build_kitty(t, &wb_kitty, 226, TY(KID_CARD_TOP), true);
}

static void build_days(lv_obj_t *t)
{
    lbl_days_title = label(t, &font_title, "Next few days");
    lv_obj_set_pos(lbl_days_title, 24, 2);
    for (int i = 0; i < 3; i++) {
        lv_obj_t *c = card(t, 20 + i * 260, TY(DAY_CARD_TOP), 240, DAY_CARD_H);
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
    lv_obj_add_flag(lbl_time, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(lbl_time, on_long_press, LV_EVENT_LONG_PRESSED, NULL);

    /* Two pages. A swipe flips straight to the other one (on_swipe); there's no
     * dragging, because moving the whole 800x480 picture under a finger is the
     * slowest thing this chip can draw. */
    tv = lv_tileview_create(scr_main);
    lv_obj_set_pos(tv, 0, 58);
    lv_obj_set_size(tv, UI_W, 400);
    lv_obj_set_style_bg_opa(tv, LV_OPA_TRANSP, 0);
    lv_obj_set_scrollbar_mode(tv, LV_SCROLLBAR_MODE_OFF);
    tile0 = lv_tileview_add_tile(tv, 0, 0, LV_DIR_RIGHT);
    tile1 = lv_tileview_add_tile(tv, 1, 0, LV_DIR_LEFT);
    lv_obj_set_style_bg_opa(tile0, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(tile1, LV_OPA_TRANSP, 0);
    lv_obj_remove_flag(tv, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(tile0, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(tile1, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(tile0, on_long_press, LV_EVENT_LONG_PRESSED, NULL);
    lv_obj_add_event_cb(tile1, on_long_press, LV_EVENT_LONG_PRESSED, NULL);
    build_today(tile0);
    build_days(tile1);

    /* footer: just when the weather was last updated (the town is in Settings) */
    lbl_status = label(scr_main, &font_tiny, "");
    lv_obj_align(lbl_status, LV_ALIGN_TOP_RIGHT, -24, 459);
    lv_obj_add_flag(lbl_status, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(lbl_status, 16);
    lv_obj_add_event_cb(lbl_status, on_footer_tap, LV_EVENT_CLICKED, NULL);

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
    lv_obj_t *txt[] = {lbl_greet, lbl_time, lbl_temp, lbl_days_title, lbl_status};
    for (size_t i = 0; i < sizeof(txt) / sizeof(txt[0]); i++) {
        lv_obj_set_style_text_color(txt[i], C(th->text), 0);
    }
    lv_obj_set_style_text_color(lbl_cond, C(th->text2), 0);
    lv_obj_set_style_bg_color(bubble, C(th->bubble), 0);
    lv_obj_set_style_bg_color(bubble_tail, C(th->bubble), 0);
    lv_obj_set_style_text_color(lbl_head, C(th->accent), 0);
    lv_obj_set_style_text_color(lbl_tip, C(th->bubble_text), 0);
    for (int i = 0; i < WX_MOMENTS; i++) {
        card_fill(pcard[i], KID_CARD_TOP, KID_CARD_TOP + KID_CARD_H, th->card_opa);
        lv_obj_set_style_text_color(p_lbl[i], C(th->text), 0);
        lv_obj_set_style_text_color(p_sub[i], C(th->text2), 0);
        lv_obj_set_style_text_color(p_temp[i], C(th->text), 0);
        lv_obj_set_style_text_color(p_pop[i], C(th->text2), 0);
    }
    for (int i = 0; i < 3; i++) {
        card_fill(dcard[i], DAY_CARD_TOP, DAY_CARD_TOP + DAY_CARD_H, th->card_opa);
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
    lv_obj_align(lbl_time, LV_ALIGN_TOP_RIGHT, -24, 14);
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

/* ------------------------------------------------------------------ speed test */

/* Moments that already happened are faded. Fading each piece is cheap; fading the
 * whole card (the old way, kept for the speed test) makes LVGL draw it into a
 * separate layer first. */
/* The sky colour at a screen row (the main screen's vertical gradient). */
static lv_color_t sky_at(int y)
{
    int mix = y <= 0 ? 0 : y >= UI_H ? 255 : y * 255 / UI_H;
    return lv_color_mix(C(s_theme->bottom), C(s_theme->top), (uint8_t)mix);
}

/* Paints a card solid, in exactly the colours the see-through card had over the
 * sky. A solid card is quicker to draw, and LVGL skips the sky behind it. In the
 * speed test's "old" mode it goes back to see-through. */
static void card_fill(lv_obj_t *o, int screen_y0, int screen_y1, lv_opa_t opa)
{
    if (s_speed == 2) {
        lv_obj_set_style_bg_color(o, C(s_theme->card), 0);
        lv_obj_set_style_bg_grad_dir(o, LV_GRAD_DIR_NONE, 0);
        lv_obj_set_style_bg_opa(o, opa, 0);
        return;
    }
    lv_color_t card = C(s_theme->card);
    lv_obj_set_style_bg_color(o, lv_color_mix(card, sky_at(screen_y0), opa), 0);
    lv_obj_set_style_bg_grad_color(o, lv_color_mix(card, sky_at(screen_y1), opa), 0);
    lv_obj_set_style_bg_grad_dir(o, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
}

static void style_card(int i)
{
    bool past = s_past[i];
    bool old = s_speed == 2;
    lv_obj_set_style_opa(pcard[i], past && old ? LV_OPA_50 : LV_OPA_COVER, 0);
    lv_opa_t dim = past && !old ? 120 : LV_OPA_COVER;
    card_fill(pcard[i], KID_CARD_TOP, KID_CARD_TOP + KID_CARD_H,
              past && !old ? s_theme->card_opa / 2 : s_theme->card_opa);
    lv_obj_t *txt[] = {p_lbl[i], p_sub[i], p_temp[i], p_pop[i]};
    for (size_t k = 0; k < sizeof(txt) / sizeof(txt[0]); k++) {
        lv_obj_set_style_text_opa(txt[k], dim, 0);
    }
    lv_obj_set_style_image_opa(p_img[i], dim, 0);
    lv_obj_set_style_image_opa(p_drop[i], dim, 0);
}

static void show_footer(void)
{
    if (s_speed) {
        char info[48] = "simulator";
        if (s_speed_cb) {
            s_speed_cb(s_speed == 2, info, sizeof(info));
        }
        lv_label_set_text_fmt(lbl_status, "Speed test: %s settings \xC2\xB7 %s", s_speed == 2 ? "old" : "new", info);
        lv_obj_set_style_text_color(lbl_status, C(s_theme->text), 0);
    } else {
        lv_label_set_text(lbl_status, s_footer);
        lv_obj_set_style_text_color(lbl_status, s_footer_warn ? C(0xE0457B) : C(s_theme->text), 0);
    }
    lv_obj_align(lbl_status, LV_ALIGN_TOP_RIGHT, -24, 459);
}

static void speed_timer_cb(lv_timer_t *t)
{
    (void)t;
    show_footer();   /* free memory changes, so keep the readout current */
}

static void on_footer_tap(lv_event_t *e)
{
    (void)e;
    s_speed = (s_speed + 1) % 3;
    lv_display_t *d = lv_display_get_default();
    if (s_speed) {
        lv_sysmon_show_performance(d);
        lv_timer_resume(s_speed_timer);
    } else {
        lv_sysmon_hide_performance(d);
        lv_timer_pause(s_speed_timer);
        if (s_speed_cb) {
            char tmp[8];
            s_speed_cb(false, tmp, sizeof(tmp));   /* back to the new settings */
        }
    }
    bool old = s_speed == 2;
    lv_obj_set_style_shadow_width(bubble, old ? 28 : 0, 0);
    if (old) {
        lv_obj_add_flag(bubble_drop, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(bubble_drop, LV_OBJ_FLAG_HIDDEN);
    }
    for (int i = 0; i < WX_MOMENTS; i++) {
        style_card(i);
    }
    for (int i = 0; i < 3; i++) {
        card_fill(dcard[i], DAY_CARD_TOP, DAY_CARD_TOP + DAY_CARD_H, s_theme->card_opa);
    }
    show_footer();
}

void ui_set_speed_cb(ui_speed_cb_t cb)
{
    s_speed_cb = cb;
}

/* ------------------------------------------------------------------ weather */

/* Height the bubble's contents need right now. */
static int bubble_need(void)
{
    lv_obj_update_layout(bubble);
    return lv_obj_get_height(lbl_head) + lv_obj_get_height(lbl_tip) + lv_obj_get_height(wear_row) + 2 * 8;
}

/* Long advice: step the tip, then the headline, down a font size until it fits. */
static void fit_bubble(void)
{
    lv_obj_set_style_text_font(lbl_head, &font_title, 0);
    lv_obj_set_style_text_font(lbl_tip, &font_body, 0);
    int avail = lv_obj_get_content_height(bubble);
    if (bubble_need() <= avail) {
        return;
    }
    lv_obj_set_style_text_font(lbl_tip, &font_small, 0);
    if (bubble_need() <= avail) {
        return;
    }
    lv_obj_set_style_text_font(lbl_head, &font_body, 0);
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

    /* weather block */
    lv_image_set_src(img_hero, ICON_M[wx_icon_for(wx->code, wx->is_day)]);
    lv_label_set_text_fmt(lbl_temp, "%d" DEG, iround(wx->temp));
    if (wx->ndays > 0) {
        lv_label_set_text_fmt(lbl_cond, "%s  \xC2\xB7  High %d" DEG, wx_describe(wx->code, wx->is_day),
                              iround(wx->days[0].tmax));
    } else {
        lv_label_set_text(lbl_cond, wx_describe(wx->code, wx->is_day));
    }

    /* what the cat says */
    advice_t a;
    advice_make(wx, &s_opts, &a);
    lv_label_set_text(lbl_head, a.kid_head);
    lv_label_set_text(lbl_tip, a.tip);
    for (int i = 0; i < ADVICE_MAX_WEAR; i++) {
        if (i < a.nwear) {
            lv_image_set_src(wear_img[i], WEAR_ICONS[a.wear[i]]);
            lv_obj_remove_flag(wear_chip[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(wear_chip[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
    fit_bubble();
    kitty_show(wx, &a);

    /* walk to school / recess / going home */
    struct tm now;
    int hh = wx->hour, mm = wx->minute;
    if (s_time_cb && s_time_cb(&now)) {
        hh = now.tm_hour;
        mm = now.tm_min;
    }
    wx_moment_t mo[WX_MOMENTS];
    int n = wx_kid_moments(wx, hh, mm, mo);
    for (int i = 0; i < WX_MOMENTS; i++) {
        if (i >= n) {
            lv_obj_add_flag(pcard[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_remove_flag(pcard[i], LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(p_lbl[i], mo[i].title);
        lv_label_set_text(p_sub[i], mo[i].sub);
        lv_image_set_src(p_img[i], ICON_S[wx_icon_for(mo[i].code, mo[i].is_day)]);
        lv_label_set_text_fmt(p_temp[i], "%d" DEG, iround(mo[i].temp));
        /* Rain chance only when it matters; otherwise the picture says it. */
        if (mo[i].pop >= 30) {
            lv_obj_remove_flag(p_drop[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(p_pop[i], LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_fmt(p_pop[i], "%d%%", mo[i].pop);
        } else {
            lv_obj_add_flag(p_drop[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(p_pop[i], LV_OBJ_FLAG_HIDDEN);
        }
        s_past[i] = mo[i].past;
        style_card(i);
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
    (void)place;   /* the town is shown in Settings */
    snprintf(s_footer, sizeof(s_footer), "%s", status ? status : "");
    s_footer_warn = warn;
    show_footer();
}

void ui_show_page(int page)
{
    lv_tileview_set_tile_by_index(tv, page ? 1 : 0, 0, LV_ANIM_OFF);
    update_dots();
    fx_page(page ? 1 : 0);
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
    s_speed_timer = lv_timer_create(speed_timer_cb, 1000, NULL);
    for (lv_indev_t *in = lv_indev_get_next(NULL); in; in = lv_indev_get_next(in)) {
        lv_indev_add_event_cb(in, on_indev, LV_EVENT_ALL, NULL);   /* swipes */
    }
    lv_timer_pause(s_speed_timer);
#if LV_USE_PERF_MONITOR
    lv_sysmon_hide_performance(lv_display_get_default());   /* LVGL shows it by default */
#endif
    ui_show_boot(NULL);
}
