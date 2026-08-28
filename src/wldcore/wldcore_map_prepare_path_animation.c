#include "fft/wldcore.h"
#include "psx/gte.h"
#include "psx/types.h"

/* Loads the route joining locations from and to (in either direction) into
 * g_wldcore_map_path_animation as a slideshow: copies its polyline, reversed when the record
 * runs from `to` to `from`, and times each segment by its length.
 *
 * `data` holds the polyline table and then the polyline itself, as the target
 * keeps both in one register. The header's second endpoint is read from the
 * whole header word, as the target does. The segment differences read the
 * start point first: that makes the start of the point array the loop's base
 * register, from which the other three coordinates are addressed. The unused
 * local reproduces the target's frame size. */
void wldcore_map_prepare_path_animation(s32 from, s32 to) {
    wldcore_route_record_t** records;
    u16* data;
    wldcore_route_record_t* record;
    s32 unused[2];
    s32 first;
    s32 second;
    s32 dx;
    s32 dy;
    s32 i;

    records = g_wldcore_map_route_tables;
    g_wldcore_map_path_animation.flags = 0;
    for (i = 0; i < 48; i++) {
        record = *records;
        first = record->start_location;
        second = (*(u32*)record & 0xFF0000) >> 16;
        if (first == from && second == to) {
            g_wldcore_map_path_animation.script_variable = i;
            break;
        }
        if (second == from && first == to) {
            g_wldcore_map_path_animation.script_variable = i;
            g_wldcore_map_path_animation.flags |= 2;
            break;
        }
        records++;
    }
    if (i == 48) {
        return;
    }
    g_wldcore_map_path_animation.flags |= 1;
    first = record->count - 1;
    g_wldcore_map_path_animation.frame_timer = 0;
    g_wldcore_map_path_animation.page_index = 0;
    g_wldcore_map_path_animation.page_count = first;
    data = (u16*)g_wldcore_map_route_polylines;
    data = ((u16**)data)[g_wldcore_map_path_animation.script_variable];
    g_wldcore_map_path_animation.point_count = *data;
    data++;
    for (i = 0; i < g_wldcore_map_path_animation.point_count; i++) {
        if (g_wldcore_map_path_animation.flags & 2) {
            g_wldcore_map_path_animation.coords[i * 2] = data[(g_wldcore_map_path_animation.point_count - i - 1) * 2];
            g_wldcore_map_path_animation.coords[i * 2 + 1]
                = data[(g_wldcore_map_path_animation.point_count - i - 1) * 2 + 1];
        } else {
            g_wldcore_map_path_animation.coords[i * 2] = data[i * 2];
            g_wldcore_map_path_animation.coords[i * 2 + 1] = data[i * 2 + 1];
        }
    }
    for (i = 0; i < g_wldcore_map_path_animation.page_count; i++) {
        dx = -g_wldcore_map_path_animation.coords[i * 4] + g_wldcore_map_path_animation.coords[i * 4 + 4];
        dy = -g_wldcore_map_path_animation.coords[i * 4 + 1] + g_wldcore_map_path_animation.coords[i * 4 + 5];
        g_wldcore_map_path_animation.page_frames[i] = csqrt(dx * dx + dy * dy) >> 5;
    }
}
