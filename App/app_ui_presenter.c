#include "app_ui_presenter.h"

#include "app_display_service.h"

app_err_t app_ui_presenter_init(void)
{
    return app_display_init();
}

bool app_ui_presenter_ready(void)
{
    return app_display_ready();
}

app_err_t app_ui_presenter_last_err(void)
{
    return app_display_last_err();
}

app_err_t app_ui_presenter_flush(const app_ui_frame_t *frame)
{
    return app_display_show_menu_frame(frame);
}
