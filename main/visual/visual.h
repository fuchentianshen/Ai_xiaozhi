#pragma once

typedef struct visual visual_t;

visual_t *visual_create(void);

void visual_destroy(visual_t *visual);

void visual_set_state(visual_t *visual, const char *state);

void visual_set_emotion(visual_t *visual, const char *emotion);

void visual_set_text(visual_t *visual, const char *text);

void visual_update(visual_t *visual, int battery_soc, int wifi_rssi);

void visual_show_notification(visual_t *visual, const char *title, const char *text, int timeout_ms);

void visual_show_qrcode(visual_t *visual, const char *title, const char *content);
