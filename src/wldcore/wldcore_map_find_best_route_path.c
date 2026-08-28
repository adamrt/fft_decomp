#include "fft/main_file.h"
#include "fft/main_runtime.h"
#include "fft/wldcore.h"
#include "fft/world.h"
#include "psx/etc.h"
#include "psx/types.h"

/* Scalar views of g_wldcore_location_entry_state at 0x800d0b24 (flags), +0x18
 * (best path, copied whole by a movstrsi loop), +0x5c, +0x60 (best cost) and
 * +0x64 (best length). The target addresses each with its own lui; a struct
 * access here makes GCC keep the base in a register instead. */
extern s32 g_wldcore_location_entry_flags;
extern wldcore_route_path_t g_wldcore_route_best_path;
extern s32 g_wldcore_route_best_path_length;
extern s32 g_wldcore_route_best_cost;
extern s32 g_wldcore_route_best_length_bound;

/* Search path under construction; g_wldcore_route_search_depth is its length and g_wldcore_route_search_cost
 * its accumulated cost. */
extern wldcore_route_path_t g_wldcore_route_search_path;

/* Depth-first search for the cheapest route path between two locations.
 *
 * Tries every unlocked route (script variable 0x22c + route) not already on
 * the path; a route joining from and to completes the path, which is kept
 * when it is shorter, or as long but no more expensive, than the best one.
 * Other routes leaving from recurse from their far end. The frame is kept
 * responsive while searching: past 200 vsyncs the world frame is redrawn.
 *
 * The block-scoped copies of g_wldcore_route_search_cost give the target's `addu` operand
 * order, and computing both decrements before storing them after the
 * recursive call keeps `cost` short-lived enough to be allocated $s0. */
void wldcore_map_find_best_route_path(s32 from, s32 to) {
    s32 unused[4];
    s32 cost;
    s32 route;
    wldcore_route_record_t** table;
    s32 i;
    s32 depth;
    s32 total;
    s32* routes;
    wldcore_route_record_t* record;
    s32 start;
    s32 end;
    s32 length;
    s32 buffer_index;

    if (g_wldcore_route_search_depth < 16) {
        route = 0;
        routes = g_wldcore_route_search_path.routes;
        for (table = g_wldcore_map_route_tables; route < 48; route++, table++) {
            if (VSync(1) > 200) {
                main_file_poll_load(&g_main_file_cd_state);
                wldcore_sound_process_audio_queue();
                wldcore_map_pulse_dot_colors();
                wldcore_gfx_draw_world_frame();
                buffer_index = world_gs_get_active_buffer();
                g_active_graphics_buffer_index = buffer_index;
                world_gs_setworkbase(&g_wldcore_gfx_world_primitive_buffers + buffer_index * 0xE000);
            }
            if (world_script_get_variable(route + 0x22C) == 0) {
                continue;
            }
            for (i = 0; i < g_wldcore_route_search_depth; i++) {
                if ((routes[i] & 0xff) == route) {
                    break;
                }
            }
            if (i < g_wldcore_route_search_depth) {
                continue;
            }
            record = *table;
            length = g_wldcore_route_search_depth + 1;
            start = record->start_location;
            end = record->end_location;
            cost = record->cost;
            if (g_wldcore_route_best_length_bound < length) {
                continue;
            }
            if (length == g_wldcore_route_best_length_bound
                && g_wldcore_route_best_cost < cost + g_wldcore_route_search_cost) {
                continue;
            }
            if (from == start && to == end) {
                routes[g_wldcore_route_search_depth] = route;
                /* Shared tail: the target records both directions through one
                 * copy (its path copy loop keeps a duplicate from cross-jumping). */
                goto found;
            }
            if (to == start && from == end) {
                routes[g_wldcore_route_search_depth] = route | 0x100;
            found:
                g_wldcore_route_best_path = g_wldcore_route_search_path;
                {
                    s32 current = g_wldcore_route_search_cost;

                    g_wldcore_route_best_cost = current + cost;
                }
                g_wldcore_route_best_length_bound = g_wldcore_route_best_path_length = g_wldcore_route_search_depth + 1;
                g_wldcore_location_entry_flags |= 1;
                continue;
            }
            if (from == start) {
                routes[g_wldcore_route_search_depth] = route;
                g_wldcore_route_search_depth++;
                {
                    s32 current = g_wldcore_route_search_cost;

                    g_wldcore_route_search_cost = current + cost;
                }
                wldcore_map_find_best_route_path(end, to);
                depth = g_wldcore_route_search_depth - 1;
                total = g_wldcore_route_search_cost - cost;
                g_wldcore_route_search_depth = depth;
                g_wldcore_route_search_cost = total;
            } else if (from == end) {
                routes[g_wldcore_route_search_depth] = route | 0x100;
                g_wldcore_route_search_depth++;
                {
                    s32 current = g_wldcore_route_search_cost;

                    g_wldcore_route_search_cost = current + cost;
                }
                wldcore_map_find_best_route_path(start, to);
                depth = g_wldcore_route_search_depth - 1;
                total = g_wldcore_route_search_cost - cost;
                g_wldcore_route_search_depth = depth;
                g_wldcore_route_search_cost = total;
            }
        }
    }
}
