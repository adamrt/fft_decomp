/* Cycle the formation roster selection with L1 and R1, skipping egg units. */

#include "fft/formation.h"
#include "fft/sound.h"
#include "fft/unit.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Steps the formation roster cursor one slot backwards (L1) or forwards (R1),
 * wrapping at either end and skipping over egg units. Plays the page-switch
 * sound on either move. */
void formation_selected_unit_cycle(s16 pad_buttons) {
    u16 index;

    if (pad_buttons & PSX_PAD_L1) {
        g_sound_queued_effect_id = SOUND_SFX_PAGE_SWITCH;
        do {
            if (g_unit_selected_index == 0) {
                index = g_formation_unit_count - 1;
            } else {
                index = g_unit_selected_index - 1;
            }
            g_unit_selected_index = index;
        } while (g_formation_unit_pointers[g_unit_selected_index]->gender_flags & UNIT_FLAG_EGG);
    } else if (pad_buttons & PSX_PAD_R1) {
        g_sound_queued_effect_id = SOUND_SFX_PAGE_SWITCH;
        do {
            index = (g_unit_selected_index != g_formation_unit_count - 1) ? g_unit_selected_index + 1 : 0;
            g_unit_selected_index = index;
        } while (g_formation_unit_pointers[g_unit_selected_index]->gender_flags & UNIT_FLAG_EGG);
    }
}
