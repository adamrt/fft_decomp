#include "fft/wldcore.h"
#include "psx/types.h"

/* Provisional: a route word read whole (lw, stored as its low half) or as its
 * high half (lhu at +2). */
typedef union wldcore_route_word {
    s32 word;
    struct {
        s16 low;
        u16 high;
    } halves;
} wldcore_route_word_t;

/* Provisional: one 12-byte route point. The heading's high half is a 12-bit
 * angle. The points follow the wldcore_route_record_t header and are indexed
 * as 4-byte words from the record base, (point * 3 + 1) * 4. */
typedef struct wldcore_route_point {
    wldcore_route_word_t position; /* 0x00 */
    wldcore_route_word_t heading;  /* 0x04 */
    s32 distance;                  /* 0x08 */
} wldcore_route_point_t;

/* Stages one world-map route segment in the location-entry state.
 *
 * The low byte of `route` is the route id; its bit 8 becomes flags bit 1 and
 * selects the direction, and `step` is the point to start from: the
 * descending arm counts back from the end of the record and takes the heading
 * (turned by 0x800) and distance from the preceding point; the ascending arm
 * takes them from the point itself. Returns the point count minus one.
 *
 * The table pointer and the record share one variable: that is what makes the
 * target load the table first and keep both in $t0. The unused 32-byte local
 * reproduces the target's otherwise empty 0x20 frame, and `index` is shared by
 * both arms while each arm has its own point pointer, which fixes the
 * target's register choice for both index computations. Reading a position
 * word through `word` keeps the target's whole-word lw (a direct narrowing
 * store is expanded as lhu), and the `flags` local gives its `or` operand
 * order. */
s32 wldcore_location_begin_route_segment(wldcore_location_entry_state_t* state, s32 route, s32 step) {
    s32 unused[8];
    wldcore_route_record_t* record;
    s32 index;
    wldcore_route_point_t* point_down;
    wldcore_route_point_t* point_up;
    s32 word;
    s32 flags;

    record = (wldcore_route_record_t*)g_wldcore_map_route_tables;
    state->route_id = route & 0xff;
    flags = state->flags;
    state->flags = flags | (((u32)route >> 7) & 2);
    record = ((wldcore_route_record_t**)record)[state->route_id];
    if (state->flags & 2) {
        state->leg_end_location = record->start_location;
        state->leg_start_location = record->end_location;
        index = (record->count - step - 1) * 3 + 1;
        point_down = (wldcore_route_point_t*)(index * 4 + (s32)record);
        word = point_down->position.word;
        state->position.vx = word;
        state->position.vy = point_down->position.halves.high;
        state->position.vz = 0;
        word = point_down[-1].position.word;
        state->target_position.vx = word;
        state->target_position.vy = point_down[-1].position.halves.high;
        state->target_position.vz = 0;
        state->previous_position = state->position;
        state->heading = (point_down[-1].heading.halves.high + 0x800) & 0xfff;
        state->segment_frames = point_down[-1].distance * 2;
    } else {
        state->leg_start_location = record->start_location;
        state->leg_end_location = record->end_location;
        index = step * 3 + 1;
        point_up = (wldcore_route_point_t*)(index * 4 + (s32)record);
        word = point_up->position.word;
        state->position.vx = word;
        state->position.vy = point_up->position.halves.high;
        state->position.vz = 0;
        word = point_up[1].position.word;
        state->target_position.vx = word;
        state->target_position.vy = point_up[1].position.halves.high;
        state->target_position.vz = 0;
        state->previous_position = state->position;
        state->heading = point_up->heading.halves.high;
        state->segment_frames = point_up->distance * 2;
    }
    state->segment_frame = 0;
    return record->count - 1;
}
