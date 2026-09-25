#include "fft/event_jobstts.h"
#include "psx/types.h"

void jobstts_menu_init_scrollable_list(const s16* entries, s32 selected_index, s32 value, const void* data) {
    jobstts_menu_init_scrollable_list_core((s16*)entries, selected_index, (s32)data);
    g_jobstts_menu_scroll_base_index = value;
}
