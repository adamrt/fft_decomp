#include "fft/wldcore.h"

/* Menu step that toggles script variable 0x200 + dot_index. Once system
 * flag 0x08 is clear it plays the pending sound (setting the variable to 1
 * when it was clear), waits out the delay with system flag 0x02 raised, then,
 * if the variable was set, flags the map dot 0x10 and clears the variable
 * before closing the level.
 *
 * The window index is read into a local before the flag clear, which keeps
 * the target's load and store order. */
void wldcore_map_step_location_visibility_change(wldcore_map_visibility_level_t* level) {
    s32 index;

    if (level->pending_sound != 0) {
        if (g_main_system_flags & 8) {
            return;
        }
        if (level->was_set != 0) {
            wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        } else {
            world_script_set_variable(level->dot_index + 0x200, 1);
            wldcore_sound_play_effect(0x77);
        }
        level->pending_sound = 0;
    }
    if (level->delay != 0) {
        g_main_system_flags |= 2;
        level->delay--;
        return;
    }
    if (level->was_set != 0) {
        g_wldcore_map_dots[level->dot_index].flags |= 0x10;
        world_script_set_variable(level->dot_index + 0x200, 0);
    }
    g_main_system_flags |= 2;
    g_wldcore_menu_stack_depth--;
    index = g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1].window_index;
    g_main_system_flags &= ~0x2000;
    g_wldcore_window_records[index].flags &= ~0x10;
    wldcore_menu_dispatch_resume_handler();
}
