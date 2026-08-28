#include "fft/data.h"
#include "fft/job.h"
#include "fft/main_sound.h"
#include "fft/world.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Steps the formation roster cursor one slot backwards (L1) or forwards (R1),
 * wrapping at either end and skipping over egg units. Plays menu sound 6 on
 * either move.
 */
void world_formation_cycle_selected_unit(s32 pad_buttons) {
    u16 index;
    s32 prev;

    index = pad_buttons;
    if (pad_buttons & PSX_PAD_L1) {
        g_world_menu_sound_effect_id = MAIN_SFX_PAGE_SWITCH;
        do {
            if (g_world_formation_selected_unit_index == 0) {
                index = (u16)g_world_formation_unit_count - 1;
            } else {
                prev = (u16)g_world_formation_selected_unit_index;
                index = prev - 1;
            }
            g_world_formation_selected_unit_index = index;
        } while (g_world_formation_unit_pointers[g_world_formation_selected_unit_index]->gender_flags & UNIT_FLAG_EGG);
    } else if (index & PSX_PAD_R1) {
        g_world_menu_sound_effect_id = MAIN_SFX_PAGE_SWITCH;
        do {
            index = (g_world_formation_selected_unit_index != g_world_formation_unit_count - 1)
                ? g_world_formation_selected_unit_index + 1
                : 0;
            g_world_formation_selected_unit_index = index;
        } while (g_world_formation_unit_pointers[g_world_formation_selected_unit_index]->gender_flags & UNIT_FLAG_EGG);
    }
}
