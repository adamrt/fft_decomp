#include "fft/event_jobstts.h"

void jobstts_menu_init_and_draw_scrollable_list_simple(
    const s16* entries, s32 selected_index, const void* data, const u8* commands) {
    jobstts_menu_init_scrollable_list_core((s16*)entries, selected_index, (s32)data);
    jobstts_menu_draw_scrollable_list(commands);
}
