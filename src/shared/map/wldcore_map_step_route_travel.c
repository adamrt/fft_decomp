#include "fft/script.h"
#include "fft/wldcore.h"

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
 * each copy keeps the target's direct g_wldcore_map_projection_state accesses and the &state
 * base register in the flags update. */
void wldcore_map_step_route_travel(void) {
    point32_t delta;
    SVECTOR previous;
    s32 dx;
    s32 dy;
    s32 result;

    if (!(g_wldcore_location_entry_state.flags & WLDCORE_ROUTE_TRAVEL_ACTIVE)) {
        return;
    }
    if (g_wldcore_map_projection_motion.flags & WLDCORE_PROJECTION_MOTION_ACTIVE) {
        if (wldcore_map_step_projection_motion() == 0) {
            g_wldcore_map_projection_state.flags
                |= WLDCORE_PROJECTION_FLAG_UPDATE_DIRECTION | WLDCORE_PROJECTION_FLAG_ALTERNATE_DIRECTION_SET;
        }
        g_wldcore_map_projection_state.flags |= WLDCORE_PROJECTION_FLAG_UPDATE_POSITION;
        return;
    }
    if (g_wldcore_location_entry_state.segment_frame >= g_wldcore_location_entry_state.segment_frames) {
        if (++g_wldcore_location_entry_state.segment_index >= g_wldcore_location_entry_state.segment_count) {
            wldcore_calendar_advance_day();
            g_wldcore_map_projection_state.marker.position = g_wldcore_location_entry_state.target_position;
            g_wldcore_map_projection_state.flags
                |= WLDCORE_PROJECTION_FLAG_UPDATE_POSITION | WLDCORE_PROJECTION_FLAG_UPDATE_DIRECTION;
            g_wldcore_map_projection_state.marker.kind = g_wldcore_location_entry_state.leg_end_location;
            script_set_variable(EVENT_SCRIPT_VAR_LOCATION, g_wldcore_location_entry_state.leg_end_location);
            g_wldcore_location_entry_state.route_index++;
            if (wldcore_script_process_conditional_set(
                    g_wldcore_map_projection_state.marker.kind, WLDCORE_SCRIPT_ACTION_LOAD_EVENT)
                != 0) {
                g_wldcore_next_map_id[0] = 0;
                script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, g_wldcore_script_state.args[0]);
                wldcore_menu_push_event_transition_level(g_wldcore_script_state.args[1]);
                g_wldcore_map_projection_state.angle = 0;
                g_wldcore_map_projection_state.flags &= ~WLDCORE_PROJECTION_FLAG_ALTERNATE_DIRECTION_SET;
                g_wldcore_location_entry_state.flags &= ~(WLDCORE_ROUTE_TRAVEL_ACTIVE | WLDCORE_ROUTE_TRAVEL_REVERSED);
                g_wldcore_menu_stack_records_next[g_wldcore_location_entry_state.menu_level]
                    .location_transition.location_id = g_wldcore_map_projection_state.marker.kind;
                return;
            }
            if (wldcore_script_process_conditional_set(
                    g_wldcore_map_projection_state.marker.kind, WLDCORE_SCRIPT_ACTION_SET_VAR_2)
                != 0) {
                script_set_variable(g_wldcore_script_state.args[0], g_wldcore_script_state.args[1]);
            }
            if (wldcore_map_roll_random_encounter(g_wldcore_location_entry_state.leg_end_location,
                    g_wldcore_location_entry_state.route_path.routes[g_wldcore_location_entry_state.route_index - 1]
                        & WLDCORE_ROUTE_PATH_ID_MASK)
                != 0) {
                g_main_system_flags |= MAIN_SYSTEM_FLAG_RANDOM_BATTLE;
                g_wldcore_next_map_id[0] = g_wldcore_random_battle_map_id;
                script_set_variable(EVENT_SCRIPT_VAR_CURRENT_ENTD, g_wldcore_random_battle_entd_id);
                script_set_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_SQUAD_COUNT, 1);
                script_set_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_PRIMARY_SQUAD_ID, g_wldcore_random_battle_squad_id);
                script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, 0);
                wldcore_menu_push_event_transition_level(WLDCORE_EVENT_TRANSITION_MODE_SPIN);
                g_wldcore_map_projection_state.angle = 0;
                g_wldcore_map_projection_state.flags &= ~WLDCORE_PROJECTION_FLAG_ALTERNATE_DIRECTION_SET;
                g_wldcore_location_entry_state.flags &= ~(WLDCORE_ROUTE_TRAVEL_ACTIVE | WLDCORE_ROUTE_TRAVEL_REVERSED);
                g_wldcore_menu_stack_records_next[g_wldcore_location_entry_state.menu_level]
                    .location_transition.location_id = g_wldcore_map_projection_state.marker.kind;
                return;
            }
            wldcore_sound_play_effect(MAIN_SFX_WORLD_MAP_ARRIVE);
            if (g_wldcore_location_entry_state.route_index < g_wldcore_location_entry_state.best_path_length) {
                g_wldcore_location_entry_state.flags &= ~WLDCORE_ROUTE_TRAVEL_REVERSED;
                g_wldcore_location_entry_state.segment_count = wldcore_location_begin_route_segment(
                    &g_wldcore_location_entry_state,
                    g_wldcore_location_entry_state.route_path.routes[g_wldcore_location_entry_state.route_index], 0);
                g_wldcore_location_entry_state.segment_index = 0;
                g_wldcore_map_projection_state.angle = g_wldcore_location_entry_state.heading;
                return;
            }
            g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_location_entry_state.menu_level]
                                         .location_transition.window_index]
                .flags ^= WORLD_RENDER_RECORD_HIDDEN;
            g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_location_entry_state.menu_level]
                                         .location_transition.window_index]
                .position.x = g_wldcore_map_projection_state.marker.render.position.x;
            g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_location_entry_state.menu_level]
                                         .location_transition.window_index]
                .position.y = g_wldcore_map_projection_state.marker.render.position.y - 4;
            g_main_system_flags &= ~MAIN_SYSTEM_FLAG_HIDE_LOCATION_MARKER;
            g_wldcore_location_entry_state.arrival_location = g_wldcore_map_projection_state.marker.kind + 1;
            file_wait_for_pending_variant();
            g_wldcore_map_projection_state.angle = 0;
            g_wldcore_map_projection_state.flags &= ~WLDCORE_PROJECTION_FLAG_ALTERNATE_DIRECTION_SET;
            g_wldcore_location_entry_state.flags &= ~(WLDCORE_ROUTE_TRAVEL_ACTIVE | WLDCORE_ROUTE_TRAVEL_REVERSED);
            g_wldcore_menu_stack_records_next[g_wldcore_location_entry_state.menu_level].location_transition.location_id
                = g_wldcore_map_projection_state.marker.kind;
            result = wldcore_location_collect_choice_text_ids(g_wldcore_map_projection_state.marker.kind,
                g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].location_transition.choice_text_ids);
            if (result != -1 && result > 0) {
                g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].location_transition.pending_result
                    = result;
                wldcore_menu_dispatch_publish_handler();
                wldcore_menu_push_location_menu_level(g_wldcore_map_projection_state.marker.kind);
            }
            return;
        } else {
            wldcore_location_begin_route_segment(&g_wldcore_location_entry_state,
                g_wldcore_location_entry_state.route_id, g_wldcore_location_entry_state.segment_index);
            g_wldcore_map_projection_state.angle = g_wldcore_location_entry_state.heading;
            g_wldcore_map_projection_state.flags |= WLDCORE_PROJECTION_FLAG_UPDATE_DIRECTION;
        }
    } else {
        g_wldcore_location_entry_state.segment_frame++;
    }
    dx = (g_wldcore_location_entry_state.target_position.vx - g_wldcore_location_entry_state.position.vx)
        * g_wldcore_location_entry_state.segment_frame / g_wldcore_location_entry_state.segment_frames;
    dy = (g_wldcore_location_entry_state.target_position.vy - g_wldcore_location_entry_state.position.vy)
        * g_wldcore_location_entry_state.segment_frame / g_wldcore_location_entry_state.segment_frames;
    previous = g_wldcore_map_projection_state.marker.position;
    delta.x = g_wldcore_location_entry_state.previous_position.vx + dx;
    g_wldcore_map_projection_state.marker.position.vx = delta.x;
    delta.y = g_wldcore_location_entry_state.previous_position.vy + dy;
    g_wldcore_map_projection_state.marker.position.vy = delta.y;
    delta.x -= previous.vx;
    delta.y -= previous.vy;
    wldcore_map_scroll_projection_if_focus_near_edge(&delta);
    g_wldcore_map_projection_state.flags |= WLDCORE_PROJECTION_FLAG_UPDATE_POSITION;
}
