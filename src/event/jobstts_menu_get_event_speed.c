#include "fft/event_jobstts.h"

s32 jobstts_menu_get_event_speed(void) {
    s32 value = g_jobstts_menu_event_speed;

    if (value == 0) {
        return 1;
    }
    return value;
}
