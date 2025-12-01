#include "visual.h"
#include "esp_lvgl_port.h"
#include "font_emoji.h"
#include "font_awesome.h"
#include "object.h"

typedef struct
{
    char *emotion;
    char *emoji;
} emotion_t;

LV_FONT_DECLARE(font_awesome_16_4);
LV_FONT_DECLARE(font_puhui_16_4);

static const emotion_t emotions[] = {
    {"neutral", "😶"},
    {"happy", "🙂"},
    {"laughing", "😆"},
    {"funny", "😂"},
    {"sad", "😔"},
    {"angry", "😠"},
    {"crying", "😭"},
    {"loving", "😍"},
    {"embarrassed", "😳"},
    {"surprised", "😯"},
    {"shocked", "😱"},
    {"thinking", "🤔"},
    {"winking", "😉"},
    {"cool", "😎"},
    {"relaxed", "😌"},
    {"delicious", "🤤"},
    {"kissy", "😘"},
    {"confident", "😏"},
    {"sleepy", "😴"},
    {"silly", "😜"},
    {"confused", "🙄"},
};

typedef struct
{
    const lv_font_t *icon_font;
    const lv_font_t *text_font;
    const lv_font_t *emoji_font;

    lv_color_t status_bar_bg_color;
    lv_color_t status_bar_text_color;
    lv_color_t content_bg_color;
    lv_color_t content_text_color;
} visual_theme_t;

struct visual
{
    visual_theme_t theme;
    lv_subject_t *emotion;
    lv_subject_t *text;

    lv_subject_t *state;
    lv_subject_t *battery;
    lv_subject_t *wifi;

    lv_obj_t *notification;
    lv_obj_t *qrcode;

    lv_timer_t *notification_timer;
};

static void visual_theme_init(visual_t *visual)
{
    visual->theme.icon_font = &font_awesome_16_4;
    visual->theme.text_font = &font_puhui_16_4;
    visual->theme.emoji_font = font_emoji_64_init();

    visual->theme.status_bar_bg_color = lv_palette_darken(LV_PALETTE_GREY, 3);
    visual->theme.status_bar_text_color = lv_color_white();

    visual->theme.content_bg_color = lv_color_white();
    visual->theme.content_text_color = lv_color_black();
}

