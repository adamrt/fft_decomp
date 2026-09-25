#include "fft/event_bunit.h"
#include "psx/types.h"

void bunit_menu_get_selection_record(s32 index, s16* out_index, s16* out_b, s16* list) {
    s32 target;
    s32 i;

    *out_index = g_bunit_menu_selection_records[index].selected_index;
    *out_b = g_bunit_menu_selection_records[index].scroll_base_index;
    target = g_bunit_menu_selection_records[index].entry_id;
    if (target == list[*out_index]) {
        return;
    }
    i = 0;
    if (list[0] != -1) {
        do {
            if ((*list & 0x3FF) == target) {
                *out_index = i;
                return;
            }
            list++;
            i++;
        } while (*list != -1);
    }
    *out_index = 0;
    *out_b = 0;
}
