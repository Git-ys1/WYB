#ifndef DRV_BEEP_H
#define DRV_BEEP_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    BEEP_PATTERN_NONE = 0,
    BEEP_PATTERN_OK,
    BEEP_PATTERN_ALERT
} beep_pattern_t;

void beep_init(uint32_t freq_hz);
void beep_once(uint32_t duration_ms);
void beep_continuous(bool on);
void beep_pattern(beep_pattern_t pattern);
void beep_tick(void);

#endif
