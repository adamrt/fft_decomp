#include "fft/main_sound.h"
#include "fft/thread.h"
#include "fft/wldcore.h"
#include "fft/world.h"
#include "psx/pad.h"

void wldcore_menu_render_debug_variable_list(wldcore_menu_variable_list_level_t* level);

/* Step handler for the number-panel menu level (type 0x30). While the panel
 * runs it accepts the confirm button (0x20), which plays sound 1 and writes
 * the panel's value to the level's script variable, or the cancel button
 * (0x40); either way it restarts thread 0xC to close the panel. Once the
 * panel thread has stopped it pops the level, redraws the script-variable
 * list on a confirm, and restores the parent level's window and render
 * records.
 *
 * Reading the parent window index before the render-record store is what
 * lets the scheduler place that load inside the render index's multiply
 * sequence, as the target does. */
void wldcore_menu_step_number_panel_level(wldcore_menu_panel_level_t* level) {
    s32 depth;
    s32 parent;
    s32 slot;
    wldcore_window_record_t* record;
    u32 buttons;

    if (level->result != 0 && world_thread_is_running(0xC) == 0) {
        g_wldcore_menu_result = 0;
        g_wldcore_menu_ordering_table_offset = 1;
        depth = g_wldcore_menu_stack_depth - 1;
        g_wldcore_menu_stack_depth = depth;
        if (level->result == 1) {
            wldcore_menu_render_debug_variable_list(&g_wldcore_menu_stack_records[depth].variable_list);
        }
        parent = g_wldcore_menu_stack_depth - 1;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[parent].window_render.window_index].sequence = 2;
        slot = g_wldcore_menu_stack_records_next[parent].window_render.window_index;
        g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[parent].window_render.render_index].palette
            = 0;
        record = &g_wldcore_window_records[slot];
        record->palette = 0;
        record->frame_index = 0;
        g_wldcore_window_records[slot].anim_counter = 0;
        return;
    }
    buttons = g_wldcore_new_button_presses;
    if (buttons & PSX_PAD_CROSS) {
        level->result = 2;
    } else if (buttons & PSX_PAD_CIRCLE) {
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        level->result = 1;
        world_script_set_variable(level->variable_id, g_wldcore_active_menu_value);
    } else {
        return;
    }
    world_thread_set_parameters(0xC, 0, 0, 1);
}
