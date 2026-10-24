#include "ui.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "assets/assets.h"
#include "assets/cat2_sets.h"
#include "cat.h"
#include "lvgl.h"
#include "outfit.h"

#define DEG "\xC2\xB0"
#define MDOT "\xC2\xB7"   /* middle dot */

/* ------------------------------------------------------------------ palette (plan 2) */

#define COL_BG_MORNING 0xFBF6EE
#define COL_BG_EVENING 0xF2EFF8
#define COL_BG_FORECAST 0xF4F8FC
#define COL_INK 0x1E2340
#define COL_INK2 0x545A72
#define COL_TILE 0xFFFFFF
#define COL_TILE_LINE_M 0xE7D9BE   /* plan token deepened a step: at 0xECE4D5 the tiles vanished into the bg */
#define COL_TILE_LINE_E 0xD8CDF0
#define COL_LAYER_LINE 0xE8A33D   /* amber frame for the extra-layer tile, so a pre-reader can spot it */
#define COL_CHIP_M_BG 0xFFE6A6
#define COL_CHIP_M_TX 0x5A3B00
#define COL_CHIP_E_BG 0xE2DCF7
#define COL_CHIP_E_TX 0x3B3486
#define COL_OK 0x2FA866
#define COL_OK_BG 0xE8F6EE
#define COL_OK_LINE 0x9ED8B6
#define COL_RING_OFF 0xCFC7B8
#define COL_WARN_BG 0xFFEBC7
#define COL_WARN_TX 0x8A4B00
#define COL_ACCENT 0xE0457B
#define COL_TAB_LINE 0xD9D1C2

#define C(hex) lv_color_hex(hex)

/* ------------------------------------------------------------------ state */

static const lv_image_dsc_t *ICON_M[] = WB_WX_ICONS_M;
static const lv_image_dsc_t *ICON_S[] = WB_WX_ICONS_S;

/* Indexed by garment_t (outfit.h). Asset names from tools/make_assets.py. */
#define GARMENT_COUNT 18   /* garment_t count in outfit.h */
static const lv_image_dsc_t *GARMENT_96[GARMENT_COUNT];
static const lv_image_dsc_t *GARMENT_56[GARMENT_COUNT];
static const lv_image_dsc_t *EXTRA_IMG[8];   /* EXTRA_ICON_* ids 0..7 */

static char s_names[48] = "Cleo & Lila";
static ui_action_cb_t s_act_cb;
static void *s_act_user;
static ui_time_cb_t s_time_cb;
static ui_cat_cb_t s_cat_cb;
static int s_cat_choice;   /* 0 black, 1 pink, 2 take turns */

static outfit_opts_t s_opts = {.bike_bus_wday = 5}; /* Friday */

/* Weather: our own copy, so the firmware's fetch buffer can be reused. */
static wx_t s_wx;
static bool s_have_wx;
static char s_footer[64];
static bool s_footer_warn;

/* Pages: 0 Today, 1 Tomorrow, 2 Next days. */
static int s_page;
static uint32_t s_last_touch;   /* lv_tick of the last press (auto-return timer) */
static bool s_page_touched;     /* a manual page exists to revert from */

/* Garment check-offs, keyed to the outfit's target date (plan 3.3). */
static uint8_t s_checks;
static int s_check_y, s_check_m, s_check_d;
static long s_check_date;   /* outfit date the check marks belong to (-2 = adopt, test hook) */
static bool s_force = true;
static bool s_celebrating;
static uint32_t s_happy_until;

/* Speed test (Settings pill now, plan 3.6). */
static ui_speed_cb_t s_speed_cb;
static int s_speed;   /* 0 off, 1 new settings, 2 old settings */
static lv_timer_t *s_speed_timer;

/* Render cache: the plan 3.5 key plus the last outfit date shown. */
static int s_last_min = -1;
static char s_key[64] = "";

static lv_obj_t *scr_boot, *scr_setup, *scr_main;

/* boot / connecting / error */
static lv_obj_t *boot_title, *boot_msg, *boot_detail, *boot_spinner, *boot_btn;

/* setup */
static lv_obj_t *setup_qr, *setup_steps;

/* main: outfit page (shared by Today and Tomorrow) */
static lv_obj_t *page_outfit, *page_forecast;
static lv_obj_t *lbl_title, *chip, *lbl_chip;
static lv_obj_t *wx_icon, *lbl_wx1, *lbl_wx2;
static lv_obj_t *cat_stage, *cat_grp;
static lv_obj_t *cat_layer[WB_C2_COUNT];
static lv_obj_t *cat_flame[9];
static lv_obj_t *sparkle[3];
static lv_obj_t *tile[4], *tile_icon[4], *tile_label[4], *tile_check[4], *tile_check_mark[4], *tile_badge[4];
static bool s_tile_layer[4];   /* tile i is the extra layer (amber frame + star badge) */
static bool s_done_hold;       /* all tiles checked: hold the happy cat + sparkles */
static lv_obj_t *prog_chip, *lbl_prog;
static lv_obj_t *lbl_no_data, *lbl_no_data2;
static lv_obj_t *adv_icon, *lbl_advice;

/* main: forecast page */
static lv_obj_t *fc_col[3], *fc_name[3], *fc_date[3], *fc_img[3], *fc_hi[3], *fc_lo[3];
static lv_obj_t *fc_rain[3], *fc_drop[3], *fc_pop[3], *fc_garment[3], *fc_gimg[3], *fc_glabel[3];

/* main: bottom bar */
static lv_obj_t *tab_btn[3], *tab_lbl[3];
static lv_obj_t *stale_chip, *lbl_stale;

/* settings overlay */
static lv_obj_t *settings, *set_info, *set_speed_lbl;
static lv_obj_t *cat_pill[3], *cat_pill_lbl[3];

static void render(void);
static lv_obj_t *pill_button(lv_obj_t *parent, const char *txt, bool primary, lv_event_cb_t cb, void *ud);

static void on_long_press(lv_event_t *e);

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

