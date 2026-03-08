#ifndef DRV_OPAMP_INTERNAL_H
#define DRV_OPAMP_INTERNAL_H

#include <stdbool.h>

#include "drv_error.h"

app_err_t opamp1_init(void);
bool opamp1_ready(void);
app_err_t opamp1_last_status(void);

#endif
