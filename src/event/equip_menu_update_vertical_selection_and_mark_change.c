#include "fft/event_equip.h"
#include "psx/types.h"

void equip_menu_update_vertical_selection_and_mark_change(
    s32 entry_count, s32 selection_index, s32 input_mask, s8 sound_id) {
    s32 index = selection_index & 0xFF;
    s16 previous_selection = g_equip_menu_selection_values[index];
    /* The target leaves a2 (input_mask) unset for this three-argument callee. */
    if (previous_selection
        != ((s16 (*)(s32, s32))equip_menu_update_wrapped_vertical_selection)(entry_count & 0xFFFF, index)) {
        g_equip_sound_queued_effect_id = sound_id;
    }
}
