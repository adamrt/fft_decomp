#include "fft/battle.h"
#include "fft/battle_map.h"
#include "fft/battle_script.h"
#include "fft/map.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "psx/types.h"

/* Run one event frame, then return whether the deployment thread (borrowed slot
 * 6) is still running. */
s32 script_is_deployment_running(u32* ot, u32 buttons) {
    script_run_event_frame_variant(ot, buttons);
    return thread_is_running(THREAD_BORROWED_SLOT_DEPLOYMENT);
}

/*
 * Run one frame of the battle menu/event layer and return battle_menu_resolve_selection's
 * result. While event threads 6-8 run (thread 8 only when g_thread_contexts[8].task_id is not
 * 0x42) the event speed is forced to 2, restoring the saved speed afterwards.
 *
 * The `fill` staging variable places the -2 load ahead of the loop counter
 * the way the target does; a literal store hoists it after the counter.
 */
s32 menu_update_menus_and_event_speed(u32* otag, s32 controller_input) {
    s32 i;
    s32 result;
    s32 fill;

    g_ai_workspace_ptr = (event_work_t*)g_ai_workspace;
    if (g_menu_input_mode != MENU_INPUT_PLAYER) {
        controller_input = g_script_unfiltered_controller_input;
    }
    map_store_selected_tile_data();
    fill = -2;
    for (i = 3; i >= 0; i--) {
        g_menu_pending_selection[i] = fill;
    }
    script_run_event_frame_variant(otag, controller_input);
    menu_update_panel_fade();
    result = menu_resolve_selection();
    if ((thread_is_running_asm_variant(THREAD_SLOT_MENU) != 0
            && g_thread_contexts[THREAD_SLOT_MENU].task_id != THREAD_TASK_MINI_MENU)
        || thread_is_running_asm_variant(THREAD_SLOT_MENU_CHILD) != 0
        || thread_is_running_asm_variant(THREAD_SLOT_MENU_GRANDCHILD) != 0) {
        if (g_menu_animation_speed_forced == 0) {
            g_menu_animation_speed_forced = 1;
            g_menu_saved_animation_speed = battle_state_get_animation_speed();
            script_set_event_speed_variant(2);
        }
    } else if (g_menu_animation_speed_forced == 1) {
        g_menu_animation_speed_forced = 0;
        script_set_event_speed_variant(g_menu_saved_animation_speed);
    }
    return result;
}

/* Fill g_map_selected_tile_data for the tile under the cursor, which the
 * tile information display shows: the surface type, the depth, the displayed
 * height in half-height units (height * 2 + slope half-height + depth * 2) and
 * the surface type's two bytes of g_main_terrain_tile_info_pairs. */
void map_store_selected_tile_data(void) {
    SVECTOR coords;
    s16* surface_type;
    s32 depth;

    map_store_selected_tile_coordinates(&coords);
    surface_type = &g_map_selected_tile_data.surface_type;
    *surface_type = map_get_tile_data_value(MAP_TILE_DATA_SURFACE_TYPE, coords.vx, coords.vz, coords.vy) & 0xFF;
    g_map_selected_tile_data.display_height
        = map_get_tile_data_value(MAP_TILE_DATA_HALF_HEIGHT, coords.vx, coords.vz, coords.vy) & 0xFF;
    g_map_selected_tile_data.display_height
        += (map_get_tile_data_value(MAP_TILE_DATA_HEIGHT, coords.vx, coords.vz, coords.vy) & 0xFF) * 2;
    depth = map_get_tile_data_value(MAP_TILE_DATA_DEPTH, coords.vx, coords.vz, coords.vy) & 0xFF;
    g_map_selected_tile_data.depth = depth;
    g_map_selected_tile_data.display_height += depth * 2;
    g_map_selected_tile_data._unknown_02 = g_main_terrain_tile_info_pairs[*surface_type][0];
    g_map_selected_tile_data._unknown_04 = g_main_terrain_tile_info_pairs[*surface_type][1];
}
