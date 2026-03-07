#include "App/app.h"
#include "BSP/bsp.h"

int main_host_entry(void)
{
    bsp_init();
    app_init();

    while (1) {
        app_poll_button();
        app_measure_tick();
        app_ui_tick();
        app_beep_tick();
    }
}
