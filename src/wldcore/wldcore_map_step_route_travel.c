#include "fft/main_runtime.h"
#include "fft/script_variables.h"
#include "fft/wldcore.h"
#include "fft/world.h"

s32 wldcore_script_process_conditional_set(s32 location, s32 condition);
s32 wldcore_map_roll_random_encounter(s32 location, s32 route);
s32 wldcore_location_collect_choice_text_ids(s32 location_id, s32* out);
void wldcore_map_scroll_projection_if_focus_near_edge(wldcore_point32_t* delta);

/* Advances the world-map movement along the current location route.
 *
 * Each frame interpolates the view coordinates between the step's start and
 * end vertices and scrolls the projection. When a step's frames are used up
 * it starts the next step, or on the last step advances the calendar day,
 * evaluates the arrival conditionals (event, SetVar, random encounter) and
 * either continues to the next route leg or finishes at the location.
 *
 * The two event-push tails are written out in full: GCC 2.7.2 cross-jumps
 * them into the single shared tail at 0x8008e770, and only per-path CSE of
 * each copy keeps the target's direct g_wldcore_location_view accesses and the &state
 * base register in the flags update. */
void wldcore_map_step_route_travel(void) {
    wldcore_point32_t delta;
    SVECTOR previous;
    s32 dx;
    s32 dy;
    s32 result;

    if (!(g_wldcore_location_entry_state.flags & 1)) {
        return;
    }
    if (g_wldcore_map_projection_motion.flags & 1) {
        if (wldcore_map_step_projection_motion() == 0) {
            g_wldcore_location_view.projection_state |= 6;
        }
        g_wldcore_location_view.projection_state |= 1;
        return;
    }
    if (g_wldcore_location_entry_state.segment_frame >= g_wldcore_location_entry_state.segment_frames) {
        if (++g_wldcore_location_entry_state.segment_index >= g_wldcore_location_entry_state.segment_count) {
            wldcore_advance_calendar_day();
            g_wldcore_location_view.coordinates = g_wldcore_location_entry_state.target_position;
            g_wldcore_location_view.projection_state |= 3;
            g_wldcore_location_view.location_id = g_wldcore_location_entry_state.leg_end_location;
            world_script_set_variable(EVENT_SCRIPT_VAR_LOCATION, g_wldcore_location_entry_state.leg_end_location);
            g_wldcore_location_entry_state.route_index++;
            if (wldcore_script_process_conditional_set(g_wldcore_location_view.location_id, 8) != 0) {
                g_wldcore_next_map_id[0] = 0;
                world_script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, g_wldcore_script_state.args[0]);
                wldcore_menu_push_event_transition_level(g_wldcore_script_state.args[1]);
                g_wldcore_location_view.angle = 0;
                g_wldcore_location_view.projection_state &= ~4;
                g_wldcore_location_entry_state.flags &= ~3;
                g_wldcore_menu_stack_records_next[g_wldcore_location_entry_state.menu_level]
                    .location_transition.location_id = g_wldcore_location_view.location_id;
                return;
            }
            if (wldcore_script_process_conditional_set(g_wldcore_location_view.location_id, 0x40) != 0) {
                world_script_set_variable(g_wldcore_script_state.args[0], g_wldcore_script_state.args[1]);
            }
            if (wldcore_map_roll_random_encounter(g_wldcore_location_entry_state.leg_end_location,
                    g_wldcore_location_entry_state.route_path.routes[g_wldcore_location_entry_state.route_index - 1]
                        & 0xFF)
                != 0) {
                g_main_system_flags |= 0x8000;
                g_wldcore_next_map_id[0] = g_wldcore_random_battle_map_id;
                world_script_set_variable(EVENT_SCRIPT_VAR_CURRENT_ENTD, g_wldcore_random_battle_entd_id);
                world_script_set_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_SQUAD_COUNT, 1);
                world_script_set_variable(
                    EVENT_SCRIPT_VAR_DEPLOYMENT_PRIMARY_SQUAD_ID, g_wldcore_random_battle_squad_id);
                world_script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, 0);
                wldcore_menu_push_event_transition_level(0);
                g_wldcore_location_view.angle = 0;
                g_wldcore_location_view.projection_state &= ~4;
                g_wldcore_location_entry_state.flags &= ~3;
                g_wldcore_menu_stack_records_next[g_wldcore_location_entry_state.menu_level]
                    .location_transition.location_id = g_wldcore_location_view.location_id;
                return;
            }
            wldcore_sound_play_effect(0x43);
            if (g_wldcore_location_entry_state.route_index < g_wldcore_location_entry_state.best_path_length) {
                g_wldcore_location_entry_state.flags &= ~2;
                g_wldcore_location_entry_state.segment_count = wldcore_location_begin_route_segment(
                    &g_wldcore_location_entry_state,
                    g_wldcore_location_entry_state.route_path.routes[g_wldcore_location_entry_state.route_index], 0);
                g_wldcore_location_entry_state.segment_index = 0;
                g_wldcore_location_view.angle = g_wldcore_location_entry_state.heading;
                return;
            }
            g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_location_entry_state.menu_level]
                                         .location_transition.window_index]
                .flags ^= 0x10;
            g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_location_entry_state.menu_level]
                                         .location_transition.window_index]
                .x = g_wldcore_location_view.point.x;
            g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_location_entry_state.menu_level]
                                         .location_transition.window_index]
                .y = g_wldcore_location_view.point.y - 4;
            g_main_system_flags &= ~0x2000;
            g_wldcore_location_entry_state.arrival_location = g_wldcore_location_view.location_id + 1;
            wldcore_wait_for_file_load();
            g_wldcore_location_view.angle = 0;
            g_wldcore_location_view.projection_state &= ~4;
            g_wldcore_location_entry_state.flags &= ~3;
            g_wldcore_menu_stack_records_next[g_wldcore_location_entry_state.menu_level].location_transition.location_id
                = g_wldcore_location_view.location_id;
            result = wldcore_location_collect_choice_text_ids(g_wldcore_location_view.location_id,
                (s32*)g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth]
                    .location_transition.transition_data);
            if (result != -1 && result > 0) {
                g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].location_transition.pending_result
                    = result;
                wldcore_menu_dispatch_publish_handler();
                wldcore_menu_push_location_menu_level(g_wldcore_location_view.location_id);
            }
            return;
        } else {
            wldcore_location_begin_route_segment(&g_wldcore_location_entry_state,
                g_wldcore_location_entry_state.route_id, g_wldcore_location_entry_state.segment_index);
            g_wldcore_location_view.angle = g_wldcore_location_entry_state.heading;
            g_wldcore_location_view.projection_state |= 2;
        }
    } else {
        g_wldcore_location_entry_state.segment_frame++;
    }
    dx = (g_wldcore_location_entry_state.target_position.vx - g_wldcore_location_entry_state.position.vx)
        * g_wldcore_location_entry_state.segment_frame / g_wldcore_location_entry_state.segment_frames;
    dy = (g_wldcore_location_entry_state.target_position.vy - g_wldcore_location_entry_state.position.vy)
        * g_wldcore_location_entry_state.segment_frame / g_wldcore_location_entry_state.segment_frames;
    previous = g_wldcore_location_view.coordinates;
    delta.x = g_wldcore_location_entry_state.previous_position.vx + dx;
    g_wldcore_location_view.coordinates.vx = delta.x;
    delta.y = g_wldcore_location_entry_state.previous_position.vy + dy;
    g_wldcore_location_view.coordinates.vy = delta.y;
    delta.x -= previous.vx;
    delta.y -= previous.vy;
    wldcore_map_scroll_projection_if_focus_near_edge(&delta);
    g_wldcore_location_view.projection_state |= 1;
}
