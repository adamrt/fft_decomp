#include "fft/event_jobstts.h"

void jobstts_menu_set_selection_record(s32 index, s32 selected_index, s32 scroll_index, const u16* abilities) {
    g_jobstts_menu_selection_records[index].selected_index = selected_index;
    g_jobstts_menu_selection_records[index].scroll_index = scroll_index;
    g_jobstts_menu_selection_records[index].selected_ability_id = abilities[selected_index] & 0x3ff;
}