static void visual_ui_init(visual_t *visual)
{
    lvgl_port_lock(1000);
    lv_obj_t *screen = lv_screen_active();
    lv_obj_t *status_bar = lv_obj_create(screen);
    lv_obj_t *container = lv_obj_create(screen);

    // 设置状态栏大小
    lv_obj_set_size(status_bar, LV_HOR_RES, LV_PCT(10));
    lv_obj_set_pos(status_bar, 0, 0);
    lv_obj_set_style_bg_color(status_bar, visual->theme.status_bar_bg_color, 0);
    lv_obj_remove_flag(status_bar, LV_OBJ_FLAG_SCROLLABLE);

    // 设置内容区域大小
    lv_obj_set_size(container, LV_HOR_RES, LV_PCT(90));
    lv_obj_set_pos(container, 0, LV_PCT(10));
    lv_obj_set_style_bg_color(container, visual->theme.content_bg_color, 0);

    lv_obj_t *battery_label = lv_label_create(status_bar);
    lv_obj_set_style_text_font(battery_label, visual->theme.icon_font, 0);
    lv_obj_set_style_text_color(battery_label, visual->theme.status_bar_text_color, 0);
    lv_label_bind_text(battery_label, visual->battery, "%s");
    lv_obj_align(battery_label, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *status_label = lv_label_create(status_bar);
    lv_obj_set_style_text_font(status_label, visual->theme.text_font, 0);
    lv_obj_set_style_text_color(status_label, visual->theme.status_bar_text_color, 0);
    lv_label_bind_text(status_label, visual->state, "%s");
    lv_obj_align(status_label, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *wifi_label = lv_label_create(status_bar);
    lv_obj_set_style_text_font(wifi_label, visual->theme.icon_font, 0);
    lv_obj_set_style_text_color(wifi_label, visual->theme.status_bar_text_color, 0);
    lv_label_bind_text(wifi_label, visual->wifi, "%s");
    lv_obj_align(wifi_label, LV_ALIGN_RIGHT_MID, 0, 0);

    // 设置内容区域
    lv_obj_t *emoji_label = lv_label_create(container);
    lv_obj_align(emoji_label, LV_ALIGN_CENTER, 0, -60);
    lv_obj_set_style_text_font(emoji_label, visual->theme.emoji_font, 0);
    lv_label_bind_text(emoji_label, visual->emotion, "%s");

    lv_obj_t *text_label = lv_label_create(container);
    lv_obj_align(text_label, LV_ALIGN_CENTER, 0, 30);
    lv_obj_set_style_text_font(text_label, visual->theme.text_font, 0);
    lv_obj_set_style_text_align(text_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(text_label, 200);
    lv_label_set_long_mode(text_label, LV_LABEL_LONG_WRAP);
    lv_label_bind_text(text_label, visual->text, "%s");
    lvgl_port_unlock();
}

visual_t *visual_create(void)
{
    visual_t *visual = (visual_t *)object_create(sizeof(visual_t));
    visual->emotion = object_create(sizeof(lv_subject_t));
    visual->text = object_create(sizeof(lv_subject_t));
    visual->state = object_create(sizeof(lv_subject_t));
    visual->battery = object_create(sizeof(lv_subject_t));
    visual->wifi = object_create(sizeof(lv_subject_t));

    char *text_buf = object_create(sizeof(char) * 301);
    lv_subject_init_pointer(visual->emotion, "😶");
    lv_subject_init_string(visual->text, text_buf, NULL, 64, "我是智能助手小智，请用“你好小智”唤醒我。");
    lv_subject_init_pointer(visual->state, "STARTING");
    lv_subject_init_pointer(visual->battery, FONT_AWESOME_BATTERY_FULL);
    lv_subject_init_pointer(visual->wifi, FONT_AWESOME_WIFI);

    // 初始化主题
    visual_theme_init(visual);

    // 创建视觉元素
    visual_ui_init(visual);

    return visual;
}

void visual_destroy(visual_t *visual)
{
    free(visual->battery);
    free(visual->wifi);
    free(visual->state);
    free(visual->emotion);
    free(visual->text);

    free(visual);

    lv_obj_t *screen = lv_screen_active();
    lv_obj_delete(lv_obj_get_child(screen, 0));
    lv_obj_delete(lv_obj_get_child(screen, 1));
}

void visual_set_state(visual_t *visual, const char *state)
{
    lvgl_port_lock(1000);
    lv_subject_set_pointer(visual->state, (void *)state);
    lvgl_port_unlock();
}

void visual_set_text(visual_t *visual, const char *text)
{
    lvgl_port_lock(1000);
    lv_subject_copy_string(visual->text, text);
    lvgl_port_unlock();
}

void visual_update(visual_t *visual, int battery_soc, int wifi_rssi)
{
    if (battery_soc > 100)
    {
        battery_soc = 100;
    }
    if (battery_soc < 0)
    {
        battery_soc = 0;
    }

    static const char *battery_socs[] = {
        FONT_AWESOME_BATTERY_EMPTY,
        FONT_AWESOME_BATTERY_QUARTER,
        FONT_AWESOME_BATTERY_HALF,
        FONT_AWESOME_BATTERY_THREE_QUARTERS,
        FONT_AWESOME_BATTERY_FULL,
        FONT_AWESOME_BATTERY_FULL,
    };
    const char *wifi_str;
    if (wifi_rssi >= 0)
    {
        wifi_str = FONT_AWESOME_WIFI_SLASH;
    }
    else if (wifi_rssi < -70)
    {
        wifi_str = FONT_AWESOME_WIFI_WEAK;
    }
    else if (wifi_rssi < -50)
    {
        wifi_str = FONT_AWESOME_WIFI_FAIR;
    }
    else
    {
        wifi_str = FONT_AWESOME_WIFI;
    }
    lvgl_port_lock(1000);
    lv_subject_set_pointer(visual->battery, (void *)battery_socs[battery_soc / 20]);
    lv_subject_set_pointer(visual->wifi, (void *)wifi_str);
    lvgl_port_unlock();
}

static void visual_notification_timer_cb(lv_timer_t *timer)
{
    lvgl_port_lock(1000);
    visual_t *visual = (visual_t *)lv_timer_get_user_data(timer);
    if (visual->notification)
    {
        lv_obj_delete(visual->notification);
        visual->notification = NULL;
    }
    lvgl_port_unlock();
}
void visual_show_notification(visual_t *visual, const char *title, const char *text, int timeout_ms)
{
    lvgl_port_lock(1000);
    if (visual->notification)
    {
        lv_obj_delete(visual->notification);
        visual->notification = NULL;
        lv_timer_delete(visual->notification_timer);
    }

    if (!text)
    {
        lvgl_port_unlock();
        return;
    }

    visual->notification = lv_msgbox_create(lv_screen_active());
    lv_obj_set_style_text_font(visual->notification, visual->theme.text_font, 0);
    lv_obj_set_size(visual->notification, LV_PCT(80), LV_PCT(50));
    lv_obj_center(visual->notification);
    if (title)
    {
        lv_msgbox_add_title(visual->notification, title);
    }
    lv_msgbox_add_text(visual->notification, text);

    visual->notification_timer = lv_timer_create(visual_notification_timer_cb, timeout_ms, visual);
    lv_timer_set_repeat_count(visual->notification_timer, 1);
    lv_timer_set_auto_delete(visual->notification_timer, true);
    lvgl_port_unlock();
}

void visual_show_qrcode(visual_t *visual, const char *title, const char *content)
{
    lvgl_port_lock(1000);
    // 删除旧二维码
    if (visual->qrcode)
    {
        lv_obj_del(visual->qrcode);
        visual->qrcode = NULL;
    }

    if (!content)
    {
        lvgl_port_unlock();
        return;
    }

    lv_obj_t *screen = lv_screen_active();
    visual->qrcode = lv_obj_create(screen);
    lv_obj_set_style_border_width(visual->qrcode, 0, 0);
    lv_obj_set_style_bg_color(visual->qrcode, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(visual->qrcode, LV_OPA_30, 0);
    lv_obj_set_size(visual->qrcode, LV_PCT(90), LV_PCT(80));
    lv_obj_center(visual->qrcode);

    if (title)
    {
        // 添加二维码标题
        lv_obj_t *qrcode_label = lv_label_create(visual->qrcode);
        lv_obj_set_style_text_color(qrcode_label, lv_color_white(), 0);
        lv_obj_set_style_text_font(qrcode_label, visual->theme.text_font, 0);
        lv_label_set_text(qrcode_label, title);
        lv_obj_align(qrcode_label, LV_ALIGN_TOP_MID, 0, 0);
    }

    lv_obj_t *qrcode_square = lv_qrcode_create(visual->qrcode);
    lv_obj_align(qrcode_square, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_qrcode_set_size(qrcode_square, 200);
    lv_qrcode_set_data(qrcode_square, content);
    lvgl_port_unlock();
}

void visual_set_emotion(visual_t *visual, const char *emotion)
{
    const char *emoji = "😶";
    for (size_t i = 0; i < sizeof(emotions) / sizeof(emotion_t); i++)
    {
        if (strcmp(emotion, emotions[i].emotion) == 0)
        {
            emoji = emotions[i].emoji;
            break;
        }
    }
    lvgl_port_lock(1000);
    lv_subject_set_pointer(visual->emotion, (void *)emoji);
    lvgl_port_unlock();
}
