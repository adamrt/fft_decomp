#include "fft/jobstts.h"

void jobstts_menu_init_and_draw_scrollable_list(void* entries, void* selected, s32 value, void* data, void* commands) {
    jobstts_menu_init_scrollable_list(entries, (s32)selected, value, data);
    jobstts_menu_draw_scrollable_list(commands);
}
