#include "fft/main_runtime.h"
#include "fft/script_variables.h"
#include "fft/wldcore.h"
#include "fft/world.h"

s32 wldcore_script_process_conditional_set(s32 location, s32 action_mask);

/* Location-entry step: when the current location's conditional set yields
 * LoadEvent, stores script variable 0x27 and pushes menu level type 0x23;
 * otherwise it applies a pending SetVar2, rescans the location entries
 * through 0x8008df98 and, for a found entry, records it, hides the parent
 * window, starts the projection motion and loads the proposition picture.
 * Returns 0 after the type-0x23 push, else 1.
 *
 * g_wldcore_route_search_depth is a separate scalar: as a field of the entry state it becomes
 * the address anchor that the later &g_wldcore_location_entry_flags argument is formed from, and
 * the chained 99999 store makes best_cost that anchor as in the target. The
 * location load and the two argument addresses share one base register in the
 * target, which needs them to be members of the one g_wldcore_location_view view. */
s32 wldcore_location_process_entry(s32 from_location, s32 to_location) {
    s32 depth;

    if (wldcore_script_process_conditional_set(g_wldcore_location_view.location_id, WLDCORE_SCRIPT_ACTION_LOAD_EVENT)
        != 0) {
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1].window_index].flags
            |= 0x10;
        g_wldcore_next_map_id[0] = 0;
        world_script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, g_wldcore_script_state.args[0]);
        wldcore_menu_push_event_transition_level(g_wldcore_script_state.args[1]);
        g_main_system_flags |= 0x2000;
        return 0;
    } else {
        if (wldcore_script_process_conditional_set(g_wldcore_location_view.location_id, WLDCORE_SCRIPT_ACTION_SET_VAR_2)
            != 0) {
            world_script_set_variable(g_wldcore_script_state.args[0], g_wldcore_script_state.args[1]);
        }
        g_wldcore_route_search_cost = 0;
        g_wldcore_route_search_depth = 0;
        g_wldcore_location_entry_state.best_cost = g_wldcore_location_entry_state.best_length_bound = 99999;
        g_wldcore_location_entry_state.flags = 0;
        wldcore_map_find_best_route_path(from_location, to_location);
        if (!(g_wldcore_location_entry_state.flags & 1)) {
            return 1;
        }
        g_wldcore_location_entry_state.route_index = 0;
        if (g_wldcore_location_entry_state.best_path_length == 0) {
            return 1;
        }
        g_wldcore_location_entry_state.segment_count = wldcore_location_begin_route_segment(
            &g_wldcore_location_entry_state, g_wldcore_location_entry_state.route_path.routes[0], 0);
        g_wldcore_location_entry_state.segment_index = 0;
        g_wldcore_location_view.angle = g_wldcore_location_entry_state.heading;
        depth = g_wldcore_menu_stack_depth - 1;
        g_wldcore_location_entry_state.menu_level = depth;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].window_index].flags |= 0x10;
        if (wldcore_map_start_projection_motion_if_outside_bounds(
                &g_wldcore_location_view.point, &g_wldcore_location_view.coordinates.vx)
            == 0) {
            g_wldcore_location_view.projection_state |= 6;
        }
        g_wldcore_location_view.picture_id = wldcore_proposition_load_picture(to_location);
        g_main_system_flags |= 0x2000;
        return 1;
    }
}
