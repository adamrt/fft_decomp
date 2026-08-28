#include "fft/jobstts.h"

void jobstts_menu_get_selection_record(
    s32 index, s16* out_selected_index, s16* out_scroll_index, const s16* abilities) {
    s32 ability_id;
    s32 i;

    *out_selected_index = g_jobstts_menu_selection_records[index].selected_index;
    *out_scroll_index = g_jobstts_menu_selection_records[index].scroll_index;
    ability_id = (s16)g_jobstts_menu_selection_records[index].selected_ability_id;
    if (ability_id != abilities[*out_selected_index]) {
        i = 0;
        if (*abilities != -1) {
            do {
                if ((*abilities & 0x3ff) == ability_id) {
                    *out_selected_index = i;
                    return;
                }
                abilities++;
                i++;
            } while (*abilities != -1);
        }
        *out_selected_index = 0;
        *out_scroll_index = 0;
    }
}
