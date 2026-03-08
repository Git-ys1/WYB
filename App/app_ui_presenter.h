#ifndef APP_UI_PRESENTER_H
#define APP_UI_PRESENTER_H

#include "app_display_service.h"

app_err_t app_ui_presenter_init(void);
bool app_ui_presenter_ready(void);
app_err_t app_ui_presenter_last_err(void);
app_err_t app_ui_presenter_flush(const app_ui_frame_t *frame);

#endif