/* Days since 1970-01-01 (Howard Hinnant's days_from_civil; same as cat.c). */
static long days_from_civil(int y, int m, int d)
{
    y -= m <= 2;
    long era = (y >= 0 ? y : y - 399) / 400;
    long yoe = y - era * 400;
    long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

/* Minutes since the civil epoch, for forecast-age math. */
static long minutes_of(int y, int mo, int d, int h, int mi)
{
    return days_from_civil(y, mo, d) * 1440 + h * 60 + mi;
}

static void fmt_time12(int h, int mi, char *buf, size_t n)
{
    int h12 = h % 12;
    if (h12 == 0) {
        h12 = 12;
    }
    snprintf(buf, n, "%d:%02d %s", h12, mi, h < 12 ? "AM" : "PM");
}

static int text_w(const char *txt, const lv_font_t *f)
{
    lv_point_t sz;
    lv_text_get_size(&sz, txt, f, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    return (int)sz.x;
}

static void hide(lv_obj_t *o)
{
    lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
}

static void show_obj(lv_obj_t *o)
{
    lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
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

static void solid_bg(lv_obj_t *o, uint32_t col)
{
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(o, C(col), 0);
    lv_obj_set_style_bg_grad_dir(o, LV_GRAD_DIR_NONE, 0);
}

static void build_boot(void)
{
    scr_boot = lv_obj_create(NULL);
    lv_obj_remove_flag(scr_boot, LV_OBJ_FLAG_SCROLLABLE);
    solid_bg(scr_boot, COL_BG_MORNING);

    cats_pair(scr_boot, 78, true);

    boot_title = label(scr_boot, &font_title, "Weather Besties");
    lv_obj_set_style_text_color(boot_title, C(COL_INK), 0);
    lv_obj_align(boot_title, LV_ALIGN_TOP_MID, 0, 214);

    boot_msg = label(scr_boot, &font_body, "");
    lv_obj_set_style_text_color(boot_msg, C(COL_INK), 0);
    lv_obj_set_style_text_align(boot_msg, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(boot_msg, 700);
    lv_obj_align(boot_msg, LV_ALIGN_TOP_MID, 0, 266);

    boot_detail = label(scr_boot, &font_small, "");
    lv_obj_set_style_text_color(boot_detail, C(COL_INK2), 0);
    lv_obj_set_style_text_align(boot_detail, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(boot_detail, 700);
    lv_obj_align(boot_detail, LV_ALIGN_TOP_MID, 0, 306);

    boot_spinner = lv_spinner_create(scr_boot);
    lv_obj_set_size(boot_spinner, 48, 48);
    lv_spinner_set_anim_params(boot_spinner, 1100, 70);
    lv_obj_set_style_arc_width(boot_spinner, 7, LV_PART_MAIN);
    lv_obj_set_style_arc_width(boot_spinner, 7, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(boot_spinner, C(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_arc_color(boot_spinner, C(COL_ACCENT), LV_PART_INDICATOR);
    lv_obj_align(boot_spinner, LV_ALIGN_TOP_MID, 0, 356);

    boot_btn = pill_button(scr_boot, LV_SYMBOL_WIFI "  Set up WiFi", true, on_setup_btn, NULL);
    lv_obj_align(boot_btn, LV_ALIGN_TOP_MID, 0, 360);
    hide(boot_btn);
}


static void boot_mode(const char *msg, const char *detail, bool spinner, bool button)
{
    lv_label_set_text(boot_msg, msg ? msg : "");
    lv_label_set_text(boot_detail, detail ? detail : "");
    if (spinner) {
        show_obj(boot_spinner);
    } else {
        hide(boot_spinner);
    }
    if (button) {
        show_obj(boot_btn);
    } else {
        hide(boot_btn);
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
    solid_bg(scr_setup, 0xFFF6FA);

    lv_obj_t *qcard = box(scr_setup);
    lv_obj_set_pos(qcard, 40, 64);
    lv_obj_set_size(qcard, 300, 352);
    lv_obj_set_style_radius(qcard, 24, 0);
    lv_obj_set_style_bg_opa(qcard, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(qcard, lv_color_white(), 0);

    setup_qr = lv_qrcode_create(qcard);
    lv_qrcode_set_size(setup_qr, 248);
    lv_qrcode_set_dark_color(setup_qr, C(COL_INK));
    lv_qrcode_set_light_color(setup_qr, lv_color_white());
    lv_obj_align(setup_qr, LV_ALIGN_TOP_MID, 0, 26);

    lv_obj_t *cap = label(qcard, &font_small, "Scan with your phone camera");
    lv_obj_set_style_text_color(cap, C(COL_INK2), 0);
    lv_obj_align(cap, LV_ALIGN_BOTTOM_MID, 0, -22);

    lv_obj_t *col = box(scr_setup);
    lv_obj_set_pos(col, 380, 34);
    lv_obj_set_size(col, 390, 420);

    cats_pair(col, 0, false);

    lv_obj_t *t = label(col, &font_title, "Let's get online!");
    lv_obj_set_style_text_color(t, C(COL_INK), 0);
    lv_obj_set_pos(t, 0, 128);

    setup_steps = label(col, &font_body, "");
    lv_obj_set_style_text_color(setup_steps, C(COL_INK), 0);
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

/* ------------------------------------------------------------------ shared pill button */

static lv_obj_t *pill_button(lv_obj_t *parent, const char *txt, bool primary, lv_event_cb_t cb, void *ud)
{
    lv_obj_t *b = lv_button_create(parent);
    lv_obj_remove_style_all(b);
    lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(b, primary ? C(COL_INK) : C(0xEEF1F7), 0);   /* accent pink is for hearts / all-done only */
    lv_obj_set_style_bg_color(b, primary ? C(0xB8325F) : C(0xD5DBE6), LV_STATE_PRESSED);
    lv_obj_set_style_pad_hor(b, 26, 0);
    lv_obj_set_style_pad_ver(b, 14, 0);
    lv_obj_t *l = label(b, &font_body, txt);
    lv_obj_set_style_text_color(l, primary ? lv_color_white() : C(COL_INK), 0);
    lv_obj_center(l);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, ud);
    return b;
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

static void style_cat_pills(void)
{
    for (int i = 0; i < 3; i++) {
        bool on = s_cat_choice == i;
        lv_obj_set_style_bg_color(cat_pill[i], on ? C(COL_INK) : lv_color_white(), 0);
        lv_obj_set_style_bg_opa(cat_pill[i], on ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_color(cat_pill[i], on ? C(COL_INK) : C(COL_TAB_LINE), 0);
        lv_obj_set_style_text_color(cat_pill_lbl[i], on ? lv_color_white() : C(COL_INK), 0);
    }
}

static void on_cat_pill(lv_event_t *e)
{
    intptr_t which = (intptr_t)lv_event_get_user_data(e);
    s_cat_choice = (int)which;
    style_cat_pills();
    if (s_cat_cb) {
        s_cat_cb(s_cat_choice);   /* firmware saves + calls ui_set_cat_choice, which re-renders */
    } else {
        render();
    }
}

static void show_speed_lbl(void)
{
    if (s_speed) {
        char info[48] = "simulator";
        if (s_speed_cb) {
            s_speed_cb(s_speed == 2, info, sizeof(info));
        }
        lv_label_set_text_fmt(set_speed_lbl, "Speed test: %s settings \xC2\xB7 %s", s_speed == 2 ? "old" : "new",
                              info);
    } else {
        lv_label_set_text(set_speed_lbl, "Speed test: off");
    }
}

static void speed_timer_cb(lv_timer_t *t)
{
    (void)t;
    if (s_speed && !lv_obj_has_flag(settings, LV_OBJ_FLAG_HIDDEN)) {
        show_speed_lbl();
    }
}

static void on_speed_pill(lv_event_t *e)
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
    show_speed_lbl();
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
    lv_obj_set_size(p, 640, 448);
    lv_obj_center(p);
    lv_obj_set_style_radius(p, 28, 0);
    lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(p, lv_color_white(), 0);
    lv_obj_set_style_pad_all(p, 24, 0);

    lv_obj_t *t = label(p, &font_title, LV_SYMBOL_SETTINGS "  Settings");
    lv_obj_set_style_text_color(t, C(COL_INK), 0);
    lv_obj_set_pos(t, 0, 0);

    /* Cat choice (plan 3.6). */
    lv_obj_t *cat_lbl = label(p, &font_body, "Cat:");
    lv_obj_set_style_text_color(cat_lbl, C(COL_INK), 0);
    lv_obj_set_pos(cat_lbl, 0, 58);
    static const char *cat_names[3] = {"Black cat", "Pink cat", "Take turns"};
    for (int i = 0; i < 3; i++) {
        lv_obj_t *b = lv_button_create(p);
        lv_obj_remove_style_all(b);
        lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(b, 2, 0);
        lv_obj_set_style_pad_hor(b, 20, 0);
        lv_obj_set_height(b, 48);
        lv_obj_set_pos(b, 70 + i * 178, 48);
        cat_pill[i] = b;
        cat_pill_lbl[i] = label(b, &font_chip, cat_names[i]);
        lv_obj_center(cat_pill_lbl[i]);
        lv_obj_add_event_cb(b, on_cat_pill, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }
    style_cat_pills();

    set_info = label(p, &font_body, "");
    lv_obj_set_style_text_color(set_info, C(COL_INK2), 0);
    lv_obj_set_style_text_line_space(set_info, 4, 0);
    lv_obj_set_width(set_info, 592);
    lv_label_set_long_mode(set_info, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_pos(set_info, 0, 116);

    lv_obj_t *sp = pill_button(p, "Speed test", false, on_speed_pill, NULL);
    lv_obj_set_pos(sp, 0, 284);
    set_speed_lbl = label(p, &font_small, "Speed test: off");
    lv_obj_set_style_text_color(set_speed_lbl, C(COL_INK2), 0);
    lv_obj_set_pos(set_speed_lbl, 190, 298);

    lv_obj_t *row = box(p);
    lv_obj_set_size(row, 592, LV_SIZE_CONTENT);
    lv_obj_align(row, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    pill_button(row, "Change WiFi or town", true, on_settings_btn, (void *)1);
    pill_button(row, LV_SYMBOL_REFRESH, false, on_settings_btn, (void *)2);
    pill_button(row, "Close", false, on_settings_btn, (void *)0);

    hide(settings);
}

void ui_set_info(const ui_info_t *i)
{
    char buf[448];
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
    show_obj(settings);
}

void ui_close_settings(void)
{
    hide(settings);
}

static void on_long_press(lv_event_t *e)
{
    (void)e;
    ui_open_settings();
}

void ui_set_cat_choice(int choice)
{
    s_cat_choice = choice >= 0 && choice <= 2 ? choice : 0;
    style_cat_pills();
    render();
}

void ui_set_cat_cb(ui_cat_cb_t cb)
{
    s_cat_cb = cb;
}

void ui_set_speed_cb(ui_speed_cb_t cb)
{
    s_speed_cb = cb;
}

/* ------------------------------------------------------------------ asset tables */

static void init_asset_tables(void)
{
    static bool done;
    if (done) {
        return;
    }
    done = true;
    GARMENT_96[SHORT_SLEEVES] = &img_g_short_sleeves_96;
    GARMENT_96[LONG_SLEEVES] = &img_g_long_sleeves_96;
    GARMENT_96[SHORTS] = &img_g_shorts_96;
    GARMENT_96[LONG_PANTS] = &img_g_long_pants_96;
    GARMENT_96[HOODIE] = &img_g_hoodie_96;
    GARMENT_96[LIGHT_JACKET] = &img_g_light_jacket_96;
    GARMENT_96[RAIN_JACKET] = &img_g_rain_jacket_96;
    GARMENT_96[WARM_COAT] = &img_g_warm_coat_96;
    GARMENT_96[WINTER_COAT] = &img_g_winter_coat_96;
    GARMENT_96[SNEAKERS] = &img_g_sneakers_96;
    GARMENT_96[RAIN_BOOTS] = &img_g_rain_boots_96;
    GARMENT_96[SNOW_BOOTS] = &img_g_snow_boots_96;
    GARMENT_96[WARM_HAT] = &img_g_warm_hat_96;
    GARMENT_96[GLOVES] = &img_g_gloves_96;
    GARMENT_96[UMBRELLA] = &img_g_umbrella_96;
    GARMENT_96[SUNGLASSES] = &img_g_sunglasses_96;
    GARMENT_96[WATER_BOTTLE] = &img_g_water_bottle_96;
    GARMENT_96[SCARF] = &img_g_scarf_96;
    GARMENT_56[SHORT_SLEEVES] = &img_g_short_sleeves_56;
    GARMENT_56[LONG_SLEEVES] = &img_g_long_sleeves_56;
    GARMENT_56[SHORTS] = &img_g_shorts_56;
    GARMENT_56[LONG_PANTS] = &img_g_long_pants_56;
    GARMENT_56[HOODIE] = &img_g_hoodie_56;
    GARMENT_56[LIGHT_JACKET] = &img_g_light_jacket_56;
    GARMENT_56[RAIN_JACKET] = &img_g_rain_jacket_56;
    GARMENT_56[WARM_COAT] = &img_g_warm_coat_56;
    GARMENT_56[WINTER_COAT] = &img_g_winter_coat_56;
    GARMENT_56[SNEAKERS] = &img_g_sneakers_56;
    GARMENT_56[RAIN_BOOTS] = &img_g_rain_boots_56;
    GARMENT_56[SNOW_BOOTS] = &img_g_snow_boots_56;
    GARMENT_56[WARM_HAT] = &img_g_warm_hat_56;
    GARMENT_56[GLOVES] = &img_g_gloves_56;
    GARMENT_56[UMBRELLA] = &img_g_umbrella_56;
    GARMENT_56[SUNGLASSES] = &img_g_sunglasses_56;
    GARMENT_56[WATER_BOTTLE] = &img_g_water_bottle_56;
    GARMENT_56[SCARF] = &img_g_scarf_56;
    EXTRA_IMG[EXTRA_ICON_BOLT] = &img_x_bolt;
    EXTRA_IMG[EXTRA_ICON_UMBRELLA] = &img_x_umbrella;
    EXTRA_IMG[EXTRA_ICON_HAT_GLOVES] = &img_x_hat_gloves;
    EXTRA_IMG[EXTRA_ICON_GLOVES] = &img_x_gloves;
    EXTRA_IMG[EXTRA_ICON_SUN_UP] = &img_x_sun_up;
    EXTRA_IMG[EXTRA_ICON_WATER] = &img_x_water;
    EXTRA_IMG[EXTRA_ICON_SUNGLASSES] = &img_x_sunglasses;
    EXTRA_IMG[EXTRA_ICON_WIND] = &img_x_wind;
}

/* garment ids for the forecast page's key garment */
static int layer_garment(layer_t l)
{
    switch (l) {
    case LAYER_HOODIE: return HOODIE;
    case LAYER_LIGHT_JACKET: return LIGHT_JACKET;
    case LAYER_RAIN_JACKET: return RAIN_JACKET;
    case LAYER_WARM_COAT: return WARM_COAT;
    case LAYER_WINTER_COAT: return WINTER_COAT;
    default: return -1;
    }
}

static int top_garment(top_t t)
{
    return t == TOP_SHORT_SLEEVES ? SHORT_SLEEVES : LONG_SLEEVES;
}

/* ------------------------------------------------------------------ swipes (kept from 1.x) */
/* LVGL's own gesture detection forgets a swipe whenever one touch reading shows
 * no movement, so this compares press and release points: 80+ px sideways,
 * mostly horizontal, flips the page. A finger that moved that far also doesn't
 * count as a tap or a long press. */

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

static void set_page(int p);

static void on_indev(lv_event_t *e)
{
    lv_indev_t *in = lv_event_get_target(e);
    lv_event_code_t code = lv_event_get_code(e);
    int32_t dx, dy;
    switch (code) {
    case LV_EVENT_PRESSED:
        lv_indev_get_point(in, &s_press_at);
        s_swiped = false;
        s_last_touch = lv_tick_get();
        break;
    case LV_EVENT_RELEASED:
        moved_px(in, &dx, &dy);
        if (lv_screen_active() == scr_main && lv_obj_has_flag(settings, LV_OBJ_FLAG_HIDDEN) &&
            LV_ABS(dx) >= SWIPE_MIN_PX && LV_ABS(dx) > LV_ABS(dy) * 3 / 2) {
            s_swiped = true;
            set_page(s_page + (dx < 0 ? 1 : -1));   /* finger moved left = next page */
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

/* ------------------------------------------------------------------ cat stage */

static uint32_t s_blink_at, s_blink_until;
static bool s_unavailable;
static int s_expr;   /* 0 normal, 1 puzzled (managed by blink_tick) */

static void hop_exec(void *obj, int32_t v)
{
    lv_obj_set_style_translate_y(obj, v, 0);
}

static void cat_hop(void)
{
    if (lv_anim_get(cat_grp, hop_exec)) {
        return;
    }
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, cat_grp);
    lv_anim_set_exec_cb(&a, hop_exec);
    lv_anim_set_values(&a, 0, -252 / 8);
    lv_anim_set_duration(&a, 170);
    lv_anim_set_reverse_duration(&a, 230);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
    s_blink_until = lv_tick_get() + 220;
}

static void on_cat_tap(lv_event_t *e)
{
    (void)e;
    cat_hop();
}

/* Expression overlays are exclusive; the blink timer owns them. */
static void expr_show(int which)
{
    /* which: -1 none, WB_C2_BLINK / WB_C2_HAPPY / WB_C2_PUZZLED */
    const int ids[3] = {WB_C2_BLINK, WB_C2_HAPPY, WB_C2_PUZZLED};
    for (int i = 0; i < 3; i++) {
        if (ids[i] == which) {
            show_obj(cat_layer[ids[i]]);
        } else {
            hide(cat_layer[ids[i]]);
        }
    }
}

static void blink_tick(lv_timer_t *t)
{
    (void)t;
    uint32_t now = lv_tick_get();
    if (s_done_hold) {
        /* Every tile checked: the happy cat and her sparkles stay up for as
         * long as the state lasts, so the payoff reads at any glance (and
         * in a still frame), not just for 1.5 s after the last tap. */
        expr_show(WB_C2_HAPPY);
        for (int i = 0; i < 3; i++) {
            lv_obj_set_style_opa(sparkle[i], LV_OPA_COVER, 0);
            show_obj(sparkle[i]);
        }
        return;
    }
    if (s_celebrating && (int32_t)(now - s_happy_until) < 0) {
        expr_show(WB_C2_HAPPY);
        return;
    }
    if (s_expr == 1) {
        expr_show(WB_C2_PUZZLED);
        return;
    }
#ifndef WB_SIM
    /* No blinking in the desktop renderer: a 150 ms blink window landing
     * on a captured frame reads as a celebration spent early. */
    if (s_blink_until && (int32_t)(now - s_blink_until) >= 0) {
        s_blink_until = 0;
        s_blink_at = now + (lv_rand(0, 4) == 0 ? 260 : lv_rand(2500, 6000));
        expr_show(-1);
    } else if (!s_blink_until && (int32_t)(now - s_blink_at) >= 0) {
        s_blink_until = now + 150;
        expr_show(WB_C2_BLINK);
    }
#endif
    if (s_celebrating && (int32_t)(now - s_happy_until) >= 0) {
        for (int i = 0; i < 3; i++) {
            hide(sparkle[i]);
        }
    }
}

static void build_cat(lv_obj_t *page)
{
    cat_stage = box(page);
    lv_obj_set_pos(cat_stage, 24, 100);
    lv_obj_set_size(cat_stage, 260, 252);
    lv_obj_add_flag(cat_stage, LV_OBJ_FLAG_OVERFLOW_VISIBLE);   /* the unavailable state scales the cat up */

    cat_grp = box(cat_stage);
    lv_obj_set_size(cat_grp, 260, 252);
    lv_obj_add_flag(cat_grp, LV_OBJ_FLAG_OVERFLOW_VISIBLE);   /* props sit at the stage corners */
    for (int i = 0; i < WB_C2_COUNT; i++) {
        cat_layer[i] = lv_image_create(cat_grp);
        hide(cat_layer[i]);
    }
    for (int i = 0; i < 9; i++) {
        cat_flame[i] = lv_image_create(cat_grp);
        hide(cat_flame[i]);
    }
    for (int i = 0; i < 3; i++) {
        sparkle[i] = lv_image_create(cat_grp);
        lv_image_set_src(sparkle[i], &img_x_sparkle);
        lv_image_set_scale(sparkle[i], 85);   /* 48 px art -> about 16 px */
        static const int16_t spos[3][2] = {{30, 16}, {196, 8}, {128, 52}};
        lv_obj_set_pos(sparkle[i], spos[i][0], spos[i][1]);
        hide(sparkle[i]);
    }
    /* Tap the cat and it hops. Long press still bubbles up to open Settings. */
    lv_obj_add_flag(cat_layer[WB_C2_BASE], LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_event_cb(cat_layer[WB_C2_BASE], on_cat_tap, LV_EVENT_CLICKED, NULL);

    s_blink_at = lv_tick_get() + 3000;
    lv_timer_create(blink_tick, 100, NULL);
}

/* ------------------------------------------------------------------ garment tiles */

static void check_scale_exec(void *obj, int32_t v)
{
    lv_obj_set_style_transform_scale(obj, v, 0);
}

static void set_tile_check_visual(int i, bool checked)
{
    if (checked) {
        lv_obj_set_style_bg_color(tile[i], C(COL_OK_BG), 0);
        lv_obj_set_style_border_color(tile[i], C(COL_OK_LINE), 0);
        lv_obj_set_style_border_width(tile[i], 3, 0);
        lv_obj_set_style_bg_color(tile_check[i], C(COL_OK), 0);
        lv_obj_set_style_bg_opa(tile_check[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(tile_check[i], C(COL_OK), 0);
        show_obj(tile_check_mark[i]);
    } else {
        lv_obj_set_style_bg_color(tile[i], C(COL_TILE), 0);
        /* The extra-layer tile keeps its amber frame so a pre-reader can
         * tell "one more thing" apart from the base clothes at a glance. */
        lv_obj_set_style_border_color(tile[i], s_tile_layer[i] ? C(COL_LAYER_LINE)
                                          : C(s_page == 1 ? COL_TILE_LINE_E : COL_TILE_LINE_M), 0);
        lv_obj_set_style_border_width(tile[i], 3, 0);
        /* A white filled disc, not a bare ring: the ring alone read as a
         * decoration clipped by the tile corner, not as a tap target. */
        lv_obj_set_style_bg_color(tile_check[i], lv_color_white(), 0);
        lv_obj_set_style_bg_opa(tile_check[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(tile_check[i], C(COL_RING_OFF), 0);
        hide(tile_check_mark[i]);
    }
}

static void on_tile(lv_event_t *e)
{
    intptr_t i = (intptr_t)lv_event_get_user_data(e);
    s_checks ^= (uint8_t)(1u << i);
    bool checked = s_checks & (1u << i);
    set_tile_check_visual((int)i, checked);
    /* Pop the check circle. */
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, tile_check[i]);
    lv_anim_set_exec_cb(&a, check_scale_exec);
    lv_anim_set_values(&a, 153, 256);
    lv_anim_set_duration(&a, 150);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
    s_force = true;
    render();   /* title / advice / all-done state follow the checks */
}

static void build_tiles(lv_obj_t *page)
{
    for (int i = 0; i < 4; i++) {
        lv_obj_t *t = box(page);
        tile[i] = t;
        lv_obj_set_size(t, 238, 116);
        lv_obj_set_style_radius(t, 22, 0);
        lv_obj_set_style_bg_opa(t, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(t, C(COL_TILE), 0);
        lv_obj_set_style_border_width(t, 3, 0);
        lv_obj_set_style_border_color(t, C(COL_TILE_LINE_M), 0);
        lv_obj_add_flag(t, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(t, on_tile, LV_EVENT_CLICKED, (void *)(intptr_t)i);

        tile_icon[i] = lv_image_create(t);
        lv_obj_set_pos(tile_icon[i], 10, 10);

        tile_label[i] = label(t, &font_label, "");
        lv_obj_set_style_text_color(tile_label[i], C(COL_INK), 0);
        lv_obj_set_style_text_letter_space(tile_label[i], -2, 0);   /* "Sneakers" is 122 px wide; the box is 122 */
        lv_obj_set_pos(tile_label[i], 116, 0);
        lv_label_set_long_mode(tile_label[i], LV_LABEL_LONG_MODE_WRAP);

        tile_check[i] = box(t);
        lv_obj_set_size(tile_check[i], 36, 36);
        lv_obj_set_style_radius(tile_check[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(tile_check[i], 3, 0);
        lv_obj_set_style_border_color(tile_check[i], C(COL_RING_OFF), 0);
        lv_obj_set_style_bg_color(tile_check[i], lv_color_white(), 0);
        lv_obj_set_style_bg_opa(tile_check[i], LV_OPA_COVER, 0);

        /* Star badge marking the extra-layer tile (set visible in render). */
        tile_badge[i] = lv_image_create(t);
        lv_image_set_src(tile_badge[i], &img_x_sparkle);
        lv_image_set_scale(tile_badge[i], 128);   /* 48 px art -> 24 px badge */
        lv_obj_set_pos(tile_badge[i], 8, 8);
        hide(tile_badge[i]);
        lv_obj_set_style_transform_pivot_x(tile_check[i], 18, 0);
        lv_obj_set_style_transform_pivot_y(tile_check[i], 18, 0);

        tile_check_mark[i] = label(tile_check[i], &font_chip, LV_SYMBOL_OK);
        lv_obj_set_style_text_color(tile_check_mark[i], lv_color_white(), 0);
        lv_obj_center(tile_check_mark[i]);
        hide(tile_check_mark[i]);
    }
}

/* ------------------------------------------------------------------ bottom bar */

static void style_tabs(void)
{
    static const char *names[3] = {"Today", "Tomorrow", "Next days"};
    for (int i = 0; i < 3; i++) {
        bool on = s_page == i;
        lv_obj_set_style_bg_color(tab_btn[i], on ? C(COL_INK) : lv_color_white(), 0);
        lv_obj_set_style_bg_opa(tab_btn[i], on ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_color(tab_btn[i], on ? C(COL_INK) : C(COL_TAB_LINE), 0);
        lv_obj_set_style_text_color(tab_lbl[i], on ? lv_color_white() : C(COL_INK), 0);
        (void)names;
    }
}

static void on_tab(lv_event_t *e)
{
    intptr_t p = (intptr_t)lv_event_get_user_data(e);
    set_page((int)p);
}

static void build_tabs(lv_obj_t *scr)
{
    lv_obj_t *row = box(scr);
    lv_obj_set_pos(row, 28, 424);
    lv_obj_set_size(row, LV_SIZE_CONTENT, 50);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row, 10, 0);
    static const char *names[3] = {"Today", "Tomorrow", "Next days"};
    for (int i = 0; i < 3; i++) {
        lv_obj_t *b = lv_button_create(row);
        lv_obj_remove_style_all(b);
        lv_obj_set_height(b, 50);
        lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(b, 2, 0);
        lv_obj_set_style_pad_hor(b, 22, 0);
        lv_obj_set_ext_click_area(b, 6);
        tab_btn[i] = b;
        tab_lbl[i] = label(b, &font_chip, names[i]);
        lv_obj_center(tab_lbl[i]);
        lv_obj_add_event_cb(b, on_tab, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }

    stale_chip = box(scr);
    lv_obj_set_style_radius(stale_chip, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(stale_chip, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(stale_chip, C(COL_WARN_BG), 0);
    lv_obj_set_style_pad_hor(stale_chip, 16, 0);
    lv_obj_set_height(stale_chip, 40);
    lbl_stale = label(stale_chip, &font_small, "");
    lv_obj_set_style_text_color(lbl_stale, C(COL_WARN_TX), 0);
    lv_obj_center(lbl_stale);
    lv_obj_align(stale_chip, LV_ALIGN_TOP_RIGHT, -28, 428);
    hide(stale_chip);

    /* "2 of 4 ready" progress pill: same slot as the stale chip, shown when
     * the data is fresh and some (not all) tiles are checked. */
    prog_chip = box(scr);
    lv_obj_set_style_radius(prog_chip, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(prog_chip, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(prog_chip, C(COL_OK_BG), 0);
    lv_obj_set_style_pad_hor(prog_chip, 16, 0);
    lv_obj_set_height(prog_chip, 40);
    lbl_prog = label(prog_chip, &font_small, "");
    lv_obj_set_style_text_color(lbl_prog, C(COL_INK), 0);
    lv_obj_center(lbl_prog);
    lv_obj_align(prog_chip, LV_ALIGN_TOP_RIGHT, -28, 428);
    hide(prog_chip);
}

/* ------------------------------------------------------------------ outfit page build */

static void build_outfit_page(lv_obj_t *scr)
{
    page_outfit = box(scr);
    lv_obj_set_size(page_outfit, UI_W, UI_H);
    lv_obj_add_event_cb(page_outfit, on_long_press, LV_EVENT_LONG_PRESSED, NULL);

    lbl_title = label(page_outfit, &font_h1, "");
    lv_obj_set_style_text_color(lbl_title, C(COL_INK), 0);
    lv_obj_set_width(lbl_title, 470);
    lv_label_set_long_mode(lbl_title, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_pos(lbl_title, 28, 18);

    chip = box(page_outfit);
    lv_obj_set_style_radius(chip, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(chip, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_hor(chip, 16, 0);
    lv_obj_set_height(chip, 36);
    lv_obj_set_pos(chip, 28, 70);
    lbl_chip = label(chip, &font_chip, "");
    lv_obj_center(lbl_chip);

    /* Weather block: icon left of two lines, the block right-aligned to 772. */
    wx_icon = lv_image_create(page_outfit);
    lv_obj_set_pos(wx_icon, 486, 18);
    lbl_wx1 = label(page_outfit, &font_label, "");
    lv_obj_set_style_text_color(lbl_wx1, C(COL_INK), 0);
    lv_obj_set_width(lbl_wx1, 206);
    lv_obj_set_style_text_align(lbl_wx1, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(lbl_wx1, 566, 20);
    lbl_wx2 = label(page_outfit, &font_small, "");
    lv_obj_set_style_text_color(lbl_wx2, C(COL_INK2), 0);
    lv_obj_set_width(lbl_wx2, 206);
    lv_obj_set_style_text_align(lbl_wx2, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(lbl_wx2, 566, 58);

    build_cat(page_outfit);
    build_tiles(page_outfit);

    lbl_no_data = label(page_outfit, &font_label, "I can't check the weather right now.");
    lv_obj_set_style_text_color(lbl_no_data, C(COL_INK), 0);
    lv_obj_set_width(lbl_no_data, 488);
    lv_label_set_long_mode(lbl_no_data, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_pos(lbl_no_data, 284, 196);
    lbl_no_data2 = label(page_outfit, &font_body, "Look outside, or ask a grown-up.");
    lv_obj_set_style_text_color(lbl_no_data2, C(COL_INK2), 0);
    lv_obj_set_width(lbl_no_data2, 488);
    lv_label_set_long_mode(lbl_no_data2, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_pos(lbl_no_data2, 300, 284);   /* indented past the scaled-up cat's tail */
    hide(lbl_no_data);
    hide(lbl_no_data2);

    adv_icon = lv_image_create(page_outfit);
    lv_obj_set_pos(adv_icon, 28, 358);
    lbl_advice = label(page_outfit, &font_body, "");
    lv_obj_set_style_text_color(lbl_advice, C(COL_INK), 0);
    lv_label_set_long_mode(lbl_advice, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_pos(lbl_advice, 90, 358);
    lv_obj_set_width(lbl_advice, 682);
}

/* ------------------------------------------------------------------ forecast page build */

static void build_forecast_page(lv_obj_t *scr)
{
    page_forecast = box(scr);
    lv_obj_set_size(page_forecast, UI_W, UI_H);
    lv_obj_add_event_cb(page_forecast, on_long_press, LV_EVENT_LONG_PRESSED, NULL);

    lv_obj_t *t = label(page_forecast, &font_h1, "Next few days");
    lv_obj_set_style_text_color(t, C(COL_INK), 0);
    lv_obj_set_pos(t, 28, 18);

    for (int i = 0; i < 3; i++) {
        lv_obj_t *c = box(page_forecast);
        fc_col[i] = c;
        lv_obj_set_pos(c, 28 + i * 252, 76);
        lv_obj_set_size(c, 236, 344);
        lv_obj_set_style_radius(c, 22, 0);
        lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(c, C(COL_TILE), 0);
        lv_obj_set_style_border_width(c, 3, 0);
        lv_obj_set_style_border_color(c, C(COL_TILE_LINE_M), 0);

        fc_name[i] = label(c, &font_label, "");
        lv_obj_set_style_text_color(fc_name[i], C(COL_INK), 0);
        lv_obj_align(fc_name[i], LV_ALIGN_TOP_MID, 0, 8);
        fc_date[i] = label(c, &font_small, "");
        lv_obj_set_style_text_color(fc_date[i], C(COL_INK2), 0);
        lv_obj_align(fc_date[i], LV_ALIGN_TOP_MID, 0, 44);
        fc_img[i] = lv_image_create(c);
        lv_obj_align(fc_img[i], LV_ALIGN_TOP_MID, 0, 70);
        fc_hi[i] = label(c, &font_label, "");
        lv_obj_set_style_text_color(fc_hi[i], C(COL_INK), 0);
        lv_obj_align(fc_hi[i], LV_ALIGN_TOP_MID, 0, 186);
        fc_lo[i] = label(c, &font_body, "");
        lv_obj_set_style_text_color(fc_lo[i], C(COL_INK2), 0);
        lv_obj_align(fc_lo[i], LV_ALIGN_TOP_MID, 0, 224);

        /* Fixed slots, identical in every column: the rain row keeps its
         * slot whether or not it rains, and the garment row is one pinned
         * component at the same y in all three columns. */
        fc_rain[i] = box(c);
        lv_obj_set_size(fc_rain[i], LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(fc_rain[i], LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(fc_rain[i], LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(fc_rain[i], 6, 0);
        lv_obj_align(fc_rain[i], LV_ALIGN_TOP_MID, 0, 258);
        fc_drop[i] = lv_image_create(fc_rain[i]);
        lv_image_set_src(fc_drop[i], &img_drop);
        lv_image_set_scale(fc_drop[i], 256);   /* native 22 px drop */
        fc_pop[i] = label(fc_rain[i], &font_small, "");
        lv_obj_set_style_text_color(fc_pop[i], C(COL_INK), 0);

        fc_garment[i] = box(c);
        lv_obj_set_size(fc_garment[i], LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(fc_garment[i], LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(fc_garment[i], LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(fc_garment[i], 8, 0);
        lv_obj_align(fc_garment[i], LV_ALIGN_BOTTOM_MID, 0, -6);
        fc_gimg[i] = lv_image_create(fc_garment[i]);
        /* garment icon at its full 56 px (it used to be scaled to ~32) */
        fc_glabel[i] = label(fc_garment[i], &font_small, "");
        lv_obj_set_style_text_color(fc_glabel[i], C(COL_INK), 0);
    }
    hide(page_forecast);
}

static void build_main(void)
{
    scr_main = lv_obj_create(NULL);
    lv_obj_remove_flag(scr_main, LV_OBJ_FLAG_SCROLLABLE);
    solid_bg(scr_main, COL_BG_MORNING);
    build_outfit_page(scr_main);
    build_forecast_page(scr_main);
    build_tabs(scr_main);
}

/* ------------------------------------------------------------------ render helpers */

static void civil_from_days(long z, int *y, int *m, int *d)
{
    z += 719468;
    long era = (z >= 0 ? z : z - 146096) / 146097;
    unsigned doe = (unsigned)(z - era * 146097);
    unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    long yy = (long)yoe + era * 400;
    unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    unsigned mp = (5 * doy + 2) / 153;
    *d = (int)(doy - (153 * mp + 2) / 5 + 1);
    *m = (int)(mp < 10 ? mp + 3 : mp - 9);
    *y = (int)(yy + (*m <= 2));
}

static bool clock_now(struct tm *out, bool *real)
{
    if (s_time_cb && s_time_cb(out)) {
        *real = true;
        return true;
    }
    if (s_have_wx) {
        memset(out, 0, sizeof(*out));
        out->tm_year = s_wx.year - 1900;
        out->tm_mon = s_wx.month - 1;
        out->tm_mday = s_wx.day;
        out->tm_hour = s_wx.hour;
        out->tm_min = s_wx.minute;
        out->tm_wday = s_wx.wday;
        *real = false;
        return true;
    }
    return false;
}

/* Which cat set to show: the Settings choice, with "take turns" alternating
 * by day of year (black on even days). */
static const wb_cat2_t *cat_set(const cat_season_t *seas)
{
    int choice = s_cat_choice;
    if (choice == 2) {
        choice = (seas->doy % 2 == 0) ? 0 : 1;
    }
    return choice == 1 ? &wb_cat2_pink : &wb_cat2_black;
}

static void cat2_apply(const outfit_t *o, const cat_season_t *seas, bool unavailable)
{
    const wb_cat2_t *set = cat_set(seas);
    bool vis[WB_C2_COUNT];
    for (int i = 0; i < WB_C2_COUNT; i++) {
        vis[i] = false;
    }
    vis[WB_C2_BASE] = true;
    if (o) {
        switch (o->bottom) {
        case BOTTOM_SHORTS: vis[WB_C2_BOTTOM_SHORTS] = true; break;
        case BOTTOM_LONG_PANTS: vis[WB_C2_BOTTOM_PANTS] = true; break;
        }
        switch (o->feet) {
        case FEET_SNEAKERS: vis[WB_C2_FEET_SNEAKERS] = true; break;
        case FEET_RAIN_BOOTS: vis[WB_C2_FEET_RAIN] = true; break;
        case FEET_SNOW_BOOTS: vis[WB_C2_FEET_SNOW] = true; break;
        }
        switch (o->top) {
        case TOP_SHORT_SLEEVES: vis[WB_C2_TOP_SHORT] = true; break;
        case TOP_LONG_SLEEVES: vis[WB_C2_TOP_LONG] = true; break;
        }
        switch (o->layer) {
        case LAYER_HOODIE: vis[WB_C2_LAYER_HOODIE] = true; break;
        case LAYER_LIGHT_JACKET: vis[WB_C2_LAYER_LIGHT] = true; break;
        case LAYER_RAIN_JACKET: vis[WB_C2_LAYER_RAIN] = true; break;
        case LAYER_WARM_COAT: vis[WB_C2_LAYER_WARM] = true; break;
        case LAYER_WINTER_COAT: vis[WB_C2_LAYER_WINTER] = true; break;
        default: break;
        }
        if (o->extras & EX_SCARF) {
            vis[WB_C2_SCARF] = true;
        }
        if (o->extras & EX_GLOVES) {
            vis[WB_C2_GLOVES] = true;
        }
        if (o->extras & EX_UMBRELLA) {
            vis[WB_C2_HELD_UMBRELLA] = true;
        } else if (o->extras & EX_WATER) {
            vis[WB_C2_HELD_WATER] = true;
        }
        if (o->extras & EX_SUNGLASSES) {
            vis[WB_C2_FACE_SUN] = true;
        }
        if (o->extras & EX_HAT) {
            vis[WB_C2_HAT_WARM] = true;
        }
    }
    if (!vis[WB_C2_HAT_WARM]) {   /* the prescription wins the hat slot */
        if (seas->hat == CAT_SEASON_HAT_WITCH) {
            vis[WB_C2_HAT_WITCH] = true;
        } else if (seas->hat == CAT_SEASON_HAT_SANTA) {
            vis[WB_C2_HAT_SANTA] = true;
        }
    }
    if (seas->prop == CAT_SEASON_PROP_PUMPKIN) {
        vis[WB_C2_PROP_PUMPKIN] = true;
    } else if (seas->prop == CAT_SEASON_PROP_PRESENT) {
        vis[WB_C2_PROP_PRESENT] = true;
    } else if (seas->prop == CAT_SEASON_PROP_MENORAH) {
        vis[WB_C2_PROP_MENORAH] = true;
    }
    /* Unavailable state: a bigger puzzled cat fills the stage instead of a
     * small cat next to a void. The transform scales the whole layer group
     * (positions included), so the menorah flames stay on their candles.
     * The stage itself grows for this state: a transformed child layer is
     * still clipped to its parent's box, flags or not, so the parent box
     * has to contain the scaled cat. */
    if (unavailable) {
        lv_obj_set_pos(cat_stage, 0, 64);
        lv_obj_set_size(cat_stage, 560, 400);
        lv_obj_set_pos(cat_grp, 24, 36);
        lv_obj_set_style_transform_pivot_x(cat_grp, 130, 0);
        lv_obj_set_style_transform_pivot_y(cat_grp, 205, 0);
        lv_obj_set_style_transform_scale(cat_grp, 300, 0);
    } else {
        lv_obj_set_pos(cat_stage, 24, 100);
        lv_obj_set_size(cat_stage, 260, 252);
        lv_obj_set_pos(cat_grp, 0, 0);
        lv_obj_set_style_transform_scale(cat_grp, 256, 0);
    }
    for (int i = 0; i < WB_C2_COUNT; i++) {
        if (i == WB_C2_BLINK || i == WB_C2_HAPPY || i == WB_C2_PUZZLED) {
            /* The blink timer owns their visibility, but they still need a
             * source and a position. */
            if (set->img[i]) {
                lv_image_set_src(cat_layer[i], set->img[i]);
                lv_obj_set_pos(cat_layer[i], set->x[i], set->y[i]);
            }
            continue;
        }
        if (set->img[i]) {
            lv_image_set_src(cat_layer[i], set->img[i]);
            lv_obj_set_pos(cat_layer[i], set->x[i], set->y[i]);
        }
        if (vis[i] && set->img[i]) {
            show_obj(cat_layer[i]);
        } else {
            hide(cat_layer[i]);
        }
    }
    /* Menorah flames: the shamash (index 4) plus the first N of the old
     * left-to-right lighting order, positioned by the set's flame table. */
    for (int i = 0; i < 9; i++) {
        hide(cat_flame[i]);
    }
    if (vis[WB_C2_PROP_MENORAH] && seas->candles > 0) {
        static const int order[8] = {8, 7, 6, 5, 3, 2, 1, 0};
        int lit[9] = {0};
        lit[4] = 1;
        for (int k = 0; k < seas->candles - 1 && k < 8; k++) {
            lit[order[k]] = 1;
        }
        for (int i = 0; i < 9; i++) {
            if (lit[i]) {
                lv_image_set_src(cat_flame[i], &img_cat2_flame);
                lv_obj_set_pos(cat_flame[i], WB_CAT2_MENORAH_FLAMES[i].x, WB_CAT2_MENORAH_FLAMES[i].y);
                show_obj(cat_flame[i]);
            }
        }
    }
}

static void sparkle_opa_exec(void *obj, int32_t v)
{
    lv_obj_set_style_opa(obj, (lv_opa_t)v, 0);
}

static void celebrate(void)
{
    s_celebrating = true;
    s_happy_until = lv_tick_get() + 1500;
    cat_hop();
    static const int16_t pos[3][2] = {{30, 16}, {196, 8}, {128, 52}};
    for (int i = 0; i < 3; i++) {
        lv_obj_set_pos(sparkle[i], pos[i][0], pos[i][1]);
        lv_obj_set_style_opa(sparkle[i], LV_OPA_COVER, 0);
        show_obj(sparkle[i]);
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, sparkle[i]);
        lv_anim_set_exec_cb(&a, sparkle_opa_exec);
        lv_anim_set_values(&a, 255, 0);
        lv_anim_set_duration(&a, 600);
        lv_anim_set_delay(&a, (uint32_t)(i * 120));
        lv_anim_start(&a);
    }
}

/* ------------------------------------------------------------------ render */

static bool s_built;

/* Tile labels are always font_label (plan section 2). At the label
 * width (tile w - 116) every garment word fits on one line: the
 * longest, "sleeves", measures ~105 px against 122 px. */

static void render_forecast(const wx_t *wx, const struct tm *now)
{
    outfit_opts_t opts = s_opts;   /* same engine configuration as the outfit pages */
    for (int i = 0; i < 3; i++) {
        outfit_t o;
        bool ok = outfit_make(wx, now, i + 1, &opts, &o) && o.ok;
        if (!ok) {
            hide(fc_col[i]);
            continue;
        }
        show_obj(fc_col[i]);
        char buf[64];
        if (i == 0) {
            lv_label_set_text(fc_name[i], "Tomorrow");
        } else {
            lv_label_set_text(fc_name[i], wx_wday_name(o.wday, false));
        }
        static const char *mon[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                      "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
        snprintf(buf, sizeof(buf), "%s %d", mon[o.month - 1], o.day);
        lv_label_set_text(fc_date[i], buf);
        lv_image_set_src(fc_img[i], ICON_M[wx_icon_for(o.wx_code, true)]);
        snprintf(buf, sizeof(buf), "High %d\xC2\xB0", iround(o.hi));
        lv_label_set_text(fc_hi[i], buf);
        snprintf(buf, sizeof(buf), "Low %d\xC2\xB0", iround(o.lo));
        lv_label_set_text(fc_lo[i], buf);
        if (o.pop_day >= 30) {
            show_obj(fc_rain[i]);
            snprintf(buf, sizeof(buf), "Rain %d%%", o.pop_day);
            lv_label_set_text(fc_pop[i], buf);
        } else {
            hide(fc_rain[i]);
        }
        int g = o.layer != LAYER_NONE ? layer_garment(o.layer) : top_garment(o.top);
        if (g >= 0 && GARMENT_56[g]) {
            show_obj(fc_garment[i]);
            lv_image_set_src(fc_gimg[i], GARMENT_56[g]);
            lv_label_set_text(fc_glabel[i], garment_label(g));
        } else {
            hide(fc_garment[i]);
        }
    }
}

static void render(void)
{
    if (!s_built) {
        return;
    }
    struct tm now;
    bool real;
    if (!clock_now(&now, &real)) {
        return;
    }
    if (s_page_touched && lv_tick_elaps(s_last_touch) > 180000) {
        s_page_touched = false;   /* 3 idle minutes: fall back to the time of day */
    }
    if (!s_page_touched) {
        s_page = now.tm_hour < 17 ? 0 : 1;
    }
    int page = s_page;

    long now_min = minutes_of(now.tm_year + 1900, now.tm_mon + 1, now.tm_mday, now.tm_hour, now.tm_min);
    long wx_min = s_have_wx ? minutes_of(s_wx.year, s_wx.month, s_wx.day, s_wx.hour, s_wx.minute) : 0;
    long age = now_min - wx_min;
    if (age < 0) {
        age = 0;
    }
    /* 0 fresh, 1 stale chip, 2 unavailable (plan 4.6) */
    int bucket = !s_have_wx ? 2 : (real && age >= 720) ? 2 : (real && age >= 90) ? 1 : 0;

    char key[96];
    snprintf(key, sizeof(key), "%d|%d|%ld|%d|%d|%d", page, bucket,
             days_from_civil(now.tm_year + 1900, now.tm_mon + 1, now.tm_mday) + (page < 2 ? page : 0),
             now.tm_hour, (int)s_checks, s_cat_choice);
    if (!s_force && strcmp(key, s_key) == 0) {
        return;
    }
    snprintf(s_key, sizeof(s_key), "%s", key);
    s_force = false;

    bool evening = page == 1;
    solid_bg(scr_main, page == 2 ? COL_BG_FORECAST : evening ? COL_BG_EVENING : COL_BG_MORNING);
    if (page < 2) {
        show_obj(page_outfit);
        hide(page_forecast);
    } else {
        hide(page_outfit);
        show_obj(page_forecast);
    }
    style_tabs();

    /* Stale / offline chip (plan 4.6). */
    if (bucket == 1) {
        char tbuf[24], buf[64];
        fmt_time12(s_wx.hour, s_wx.minute, tbuf, sizeof(tbuf));
        if (s_wx.day != now.tm_mday || s_wx.month != now.tm_mon + 1) {
            snprintf(buf, sizeof(buf), "Weather from %s %s", wx_wday_name(s_wx.wday, true), tbuf);
        } else {
            snprintf(buf, sizeof(buf), "Weather from %s", tbuf);
        }
        lv_label_set_text(lbl_stale, buf);
        lv_obj_set_size(stale_chip, text_w(buf, &font_small) + 32, 40);
        lv_obj_align(stale_chip, LV_ALIGN_TOP_RIGHT, -28, 428);
        lv_obj_set_width(lbl_stale, LV_SIZE_CONTENT);
        lv_obj_center(lbl_stale);
        show_obj(stale_chip);
    } else if (bucket == 2) {
        if (s_have_wx) {
            char tbuf[24], buf[64];
            fmt_time12(s_wx.hour, s_wx.minute, tbuf, sizeof(tbuf));
            snprintf(buf, sizeof(buf), "Offline since %s %s", wx_wday_name(s_wx.wday, true), tbuf);
            lv_label_set_text(lbl_stale, buf);
        } else {
            lv_label_set_text(lbl_stale, "No weather yet");
        }
        lv_obj_set_size(stale_chip, text_w(lv_label_get_text(lbl_stale), &font_small) + 32, 40);
        lv_obj_align(stale_chip, LV_ALIGN_TOP_RIGHT, -28, 428);
        lv_obj_set_width(lbl_stale, LV_SIZE_CONTENT);
        lv_obj_center(lbl_stale);
        show_obj(stale_chip);
    } else {
        hide(stale_chip);
    }

    if (page == 2) {
        if (s_have_wx) {
            render_forecast(&s_wx, &now);
        } else {
            for (int i = 0; i < 3; i++) {
                hide(fc_col[i]);
            }
        }
        return;
    }

    outfit_t o;
    outfit_opts_t opts = s_opts;
    bool avail = false;
    if (s_have_wx && bucket < 2) {
        avail = outfit_make(&s_wx, &now, page, &opts, &o) && o.ok;
    }

    if (avail) {
        long od = days_from_civil(o.year, o.month, o.day);
        if (od != s_check_date) {
            if (s_check_date != -2) {
                s_checks = 0;   /* a new outfit day starts unchecked */
            }
            s_check_date = od;
        }
    }
    int mask = avail ? (o.ntiles >= 4 ? 0xF : (1 << o.ntiles) - 1) : 0;
    bool all_done = avail && o.ntiles > 0 && (s_checks & mask) == mask;
    if (all_done && !s_celebrating) {
        celebrate();
    } else if (!all_done) {
        s_celebrating = false;
    }
    s_done_hold = all_done;

    /* Title */
    const char *title;
    if (all_done) {
        title = page == 0 ? "All dressed. Have a great day!" : "All ready for tomorrow!";
    } else if (page == 0) {
        title = now.tm_hour < 12 ? "Let's get dressed!" : "What to wear today";
    } else {
        title = "Ready for tomorrow?";
    }
    lv_label_set_text(lbl_title, title);
    lv_obj_set_style_text_font(lbl_title, text_w(title, &font_h1) > 470 ? &font_label : &font_h1, 0);

    /* Day chip */
    char cbuf[48];
    if (avail) {
        snprintf(cbuf, sizeof(cbuf), "%s \xC2\xB7 %s", page == 0 ? "Today" : "Tomorrow", wx_wday_name(o.wday, false));
    } else {
        int wy, wm, wd;
        civil_from_days(days_from_civil(now.tm_year + 1900, now.tm_mon + 1, now.tm_mday) + page, &wy, &wm, &wd);
        int wdow = wx_day_of_week(wy, wm, wd);
        snprintf(cbuf, sizeof(cbuf), "%s \xC2\xB7 %s", page == 0 ? "Today" : "Tomorrow", wx_wday_name(wdow, false));
        (void)wy; (void)wm; (void)wd;
    }
    lv_label_set_text(lbl_chip, cbuf);
    lv_obj_set_size(chip, text_w(cbuf, &font_chip) + 32, 36);   /* 16 px padding each side */
    lv_obj_set_width(lbl_chip, LV_SIZE_CONTENT);
    lv_obj_center(lbl_chip);
    lv_obj_set_style_bg_color(chip, evening ? C(COL_CHIP_E_BG) : C(COL_CHIP_M_BG), 0);
    lv_obj_set_style_text_color(lbl_chip, evening ? C(COL_CHIP_E_TX) : C(COL_CHIP_M_TX), 0);
    lv_obj_update_layout(chip);

    /* Weather block */
    if (avail) {
        show_obj(wx_icon);
        lv_image_set_src(wx_icon, ICON_S[wx_icon_for(o.wx_code, o.is_day)]);
        char b1[48], b2[48];
        if (page == 0) {
            snprintf(b1, sizeof(b1), "%d\xC2\xB0 now", iround(o.t_now));
            snprintf(b2, sizeof(b2), "High %d\xC2\xB0 \xC2\xB7 Low %d\xC2\xB0", iround(o.hi), iround(o.lo));
        } else {
            snprintf(b1, sizeof(b1), "High %d\xC2\xB0", iround(o.hi));
            if (o.pop_day >= 30) {
                snprintf(b2, sizeof(b2), "Low %d\xC2\xB0 \xC2\xB7 Rain %d%%", iround(o.lo), o.pop_day);
            } else {
                snprintf(b2, sizeof(b2), "Low %d\xC2\xB0", iround(o.lo));   /* line 1 already says the high */
            }
        }
        lv_label_set_text(lbl_wx1, b1);
        lv_label_set_text(lbl_wx2, b2);
        /* Right-align the whole block to x = 772: icon sits 8 px left of the text. */
        int tw = text_w(b1, &font_label);
        int tw2 = text_w(b2, &font_small);
        if (tw2 > tw) {
            tw = tw2;
        }
        lv_obj_set_x(lbl_wx1, 772 - tw);
        lv_obj_set_width(lbl_wx1, tw);
        lv_obj_set_x(lbl_wx2, 772 - tw);
        lv_obj_set_width(lbl_wx2, tw);
        lv_obj_set_x(wx_icon, 772 - tw - 80);
    } else {
        hide(wx_icon);
        lv_label_set_text(lbl_wx1, "--");
        lv_label_set_text(lbl_wx2, "");
    }

    /* Cat + season props for the outfit's date. */
    cat_season_t seas;
    if (avail) {
        cat_season(o.year, o.month, o.day, now.tm_hour, &seas);
    } else {
        int cy, cm, cd;
        civil_from_days(days_from_civil(now.tm_year + 1900, now.tm_mon + 1, now.tm_mday) + page, &cy, &cm, &cd);
        cat_season(cy, cm, cd, now.tm_hour, &seas);
    }
    cat2_apply(avail ? &o : NULL, &seas, !avail);
    s_expr = avail ? 0 : 1;
    /* Own the base expression here. The face overlays are otherwise only
     * touched by the blink timer, and on a page change a stale overlay
     * (the puzzled face from the offline page) would stay up forever:
     * the blink cycle that used to clear it does not run while the cat
     * is mid-blink or, in the desktop renderer, at all. */
    if (!s_done_hold && !s_celebrating) {
        expr_show(s_expr == 1 ? WB_C2_PUZZLED : -1);
    }

    /* Tiles or the no-data message. */
    if (avail) {
        hide(lbl_no_data);
        hide(lbl_no_data2);
        for (int i = 0; i < 4; i++) {
            if (i >= o.ntiles) {
                hide(tile[i]);
                continue;
            }
            show_obj(tile[i]);
            int x = i % 2 == 0 ? 284 : 534;
            int y = i < 2 ? 108 : 236;
            int w = (o.ntiles == 3 && i == 2) ? 488 : 238;
            lv_obj_set_pos(tile[i], x, y);
            lv_obj_set_size(tile[i], w, 116);
            lv_image_set_src(tile_icon[i], GARMENT_96[o.tile_kind[i]]);
            lv_label_set_text(tile_label[i], garment_label(o.tile_kind[i]));
            lv_obj_set_width(tile_label[i], w - 116);
            lv_obj_set_style_text_font(tile_label[i], &font_label, 0);   /* always the label font */
            s_tile_layer[i] = o.tile_kind[i] >= HOODIE && o.tile_kind[i] <= WINTER_COAT;
            if (s_tile_layer[i]) {
                show_obj(tile_badge[i]);
            } else {
                hide(tile_badge[i]);
            }
            lv_obj_set_pos(tile_check[i], w - 48, 12);
            lv_obj_update_layout(tile[i]);
            int th = lv_obj_get_height(tile_label[i]);
            lv_obj_set_y(tile_label[i], (116 - th) / 2);
            set_tile_check_visual(i, (s_checks >> i) & 1);
        }
    } else {
        for (int i = 0; i < 4; i++) {
            hide(tile[i]);
        }
        show_obj(lbl_no_data);
        show_obj(lbl_no_data2);
    }

    /* Progress pill (fresh data only; the stale chip owns the slot
     * otherwise). "2 of 4 ready" while a check-off is in progress. */
    if (avail && bucket == 0 && !all_done) {
        int checked_n = 0;
        for (int i = 0; i < o.ntiles; i++) {
            if ((s_checks >> i) & 1) {
                checked_n++;
            }
        }
        if (checked_n > 0) {
            char pbuf[32];
            snprintf(pbuf, sizeof(pbuf), "%d of %d ready", checked_n, o.ntiles);
            lv_label_set_text(lbl_prog, pbuf);
            lv_obj_set_size(prog_chip, text_w(pbuf, &font_small) + 32, 40);
            lv_obj_set_width(lbl_prog, LV_SIZE_CONTENT);
            lv_obj_center(lbl_prog);
            show_obj(prog_chip);
        } else {
            hide(prog_chip);
        }
    } else {
        hide(prog_chip);
    }

    /* Advice row. */
    if (!avail) {
        hide(adv_icon);
        hide(lbl_advice);
    } else if (all_done) {
        show_obj(lbl_advice);
        char abuf[96];
        const char *txt = "Nice job!";
        if (s_names[0]) {
            snprintf(abuf, sizeof(abuf), "Nice job, %s!", s_names);
            lv_point_t tr;
            lv_text_get_size(&tr, abuf, &font_body, 0, 0, 682, LV_TEXT_FLAG_NONE);
            if (tr.y <= 2 * lv_font_get_line_height(&font_body)) {
                txt = abuf;
            }
        }
        lv_label_set_text(lbl_advice, txt);
        lv_obj_set_style_text_color(lbl_advice, C(COL_ACCENT), 0);
        lv_obj_set_pos(lbl_advice, 90, 358);
        lv_obj_set_width(lbl_advice, 682);
        lv_image_set_src(adv_icon, &img_x_heart);
        show_obj(adv_icon);
    } else {
        show_obj(lbl_advice);
        lv_obj_set_style_text_color(lbl_advice, C(COL_INK), 0);
        bool has_icon = o.extra_icon >= 0 && o.extra_icon < 8 && EXTRA_IMG[o.extra_icon];
        /* Measure with the width the label will really get (682 with the
         * icon, 744 without) before deciding sentence A has to go. */
        int adv_w = has_icon ? 682 : 744;
        const char *txt = o.advice;
        lv_point_t tr;
        lv_text_get_size(&tr, txt, &font_body, 0, 0, adv_w, LV_TEXT_FLAG_NONE);
        if (tr.y > 2 * lv_font_get_line_height(&font_body)) {
            txt = o.advice_short;   /* too long for two lines: keep the change sentence */
        }
        lv_label_set_text(lbl_advice, txt);
        if (has_icon) {
            lv_image_set_src(adv_icon, EXTRA_IMG[o.extra_icon]);
            show_obj(adv_icon);
            lv_obj_set_pos(lbl_advice, 90, 358);
            lv_obj_set_width(lbl_advice, 682);
        } else {
            hide(adv_icon);
            lv_obj_set_pos(lbl_advice, 28, 358);
            lv_obj_set_width(lbl_advice, 744);
        }
    }
}

static void set_page(int p)
{
    if (p < 0) {
        p = 0;
    }
    if (p > 2) {
        p = 2;
    }
    s_page = p;
    s_page_touched = true;
    s_last_touch = lv_tick_get();
    s_force = true;
    render();
}

static void clock_tick(lv_timer_t *t)
{
    (void)t;
    render();
}

/* ------------------------------------------------------------------ public API */

void ui_init(const char *names)
{
    if (names && names[0]) {
        snprintf(s_names, sizeof(s_names), "%s", names);
    }
    init_asset_tables();
    build_boot();
    build_setup();
    build_main();
    build_settings();
    for (lv_indev_t *in = lv_indev_get_next(NULL); in; in = lv_indev_get_next(in)) {
        lv_indev_add_event_cb(in, on_indev, LV_EVENT_ALL, NULL);   /* swipes */
    }
    lv_sysmon_hide_performance(lv_display_get_default());   /* LVGL shows it by default */
    lv_timer_create(clock_tick, 1000, NULL);
    s_built = true;
    lv_screen_load(scr_boot);
}

void ui_set_names(const char *names)
{
    snprintf(s_names, sizeof(s_names), "%s", names ? names : "");
    s_force = true;
    render();
}

void ui_set_bike_bus_day(int wday)
{
    s_opts.bike_bus_wday = wday;
    s_force = true;
    render();
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

void ui_set_footer(const char *place, const char *status, bool warn)
{
    /* The 2.0 screens have no footer line; freshness lives in the stale chip.
     * Kept for the existing callers in net.c. */
    (void)place;
    (void)status;
    (void)warn;
}

void ui_show_page(int page)
{
    set_page(page);
}

void ui_refresh_clock(void)
{
    s_force = true;
    render();
}

void ui_show_weather(const wx_t *wx)
{
    if (!wx || !wx->valid) {
        if (!s_have_wx) {
            s_force = true;
            render();   /* shows the unavailable state once data was expected */
        }
        return;
    }
    s_wx = *wx;
    s_have_wx = true;
    if (lv_screen_active() != scr_main) {
        lv_screen_load(scr_main);
    }
    s_force = true;
    render();
}

#ifdef WB_SIM
void ui_test_set_checks(uint8_t mask)
{
    s_checks = mask;
    s_check_date = -2;   /* adopt the current outfit date without clearing */
    s_force = true;
    render();
}

void ui_test_set_page(int p)
{
    set_page(p);
}
#endif
