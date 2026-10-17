/* Map dots, location menus, routes and path animation, route travel, projection motion and scrolling, random
 * encounters, the location marker and camera, and render record images. */

#include "fft/calendar.h"
#include "fft/file.h"
#include "fft/gfx.h"
#include "fft/location.h"
#include "fft/menu_stack.h"
#include "fft/menu_stack_records.h"
#include "fft/screen.h"
#include "fft/script_variable.h"
#include "fft/sound.h"
#include "fft/system.h"
#include "fft/text.h"
#include "fft/window.h"
#include "fft/world_map.h"
#include "psx/libc.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/libgs.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Per location choice, its offset from EVENT_SCRIPT_VAR_LOCATION_CHOICE_HIDDEN_FIRST: a set variable hides the
 * choice from the location menu (location_build_menu_entries). */
extern u8 g_location_choice_hidden_offsets[];
extern s32 g_travel_search_depth;
extern s32 g_travel_search_cost;
extern travel_path_t g_travel_search_path;
extern random_battle_road_t (*g_random_battle_roads)[3];
extern s32 g_random_battle_map_id;
extern world_map_dot_source_t* g_world_map_dot_sources;
extern s32 g_world_map_dot_count;
extern u8 g_world_map_dot_pulse_direction;
extern u8 g_world_map_dot_pulse_phase;

/* The route records by route id: a pointer cell to the WORLD_MAP_ROUTE_COUNT record pointers. */
extern world_map_route_t** g_world_map_routes;

/* The route polylines by route id, as g_world_map_routes: each is a packed s16 count followed by that many
 * (x, y) pairs. */
extern u16** g_world_map_route_polylines;

static void world_map_negate_and_clamp(const SVECTOR* input, VECTOR* output);
static s32 random_battle_roll(s32 location, s32 route);
static void world_map_scroll_if_near_edge(point32_t* delta);
static void window_get_image_vram_position(s32 mode, RECT bounds, point32_t* output);

/* Build the world-map location dots from their packed source rows and queue them for
 * drawing. Castles use the next dot sequence, a set color variable selects the town or
 * field palette, and a clear visibility variable hides the dot. */
void world_map_init_dots(void) {
    world_map_dot_source_t* src;
    world_map_dot_t* dots = g_world_map_dots;
    world_map_dot_t* dot;
    s32* flags_base;
    CVECTOR* color;
    s32 y;
    s32 is_castle;
    s32 queue_index;
    s32 i;

    /* Queue entries are built from the flags column base; &dots[i].render.flags does not match. */
    flags_base = &dots->render.flags;
    dot = dots;
    src = g_world_map_dot_sources;
    g_world_map_dot_pulse_direction = 0;
    g_world_map_dot_pulse_phase = 0;
    g_world_map_dot_count = 0;
    for (i = 0; i < LOCATION_COUNT; i++) {
        g_world_map_dots[i].kind = (u8)(src[i].position_and_kind >> 16);
        g_world_map_dots[i].sub_kind = (u8)(src[i].position_and_kind >> 24);
        /* The x coordinate occupies the packed 32-bit value's low 16 bits. */
        g_world_map_dots[i].position.vx = src[i].position_and_kind;
        y = src[i].y;
        g_world_map_dots[i].render.flags = DISPLAY_OBJECT_ANIMATED;
        g_world_map_dots[i].render.priority = 0xf;
        /* Direct nested access biases the base to render; retail addresses the color column. */
        color = (CVECTOR*)((u8*)&g_world_map_dots[i] + offsetof(world_map_dot_t, render.color));
        g_world_map_dots[i].position.vz = 0;
        g_world_map_dots[i].position.vy = y;
        color->r = 0x80;
        color->g = 0x80;
        color->b = 0x80;
        is_castle = g_world_map_dots[i].kind == 2;
        g_world_map_dots[i].render.sequence = is_castle + WINDOW_ANIM_MAP_DOT;
        if (script_get_variable(i + EVENT_SCRIPT_VAR_LOCATION_COLOR_FIRST) != 0) {
            if (i < LOCATION_FIELD_FIRST) {
                g_world_map_dots[i].render.palette = 0xc;
            } else {
                g_world_map_dots[i].render.palette = 0x10;
            }
        } else {
            g_world_map_dots[i].render.palette = 0;
        }
        dot->render.anim_counter = 0;
        g_world_map_dots[i].render.frame_index = 0;
        if (script_get_variable(i + EVENT_SCRIPT_VAR_LOCATION_VISIBLE_FIRST) == 0) {
            g_world_map_dots[i].render.flags |= DISPLAY_OBJECT_HIDDEN;
        }
        dot++;
        queue_index = g_window_render_object_count;
        g_window_render_object_queue[queue_index] = (void*)(i * sizeof(world_map_dot_t) + (s32)flags_base);
        g_window_render_object_count = queue_index + 1;
    }

    g_world_map_dot_count = LOCATION_COUNT;
    world_map_project_dots();
}

/* Projects every world-map dot and hides the ones that are off screen, all of
 * them while SYSTEM_FLAG_HIDE_MAP_DOTS is set, and those whose location
 * is not yet visible (EVENT_SCRIPT_VAR_LOCATION_VISIBLE_FIRST + location).
 *
 * The screen bounds are written as the target's unsigned range tests: x in
 * [-128, 128] and y in [-116, 120]. */
void world_map_project_dots(void) {
    VECTOR position;
    long flag;
    s32 i;

    for (i = 0; i < g_world_map_dot_count; i++) {
        RotTrans(&g_world_map_dots[i].position, &position, &flag);
        g_world_map_dots[i].render.position.x = position.vx;
        g_world_map_dots[i].render.position.y = position.vy;
        if (position.vx < -128 || position.vx > 128 || position.vy < -116 || position.vy > 120
            || (g_system_flags & SYSTEM_FLAG_HIDE_MAP_DOTS)
            || script_get_variable(i + EVENT_SCRIPT_VAR_LOCATION_VISIBLE_FIRST) == 0) {
            g_world_map_dots[i].render.flags |= DISPLAY_OBJECT_HIDDEN;
        } else {
            g_world_map_dots[i].render.flags &= ~DISPLAY_OBJECT_HIDDEN;
        }
    }
}

/* Pulse the map dots' brightness. The phase counts up to 0x40 and back down,
 * a step a frame unless SYSTEM_FLAG_FREEZE_ANIMATION is set; dots with a
 * palette (colored towns and fields) run from 0x60 to 0xe0 with it and the
 * others the opposite way, so the two kinds pulse out of step.
 *
 * The 0x40 compare limit reuses the dead `flags` local. With its own `limit`
 * variable the allocator swaps the g_world_map_dot_pulse_direction snapshot ($a0 in the target) and
 * the limit ($v1); reusing the function-scope allocno reproduces the target
 * pair. */
void world_map_pulse_dots(void) {
    s32 flags = g_system_flags;
    s32 i;
    s32 phase;
    s32 bright;
    s32 dark;
    u8 state;
    u8 counter;
    CVECTOR* color;

    if (flags & SYSTEM_FLAG_HIDE_DISPLAY_OBJECTS) {
        return;
    }

    if (!(flags & SYSTEM_FLAG_FREEZE_ANIMATION)) {
        state = g_world_map_dot_pulse_direction;
        flags = 0x40;
        if (state == 0) {
            counter = g_world_map_dot_pulse_phase + 1;
            g_world_map_dot_pulse_phase = counter;
            if (counter == flags) {
                g_world_map_dot_pulse_direction = state ^ 1;
            }
        } else {
            counter = g_world_map_dot_pulse_phase - 1;
            g_world_map_dot_pulse_phase = counter;
            if (counter == 0) {
                g_world_map_dot_pulse_direction = state ^ 1;
            }
        }
    }

    phase = g_world_map_dot_pulse_phase * 2;
    bright = phase + 0x60;
    dark = -0x20 - phase;
    for (i = 0; i < g_world_map_dot_count; i++) {
        if (!(g_world_map_dots[i].render.flags & DISPLAY_OBJECT_HIDDEN)) {
            /* Direct nested access biases the base to render; retail addresses the color column. */
            color = (CVECTOR*)((u8*)&g_world_map_dots[i] + offsetof(world_map_dot_t, render.color));
            if (g_world_map_dots[i].render.palette != 0) {
                color->r = bright;
                color->g = bright;
                color->b = bright;
            } else {
                color->r = dark;
                color->g = dark;
                color->b = dark;
            }
        }
    }
}

/* Pick the visible map dot with the lowest priority value whose screen
 * position contains the given point, returned as a one-based index (0 when
 * nothing is hit).
 *
 * The point is passed by value in a0/a1; the target reads both members back
 * from the incoming argument home slots. */
s32 world_map_find_dot_at(point32_t point) {
    s32 best = 0xFFFFFF;
    s32 i;
    s32 hit = 0;
    s32 value;

    for (i = 0; i < g_world_map_dot_count; i++) {
        if (script_get_variable(i + EVENT_SCRIPT_VAR_LOCATION_VISIBLE_FIRST) == 0) {
            continue;
        }
        if ((g_world_map_dots[i].render.flags & (DISPLAY_OBJECT_BLANK | DISPLAY_OBJECT_HIDDEN)) != 0) {
            continue;
        }
        value = g_world_map_dots[i].render.position.x;
        if (point.x < value - 10 || value + 10 < point.x) {
            continue;
        }
        value = g_world_map_dots[i].render.position.y;
        if (point.y < value - 14 || value + 6 < point.y) {
            continue;
        }
        value = g_world_map_dots[i].render.priority;
        if (value < best) {
            hit = i + 1;
            best = value;
        }
    }
    return hit;
}

/* Report the cursor step needed to walk the given point onto the map dot it
 * currently overlaps, clamped to +/-2 per axis. Returns the one-based dot
 * index, or 0 when the point hits nothing.
 *
 * The point is passed by value in a0/a1; the target reads both members back
 * from the incoming argument home slots. */
s32 world_map_get_dot_snap_step(point32_t point, s32* step_x, s32* step_y) {
    s32 hit;
    s32 dot_x;
    s32 dot_y;

    *step_x = 0;
    *step_y = 0;
    hit = world_map_find_dot_at(point);
    if (hit != 0) {
        dot_x = g_world_map_dots[hit - 1].render.position.x;
        dot_y = g_world_map_dots[hit - 1].render.position.y - 4;
        if (dot_x < point.x) {
            if (dot_x > point.x - 2) {
                *step_x = dot_x - point.x;
            } else {
                *step_x = -2;
            }
        }
        if (point.x < dot_x) {
            if (dot_x < point.x + 2) {
                *step_x = dot_x - point.x;
            } else {
                *step_x = 2;
            }
        }
        if (dot_y < point.y) {
            if (dot_y > point.y - 2) {
                *step_y = dot_y - point.y;
            } else {
                *step_y = -2;
            }
        }
        if (point.y < dot_y) {
            if (dot_y < point.y + 2) {
                *step_y = dot_y - point.y;
            } else {
                *step_y = 2;
            }
        }
    }
    return hit;
}

/* Fills the caller's panel entry buffer with the menu entry ids offered at a
 * world map location and returns how many were written; the two callers store
 * the result as the panel's entry_count and pass the buffer as entries.
 *
 * The Deep Dungeon offers its floors (TEXT_ID_DEEP_DUNGEON_FLOOR_FIRST
 * + i), one more than EVENT_SCRIPT_VAR_DEEP_DUNGEON_LIST_LENGTH. A town offers
 * the bar, shop and soldier office, and the fur shop as a fourth entry at
 * Dorter, Warjilis and Zarghidas once EVENT_SCRIPT_VAR_FUR_SHOP_ENABLED is
 * set. Other locations offer nothing.
 */
s32 location_build_menu_entries(s32 location_id, s32* out) {
    s32 count;
    s32 i;

    if (location_id == LOCATION_DEEP_DUNGEON) {
        count = script_get_variable(EVENT_SCRIPT_VAR_DEEP_DUNGEON_LIST_LENGTH) + 1;
        for (i = 0; i < count; i++) {
            out[i] = TEXT_ID_DEEP_DUNGEON_FLOOR_FIRST + i;
        }
        return count;
    }

    count = 0;
    if (g_location_records[location_id].is_town == 1) {
        count = 3;
        out[0] = TEXT_ID_BAR;
        out[1] = TEXT_ID_SHOP;
        out[2] = TEXT_ID_SOLDIER_OFFICE;
        if (script_get_variable(EVENT_SCRIPT_VAR_FUR_SHOP_ENABLED) != 0) {
            if ((location_id == LOCATION_DORTER) || (location_id == LOCATION_WARJILIS)
                || (location_id == LOCATION_ZARGHIDAS)) {
                out[count] = TEXT_ID_FUR_SHOP;
                count++;
            }
        }
    }
    return count;
}

/* Fill `out` with the map-name text ids of the current location's Choice destinations and return
 * how many: -1 at Murond Death City, 0 when the location's color variable is set or no Choice
 * applies; a single destination is taken as is, otherwise up to five whose hidden flag is clear. */
static s32 location_collect_choices(s32 location_id, s32* out) {
    s32 available;
    s32 i;
    s32 written;
    s32 entry;

    if (location_id == LOCATION_MUROND_DEATH_CITY) {
        return -1;
    }
    if (script_get_variable(location_id + EVENT_SCRIPT_VAR_LOCATION_COLOR_FIRST) != 0) {
        return 0;
    }
    if (location_script_run(g_world_map_marker_state.marker.location_id, LOCATION_SCRIPT_ACTION_CHOICE) == 0) {
        return 0;
    }
    available = 0;
    for (i = 0; i < 4; i++) {
        if (g_location_script_state.choice_vars[i] == 0) {
            break;
        }
        available++;
    }
    if (available == 1) {
        entry = g_location_script_state.choice_vars[0];
        *out = entry + (TEXT_ID_MAP_NAME_BASE - 1);
        return 1;
    }
    written = 0;
    for (i = 0; i < available; i++) {
        entry = g_location_script_state.choice_vars[i];
        if (script_get_variable(
                g_location_choice_hidden_offsets[entry - 1] + EVENT_SCRIPT_VAR_LOCATION_CHOICE_HIDDEN_FIRST)
            == 0) {
            out[written++] = entry + (TEXT_ID_MAP_NAME_BASE - 1);
            if (written == 5) {
                break;
            }
        }
    }
    return written;
}

/* Empty. world_map_init_subsystems calls it between drawing the map dots and placing the location marker. */
void world_map_init_stub(void) {
}

/* Draws the visible world-map routes into the auxiliary ordering table as
 * textured semi-transparent ribbons. Each route record is a run of packed
 * (x, y) pairs holding the two edges of the ribbon; every point is projected
 * through RotTransSV into one of four rotating vertex slots and flagged
 * in-bounds when x + 127 and y + 111 fall inside 255 x 227. Once four slots
 * are full they become one POLY_FT4 quad, emitted when either edge-leading
 * vertex was in bounds, and the last pair slides down to slots 0 and 1 so the
 * next pair closes the following segment. Nothing is drawn while
 * SYSTEM_FLAG_HIDE_MAP_ROUTES is set, and a route only once it is
 * revealed (EVENT_SCRIPT_VAR_PATH_VISIBLE_FIRST + route).
 *
 * The count is sign-extended into count_tmp, which the loop guard tests before
 * the copy into count; the `j < count_tmp` guard leaves an unused compare
 * pseudo whose stack slot is the target's otherwise empty 32-bit stack slot at 0x88,
 * and taking `vertex` in the loop header numbers the vertex-base pseudo ahead
 * of it so the base spills to 0x80 as in the target. The 2-byte alignment of
 * SVECTOR makes `verts[0] = verts[2]` take GCC's unaligned block-move path
 * (lwl/lwr, swl/swr) as in the target. */
void world_map_draw_routes(GsOT* ot) {
    s32 flags[4];
    SVECTOR verts[4];
    SVECTOR in;
    POLY_FT4 poly;
    s32 projected;
    u16** table;
    u16* points;
    s32 count;
    s32 i;
    s32 j;
    s16 slot;
    SVECTOR* vertex;
    s16 raw;
    s32 count_tmp;

    if (g_system_flags & SYSTEM_FLAG_HIDE_MAP_ROUTES) {
        return;
    }

    table = g_world_map_route_polylines;
    SetPolyFT4(&poly);
    SetSemiTrans(&poly, 1);
    setUV4(&poly, 0x88, 0xE0, 0x97, 0xE0, 0x88, 0xEF, 0x97, 0xEF);
    setRGB0(&poly, g_world_map_brightness_rgb.r, g_world_map_brightness_rgb.g, g_world_map_brightness_rgb.b);
    poly.tpage = GetTPage(0, 0, 0x1C0, 0x100);
    poly.clut = GetClut(0x10, 0x1E8);

    for (i = 0, vertex = verts; i < WORLD_MAP_ROUTE_COUNT; i++) {
        if (script_get_variable(i + EVENT_SCRIPT_VAR_PATH_VISIBLE_FIRST) == 0) {
            continue;
        }
        slot = 0;
        points = table[i];
        raw = *points++;
        count_tmp = raw;
        in.vz = 0;
        j = 0;
        if (j < count_tmp) {
            count = count_tmp;
            do {
                in.vx = *points++;
                in.vy = *points++;
                RotTransSV(&in, &vertex[slot], &projected);
                if (vertex[slot].vx >= -127 && vertex[slot].vx <= 127 && vertex[slot].vy >= -111
                    && vertex[slot].vy <= 115) {
                    flags[slot] = 1;
                } else {
                    flags[slot] = 0;
                }
                slot++;
                if (slot == 4) {
                    poly.x0 = verts[0].vx;
                    poly.y0 = verts[0].vy;
                    poly.x1 = verts[1].vx;
                    poly.y1 = verts[1].vy;
                    poly.x2 = verts[2].vx;
                    slot -= 2;
                    poly.y2 = verts[2].vy;
                    poly.x3 = verts[3].vx;
                    poly.y3 = verts[3].vy;
                    verts[0] = verts[2];
                    verts[1] = verts[3];
                    if (flags[0] != 0 || flags[2] != 0) {
                        GsSortPoly(&poly, ot, 1);
                    }
                    flags[0] = flags[2];
                    flags[1] = flags[3];
                }
                j++;
            } while (j < count);
        }
    }
}

/* Stages one world-map route segment in the location-entry state.
 *
 * The low byte of `route` is the route id; its bit 8 becomes flags bit 1 and
 * selects the direction, and `step` is the point to start from: the
 * descending arm counts back from the end of the record and takes the heading
 * (turned by 0x800) and distance from the preceding point; the ascending arm
 * takes them from the point itself. Returns the point count minus one.
 *
 * The unused 32-byte local reproduces the target's otherwise empty 0x20 frame.
 * `index` is shared by
 * both arms while each arm has its own point pointer, which fixes the
 * target's register choice for both index computations. The packed 32-bit position values supply x in the low
 * 16 bits and y in the high 16 bits; the `flags` local gives the
 * target's `or` operand order. */
static s32 travel_begin_segment(travel_t* state, u32 route, s32 step) {
    s32 unused[8];
    world_map_route_t* record;
    s32 index;
    world_map_route_point_t* point_down;
    world_map_route_point_t* point_up;
    s32 word;
    s32 flags;

    /* Direct indexing or separate typed locals move the table load after the route-ID store. */
    record = (world_map_route_t*)g_world_map_routes;
    state->route_id = route & TRAVEL_PATH_ROUTE_MASK;
    flags = state->flags;
    state->flags = flags | ((route >> 7) & TRAVEL_REVERSED);
    record = ((world_map_route_t**)record)[state->route_id];
    if (state->flags & TRAVEL_REVERSED) {
        state->leg_end_location = record->start_location;
        state->leg_start_location = record->end_location;
        /* Direct point indexing moves the header addition past the shift. */
        index = (record->count - step - 1) * (sizeof(world_map_route_point_t) / sizeof(u32))
            + offsetof(world_map_route_t, points) / sizeof(u32);
        point_down = (world_map_route_point_t*)(index * sizeof(u32) + (s32)record);
        word = point_down->position;
        state->position.vx = word;
        state->position.vy = (point_down->position >> 16);
        state->position.vz = 0;
        word = point_down[-1].position;
        state->target_position.vx = word;
        state->target_position.vy = (point_down[-1].position >> 16);
        state->target_position.vz = 0;
        state->previous_position = state->position;
        state->heading = ((point_down[-1].heading >> 16) + 0x800) & 0xfff;
        state->segment_frames = point_down[-1].distance * 2;
    } else {
        state->leg_start_location = record->start_location;
        state->leg_end_location = record->end_location;
        index = step * (sizeof(world_map_route_point_t) / sizeof(u32))
            + offsetof(world_map_route_t, points) / sizeof(u32);
        point_up = (world_map_route_point_t*)(index * sizeof(u32) + (s32)record);
        word = point_up->position;
        state->position.vx = word;
        state->position.vy = (point_up->position >> 16);
        state->position.vz = 0;
        word = point_up[1].position;
        state->target_position.vx = word;
        state->target_position.vy = (point_up[1].position >> 16);
        state->target_position.vz = 0;
        state->previous_position = state->position;
        state->heading = (point_up->heading >> 16);
        state->segment_frames = point_up->distance * 2;
    }

    state->segment_frame = 0;
    return record->count - 1;
}

/* Loads the route joining locations from and to (in either direction) into
 * g_world_map_path_animation: copies its polyline, reversed when the record
 * runs from `to` to `from`, and times each segment by its length.
 *
 * `data` holds the polyline table and then the polyline itself, as the target
 * keeps both in one register. The header's second endpoint is read from the
 * whole 32-bit header value, as the target does. The segment differences read the
 * start point first: that makes the start of the point array the loop's base
 * register, from which the other three coordinates are addressed. The unused
 * local reproduces the target's frame size. */
void world_map_prepare_path_animation(s32 from, s32 to) {
    world_map_route_t** records;
    u16* data;
    world_map_route_t* record;
    s32 unused[2];
    s32 first;
    s32 second;
    s32 dx;
    s32 dy;
    s32 i;

    records = g_world_map_routes;
    g_world_map_path_animation.flags = 0;
    for (i = 0; i < WORLD_MAP_ROUTE_COUNT; i++) {
        record = *records;
        first = record->start_location;
        second = (*(u32*)record & 0xFF0000) >> 16;
        if (first == from && second == to) {
            g_world_map_path_animation.route_id = i;
            break;
        }
        if (second == from && first == to) {
            g_world_map_path_animation.route_id = i;
            g_world_map_path_animation.flags |= WORLD_MAP_PATH_ANIMATION_REVERSED;
            break;
        }
        records++;
    }

    if (i == WORLD_MAP_ROUTE_COUNT) {
        return;
    }

    g_world_map_path_animation.flags |= WORLD_MAP_PATH_ANIMATION_RUNNING;
    first = record->count - 1;
    g_world_map_path_animation.frame_timer = 0;
    g_world_map_path_animation.segment_index = 0;
    g_world_map_path_animation.segment_count = first;
    data = (u16*)g_world_map_route_polylines;
    data = ((u16**)data)[g_world_map_path_animation.route_id];
    g_world_map_path_animation.point_count = *data;
    data++;
    for (i = 0; i < g_world_map_path_animation.point_count; i++) {
        if (g_world_map_path_animation.flags & WORLD_MAP_PATH_ANIMATION_REVERSED) {
            g_world_map_path_animation.coords[i * 2] = data[(g_world_map_path_animation.point_count - i - 1) * 2];
            g_world_map_path_animation.coords[i * 2 + 1]
                = data[(g_world_map_path_animation.point_count - i - 1) * 2 + 1];
        } else {
            g_world_map_path_animation.coords[i * 2] = data[i * 2];
            g_world_map_path_animation.coords[i * 2 + 1] = data[i * 2 + 1];
        }
    }

    for (i = 0; i < g_world_map_path_animation.segment_count; i++) {
        dx = -g_world_map_path_animation.coords[i * 4] + g_world_map_path_animation.coords[i * 4 + 4];
        dy = -g_world_map_path_animation.coords[i * 4 + 1] + g_world_map_path_animation.coords[i * 4 + 5];
        g_world_map_path_animation.segment_frames[i] = csqrt(dx * dx + dy * dy) >> 5;
    }
}

/* Draws the route segments unrolled so far as textured semi-transparent quads:
 * every segment up to segment_index is projected from its four outline points, and
 * the current segment's far edge is interpolated by frame_timer / segment_frames so
 * it unrolls over the segment's time. A negative segment_index or one equal
 * to segment_count stops the animation instead. */
void world_map_draw_path_animation(GsOT* ot) {
    SVECTOR verts[4];
    SVECTOR in;
    POLY_FT4 poly;
    s32 projected;
    s32 i;
    s32 j;
    s32 frames;

    if (!(g_world_map_path_animation.flags & WORLD_MAP_PATH_ANIMATION_RUNNING)) {
        return;
    }

    SetPolyFT4(&poly);
    SetSemiTrans(&poly, 1);
    setUV4(&poly, 0x88, 0xE0, 0x97, 0xE0, 0x88, 0xEF, 0x97, 0xEF);
    setRGB0(&poly, g_world_map_brightness_rgb.r, g_world_map_brightness_rgb.g, g_world_map_brightness_rgb.b);
    poly.tpage = GetTPage(0, 0, 0x1C0, 0x100);
    poly.clut = GetClut(0x10, 0x1E8);
    if (g_world_map_path_animation.segment_index < 0
        || g_world_map_path_animation.segment_index == g_world_map_path_animation.segment_count) {
        g_world_map_path_animation.flags &= ~WORLD_MAP_PATH_ANIMATION_RUNNING;
        return;
    }

    for (i = 0; i < g_world_map_path_animation.segment_index + 1; i++) {
        if (i == g_world_map_path_animation.segment_index) {
            frames = g_world_map_path_animation.frame_timer;
        } else {
            frames = g_world_map_path_animation.segment_frames[i];
        }
        for (j = 0; j < 4; j++) {
            in.vx = g_world_map_path_animation.coords[i * 4 + j * 2];
            in.vy = g_world_map_path_animation.coords[i * 4 + j * 2 + 1];
            in.vz = 0;
            RotTransSV(&in, &verts[j], &projected);
        }
        if (frames != g_world_map_path_animation.segment_frames[i]) {
            verts[2].vx
                = verts[0].vx + (verts[2].vx - verts[0].vx) * frames / g_world_map_path_animation.segment_frames[i];
            verts[2].vy
                = verts[0].vy + (verts[2].vy - verts[0].vy) * frames / g_world_map_path_animation.segment_frames[i];
            verts[3].vx
                = verts[1].vx + (verts[3].vx - verts[1].vx) * frames / g_world_map_path_animation.segment_frames[i];
            verts[3].vy
                = verts[1].vy + (verts[3].vy - verts[1].vy) * frames / g_world_map_path_animation.segment_frames[i];
        }
        setXY4(&poly, verts[0].vx, verts[0].vy, verts[1].vx, verts[1].vy, verts[2].vx, verts[2].vy, verts[3].vx,
            verts[3].vy);
        GsSortPoly(&poly, ot, 1);
    }
}

/* Depth-first search for the cheapest route path between two locations.
 *
 * Tries every revealed route (EVENT_SCRIPT_VAR_PATH_VISIBLE_FIRST + route) not already on
 * the path; a route joining from and to completes the path, which is kept
 * when it is shorter, or as long but no more expensive, than the best one.
 * Other routes leaving from recurse from their far end. The frame is kept
 * responsive while searching: past 200 vsyncs the world frame is redrawn.
 *
 * `unused` keeps 16 frame bytes the target reserves and never touches. */
static void travel_find_best_path(s32 from, s32 to) {
    s32 unused[4];
    s32 cost;
    s32 route;
    world_map_route_t** table;
    s32 i;
    world_map_route_t* record;
    s32 start;
    s32 end;
    s32 length;
    s32 buffer_index;

    if (g_travel_search_depth >= TRAVEL_PATH_CAPACITY) {
        return;
    }

    table = g_world_map_routes;
    for (route = 0; route < WORLD_MAP_ROUTE_COUNT; route++) {
        if (VSync(1) > 200) {
            file_poll_load(&g_file_cd_state);
            audio_queue_process();
            world_map_pulse_dots();
            world_map_draw_frame();
            buffer_index = GsGetActiveBuff();
            g_gfx_active_buffer_index = buffer_index;
            GsSetWorkBase(g_world_map_primitive_buffers + buffer_index * 0xE000);
        }
        if (script_get_variable(route + EVENT_SCRIPT_VAR_PATH_VISIBLE_FIRST) == 0) {
            continue;
        }
        for (i = 0; i < g_travel_search_depth; i++) {
            if ((g_travel_search_path.routes[i] & TRAVEL_PATH_ROUTE_MASK) == route) {
                break;
            }
        }
        if (i < g_travel_search_depth) {
            continue;
        }
        record = table[route];
        length = g_travel_search_depth + 1;
        start = record->start_location;
        end = record->end_location;
        cost = record->cost;
        if (g_travel.best_length_bound < length) {
            continue;
        }
        if (length == g_travel.best_length_bound && g_travel.best_cost < cost + g_travel_search_cost) {
            continue;
        }
        if (from == start && to == end) {
            g_travel_search_path.routes[g_travel_search_depth] = route;
            /* Both directions record the path through one copy of this
             * code (a second copy loop could not be cross-jumped away). */
            goto found;
        }
        if (to == start && from == end) {
            g_travel_search_path.routes[g_travel_search_depth] = route | TRAVEL_PATH_REVERSED;
        found:
            g_travel.route_path = g_travel_search_path;
            g_travel.best_cost = g_travel_search_cost + cost;
            g_travel.best_length_bound = g_travel.best_path_length = g_travel_search_depth + 1;
            g_travel.flags |= TRAVEL_ACTIVE;
            continue;
        }
        if (from == start) {
            g_travel_search_path.routes[g_travel_search_depth] = route;
            g_travel_search_depth++;
            g_travel_search_cost += cost;
            travel_find_best_path(end, to);
            g_travel_search_depth--;
            g_travel_search_cost -= cost;
        } else if (from == end) {
            g_travel_search_path.routes[g_travel_search_depth] = route | TRAVEL_PATH_REVERSED;
            g_travel_search_depth++;
            g_travel_search_cost += cost;
            travel_find_best_path(start, to);
            g_travel_search_depth--;
            g_travel_search_cost -= cost;
        }
    }
}

/* Start travel from from_location to to_location. When the current location's
 * map script stages LoadEvent, that event becomes
 * EVENT_SCRIPT_VAR_CURRENT_EVENT and the event transition level is pushed;
 * returns 0. Otherwise a staged SetVar2 is applied and
 * travel_find_best_path searches the cheapest path; when there is
 * one, its first leg starts: the marker turns to the leg's heading, the menu
 * window is hidden, the view pans when the marker is off screen and the
 * destination's picture starts loading. Returns 1.
 *
 * g_travel_search_depth is a separate scalar: as a field of the entry state it becomes
 * the address anchor that the later &g_travel argument is formed from, and
 * the chained 99999 store makes best_cost that anchor as in the target. The
 * location load and the two argument addresses share one base register in the
 * target, which needs them to be members of the one g_world_map_marker_state record. */
s32 travel_start(s32 from_location, s32 to_location) {
    s32 depth;

    if (location_script_run(g_world_map_marker_state.marker.location_id, LOCATION_SCRIPT_ACTION_LOAD_EVENT) != 0) {
        g_window_records[g_menu_stack_records[g_menu_stack_depth - 1].window_index].flags |= DISPLAY_OBJECT_HIDDEN;
        g_travel_next_map_id = 0;
        script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, g_location_script_state.args[0]);
        event_transition_push_level(g_location_script_state.args[1]);
        g_system_flags |= SYSTEM_FLAG_HIDE_LOCATION_LABEL;
        return 0;
    } else {
        if (location_script_run(g_world_map_marker_state.marker.location_id, LOCATION_SCRIPT_ACTION_SET_VAR_2) != 0) {
            script_set_variable(g_location_script_state.args[0], g_location_script_state.args[1]);
        }
        g_travel_search_cost = 0;
        g_travel_search_depth = 0;
        g_travel.best_cost = g_travel.best_length_bound = 99999;
        g_travel.flags = 0;
        travel_find_best_path(from_location, to_location);
        if (!(g_travel.flags & TRAVEL_ACTIVE)) {
            return 1;
        }
        g_travel.route_index = 0;
        if (g_travel.best_path_length == 0) {
            return 1;
        }
        g_travel.segment_count = travel_begin_segment(&g_travel, g_travel.route_path.routes[0], 0);
        g_travel.segment_index = 0;
        g_world_map_marker_state.angle = g_travel.heading;
        depth = g_menu_stack_depth - 1;
        g_travel.menu_level = depth;
        g_window_records[g_menu_stack_records[depth].window_index].flags |= DISPLAY_OBJECT_HIDDEN;
        if (world_map_start_pan_if_outside(
                &g_world_map_marker_state.marker.render.position, &g_world_map_marker_state.marker.position)
            == 0) {
            g_world_map_marker_state.flags
                |= WORLD_MAP_MARKER_UPDATE_DIRECTION | WORLD_MAP_MARKER_ALTERNATE_DIRECTION_SET;
        }
        g_world_map_marker_state.marker.picture_id = location_load_picture(to_location);
        g_system_flags |= SYSTEM_FLAG_HIDE_LOCATION_LABEL;
        return 1;
    }
}

/* Advances the world-map movement along the current location route.
 *
 * Each frame interpolates the view coordinates between the step's start and
 * end vertices and scrolls the projection. When a step's frames are used up
 * it starts the next step, or on the last step advances the calendar day,
 * evaluates the arrival conditionals (event, SetVar, random encounter) and
 * either continues to the next route leg or finishes at the location.
 *
 * The two event-push tails are written out in full: GCC 2.7.2 cross-jumps
 * them into the single shared tail the target has, and only per-path CSE of
 * each copy keeps the target's direct g_world_map_marker_state accesses and the &state
 * base register in the flags update. */
void travel_step(void) {
    point32_t delta;
    SVECTOR previous;
    s32 dx;
    s32 dy;
    s32 result;

    if (!(g_travel.flags & TRAVEL_ACTIVE)) {
        return;
    }

    if (g_world_map_pan.flags & WORLD_MAP_PAN_ACTIVE) {
        if (world_map_step_pan() == 0) {
            g_world_map_marker_state.flags
                |= WORLD_MAP_MARKER_UPDATE_DIRECTION | WORLD_MAP_MARKER_ALTERNATE_DIRECTION_SET;
        }
        g_world_map_marker_state.flags |= WORLD_MAP_MARKER_UPDATE_POSITION;
        return;
    }

    if (g_travel.segment_frame >= g_travel.segment_frames) {
        if (++g_travel.segment_index >= g_travel.segment_count) {
            calendar_advance_day();
            g_world_map_marker_state.marker.position = g_travel.target_position;
            g_world_map_marker_state.flags |= WORLD_MAP_MARKER_UPDATE_POSITION | WORLD_MAP_MARKER_UPDATE_DIRECTION;
            g_world_map_marker_state.marker.location_id = g_travel.leg_end_location;
            script_set_variable(EVENT_SCRIPT_VAR_LOCATION, g_travel.leg_end_location);
            g_travel.route_index++;
            if (location_script_run(g_world_map_marker_state.marker.location_id, LOCATION_SCRIPT_ACTION_LOAD_EVENT)
                != 0) {
                g_travel_next_map_id = 0;
                script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, g_location_script_state.args[0]);
                event_transition_push_level(g_location_script_state.args[1]);
                g_world_map_marker_state.angle = 0;
                g_world_map_marker_state.flags &= ~WORLD_MAP_MARKER_ALTERNATE_DIRECTION_SET;
                g_travel.flags &= ~(TRAVEL_ACTIVE | TRAVEL_REVERSED);
                g_menu_stack_records[g_travel.menu_level].location_transition.location_id
                    = g_world_map_marker_state.marker.location_id;
                return;
            }
            if (location_script_run(g_world_map_marker_state.marker.location_id, LOCATION_SCRIPT_ACTION_SET_VAR_2)
                != 0) {
                script_set_variable(g_location_script_state.args[0], g_location_script_state.args[1]);
            }
            if (random_battle_roll(g_travel.leg_end_location,
                    g_travel.route_path.routes[g_travel.route_index - 1] & TRAVEL_PATH_ROUTE_MASK)
                != 0) {
                g_system_flags |= SYSTEM_FLAG_RANDOM_BATTLE;
                g_travel_next_map_id = g_random_battle_map_id;
                script_set_variable(EVENT_SCRIPT_VAR_CURRENT_ENTD, g_random_battle_entd_id);
                script_set_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_SQUAD_COUNT, 1);
                script_set_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_PRIMARY_SQUAD_ID, g_random_battle_squad_id);
                script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, 0);
                event_transition_push_level(EVENT_TRANSITION_MODE_SPIN);
                g_world_map_marker_state.angle = 0;
                g_world_map_marker_state.flags &= ~WORLD_MAP_MARKER_ALTERNATE_DIRECTION_SET;
                g_travel.flags &= ~(TRAVEL_ACTIVE | TRAVEL_REVERSED);
                g_menu_stack_records[g_travel.menu_level].location_transition.location_id
                    = g_world_map_marker_state.marker.location_id;
                return;
            }
            sound_play_effect(SOUND_SFX_WORLD_MAP_ARRIVE);
            if (g_travel.route_index < g_travel.best_path_length) {
                g_travel.flags &= ~TRAVEL_REVERSED;
                g_travel.segment_count
                    = travel_begin_segment(&g_travel, g_travel.route_path.routes[g_travel.route_index], 0);
                g_travel.segment_index = 0;
                g_world_map_marker_state.angle = g_travel.heading;
                return;
            }
            g_window_records[g_menu_stack_records[g_travel.menu_level].location_transition.window_index].flags
                ^= DISPLAY_OBJECT_HIDDEN;
            g_window_records[g_menu_stack_records[g_travel.menu_level].location_transition.window_index].position.x
                = g_world_map_marker_state.marker.render.position.x;
            g_window_records[g_menu_stack_records[g_travel.menu_level].location_transition.window_index].position.y
                = g_world_map_marker_state.marker.render.position.y - 4;
            g_system_flags &= ~SYSTEM_FLAG_HIDE_LOCATION_LABEL;
            g_travel.arrival_location = g_world_map_marker_state.marker.location_id + 1;
            file_wait_for_pending();
            g_world_map_marker_state.angle = 0;
            g_world_map_marker_state.flags &= ~WORLD_MAP_MARKER_ALTERNATE_DIRECTION_SET;
            g_travel.flags &= ~(TRAVEL_ACTIVE | TRAVEL_REVERSED);
            g_menu_stack_records[g_travel.menu_level].location_transition.location_id
                = g_world_map_marker_state.marker.location_id;
            result = location_collect_choices(g_world_map_marker_state.marker.location_id,
                g_menu_stack_records[g_menu_stack_depth].location_transition.choice_text_ids);
            if (result != -1 && result > 0) {
                g_menu_stack_records[g_menu_stack_depth].location_transition.pending_result = result;
                menu_stack_dispatch_suspend();
                location_push_menu_level(g_world_map_marker_state.marker.location_id);
            }
            return;
        } else {
            travel_begin_segment(&g_travel, g_travel.route_id, g_travel.segment_index);
            g_world_map_marker_state.angle = g_travel.heading;
            g_world_map_marker_state.flags |= WORLD_MAP_MARKER_UPDATE_DIRECTION;
        }
    } else {
        g_travel.segment_frame++;
    }

    dx = (g_travel.target_position.vx - g_travel.position.vx) * g_travel.segment_frame / g_travel.segment_frames;
    dy = (g_travel.target_position.vy - g_travel.position.vy) * g_travel.segment_frame / g_travel.segment_frames;
    previous = g_world_map_marker_state.marker.position;
    delta.x = g_travel.previous_position.vx + dx;
    g_world_map_marker_state.marker.position.vx = delta.x;
    delta.y = g_travel.previous_position.vy + dy;
    g_world_map_marker_state.marker.position.vy = delta.y;
    delta.x -= previous.vx;
    delta.y -= previous.vy;
    world_map_scroll_if_near_edge(&delta);
    g_world_map_marker_state.flags |= WORLD_MAP_MARKER_UPDATE_POSITION;
}

/* Start a projection move (a camera pan) toward a point outside the camera bounds.
 *
 * The motion record snapshots the current origin and stores the target
 * distance in the same 8-bit fractional scale used by its progress field. */
s32 world_map_start_pan_if_outside(const point32_t* point, const SVECTOR* coordinates) {
    s32 dx;
    s32 dy;

    if (world_map_is_outside_pan_bounds(point) == 0) {
        return 0;
    }

    world_map_negate_and_clamp(coordinates, &g_world_map_pan.target);
    g_world_map_pan.flags |= WORLD_MAP_PAN_ACTIVE;
    g_world_map_pan.origin = g_world_map_projection_view.origin;

    dx = g_world_map_pan.target.vx - g_world_map_pan.origin.vx;
    dy = g_world_map_pan.target.vy - g_world_map_pan.origin.vy;
    g_world_map_pan.progress = 0;
    g_world_map_pan.distance = csqrt(dx * dx + dy * dy) >> 8;
    return 1;
}

/* Whether a point lies outside the view far enough to pan the camera: beyond an
 * edge while the projection origin still has more than 0x20 of scroll room
 * toward it. Nothing wraps; at the scroll limit the point counts as inside. */
s32 world_map_is_outside_pan_bounds(const point32_t* point) {
    if ((point->x < -0x40 && 0x80 - g_world_map_projection_view.origin.vx >= 0x21)
        || (point->x >= 0x49 && g_world_map_projection_view.origin.vx + 0x74 >= 0x21)) {
        return 1;
    }
    if ((point->y < -0x40 && 0x50 - g_world_map_projection_view.origin.vy >= 0x21)
        || (point->y >= 0x41 && g_world_map_projection_view.origin.vy + 0x40 >= 0x21)) {
        return 1;
    }
    return 0;
}

/* Negate and clamp a signed coordinate pair to the world-scroll limits. */
static void world_map_negate_and_clamp(const SVECTOR* input, VECTOR* output) {
    s32 x = -input->vx;
    s32 y = -input->vy;

    if (x < WORLD_MAP_SCROLL_MIN_X) {
        x = WORLD_MAP_SCROLL_MIN_X;
    }
    if (x > WORLD_MAP_SCROLL_MAX_X) {
        x = WORLD_MAP_SCROLL_MAX_X;
    }
    if (y < WORLD_MAP_SCROLL_MIN_Y) {
        y = WORLD_MAP_SCROLL_MIN_Y;
    }
    if (y > WORLD_MAP_SCROLL_MAX_Y) {
        y = WORLD_MAP_SCROLL_MAX_Y;
    }
    output->vx = x;
    output->vy = y;
}

/* Separate addresses for two fields of g_world_map_pan: using the record's
 * progress reschedules loads, and using target.vx shortens this body. */
extern s32 g_world_map_pan_progress;
extern s32 g_world_map_pan_target_x;

/* Advance one frame of the camera pan started by
 * world_map_start_pan_if_outside.
 *
 * Interpolates the live projection origin between the snapshotted origin and
 * the clamped target by progress/distance. While progress is still short of
 * distance it is advanced and 1 is returned. On the final frame the origin is
 * snapped to the target, the record's active flag is toggled off, and 0 is
 * returned.
 *
 * The final snap copies the target VECTOR's four 32-bit fields. */
s32 world_map_step_pan(void) {
    s32 progress;
    s32 distance;
    s32 target_x = g_world_map_pan_target_x;
    s32 origin_x = g_world_map_pan.origin.vx;
    s32 delta_x;
    s32 origin_y;

    progress = g_world_map_pan_progress;
    distance = g_world_map_pan.distance;
    target_x = (target_x - origin_x) * progress;
    delta_x = target_x / distance;
    g_world_map_projection_view.origin.vx = origin_x + delta_x;
    origin_y = g_world_map_pan.origin.vy;
    g_world_map_projection_view.origin.vy = origin_y + (g_world_map_pan.target.vy - origin_y) * progress / distance;
    if (progress >= distance) {
        g_world_map_projection_view.origin = g_world_map_pan.target;
        g_world_map_pan.flags ^= WORLD_MAP_PAN_ACTIVE;
        return 0;
    }
    g_world_map_pan.progress = progress + 1;
    return 1;
}

/* Applies a scroll delta to the projection origin when the resulting window
 * origin would leave [-64, 72] horizontally or [-64, 64] vertically, and
 * sets SYSTEM_FLAG_DRAW_MAP if the origin's x or y changed. */
static void world_map_scroll_if_near_edge(point32_t* delta) {
    point32_t position;
    VECTOR saved = g_world_map_projection_view.origin;

    position.x = g_world_map_marker_state.marker.render.position.x + delta->x;
    position.y = g_world_map_marker_state.marker.render.position.y + delta->y;
    if ((delta->x < 0 && position.x < -0x40) || (delta->x > 0 && position.x >= 0x49)) {
        world_map_apply_clamped_horizontal_scroll(&delta->x, &g_world_map_projection_view.origin.vx);
    }
    if ((delta->y < 0 && position.y < -0x40) || (delta->y > 0 && position.y >= 0x41)) {
        world_map_apply_clamped_vertical_scroll(delta, &g_world_map_projection_view.origin);
    }
    if (saved.vx != g_world_map_projection_view.origin.vx || saved.vy != g_world_map_projection_view.origin.vy) {
        g_system_flags |= SYSTEM_FLAG_DRAW_MAP;
    }
}

#define ENTRY_AT(byte_offset)   ((random_battle_road_t*)((byte_offset) + (s32)base))
#define ENTRY_BASE(byte_offset) ((random_battle_road_t*)((s32)base + (byte_offset)))

/* Roll for a random battle on one road into world-map location 24..42.
 *
 * The tier is the first of the road's four whose progress threshold is at or
 * above the road's script variable. A tier chance of 0 never triggers;
 * otherwise a battle happens when a roll of 0..99 is at or below the chance,
 * which is chance + 1 in 100. One of the ENTDs enabled in the tier's mask is
 * then drawn evenly. Returns 1 with the ENTD and map ids set; the squad id is
 * set as soon as the road is found. */
static s32 random_battle_roll(s32 location, s32 route) {
    random_battle_road_t(*base)[3];
    s32 offset;
    random_battle_road_t* row;
    u8* tier_column;
    s32 progress;
    s32 chance;
    s32 mask;
    s32 count;
    s32 pick;

    location -= LOCATION_FIELD_FIRST;
    if (location < 0 || location >= LOCATION_FIELD_COUNT) {
        return 0;
    }

    base = g_random_battle_roads;
    offset = location * 72;
    for (location = 0; location < 3; location++) {
        if (ENTRY_BASE(offset + location * 24)->route == route) {
            break;
        }
    }

    if (location == 3) {
        return 0;
    }

    offset += location * 24;
    row = ENTRY_AT(offset);
    g_random_battle_squad_id = row->deployment_squad_id + 0x200;
    progress = script_get_variable(row->script_variable_id);
    /* Direct tier fields cache the row pointer and remove retail base additions. */
    for (location = 0; location < 4; location++) {
        if (((u8*)(offset + location * sizeof(random_battle_tier_t) + (s32)base))[offsetof(random_battle_road_t, tiers)
                + offsetof(random_battle_tier_t, progress_threshold)]
            >= progress) {
            break;
        }
    }

    if (location == 4) {
        return 0;
    }

    tier_column = (u8*)(location * sizeof(random_battle_tier_t) + offset + (s32)base);
    chance = tier_column[offsetof(random_battle_road_t, tiers) + offsetof(random_battle_tier_t, encounter_chance)];
    if (chance == 0) {
        return 0;
    }

    pick = rand() * 100 >> 15;
    if (chance >= pick) {
        mask = tier_column[offsetof(random_battle_road_t, tiers) + offsetof(random_battle_tier_t, encounter_mask)];
        count = 0;
        for (location = 0; location < 8; location++) {
            if ((mask >> location) & 1) {
                count++;
            }
        }
        pick = (rand() * count >> 15) + 1;
        for (location = 0; location < 8; location++) {
            if ((mask >> location) & 1) {
                pick--;
                if (pick == 0) {
                    break;
                }
            }
        }
        /* Typed indexing folds the base and index differently; derive the byte index from the last ENTD. */
        g_random_battle_entd_id = ((u8*)base)[offset + offsetof(random_battle_road_t, entd_ids[7]) - location];
        g_random_battle_map_id = ENTRY_AT(offset)->battle_map_id;
        return 1;
    }

    return 0;
}

/* Places the player's map marker on EVENT_SCRIPT_VAR_LOCATION, queues it for
 * drawing, picks its animation from EVENT_SCRIPT_VAR_TOWN_BACKGROUND, then
 * centers the projection on it and refreshes the camera.
 *
 * The map-dot position is copied as one SVECTOR, which reproduces the
 * target's unaligned lwl/lwr 32-bit transfers. */
void world_map_init_marker_and_camera(void) {
    VECTOR unused; /* unreferenced; reproduces the target's 0x38-byte frame */
    world_map_marker_t* marker;
    SVECTOR* position;
    s32 index = script_get_variable(EVENT_SCRIPT_VAR_LOCATION);
    s32 count;
    s32 value;

    /* Only the location ID, position and flags go through the pointer, as in the
     * target; the other fields are addressed through the symbol. */
    marker = &g_world_map_marker_state.marker;
    marker->location_id = index;
    position = &g_world_map_dots[index].position;
    marker->position = *position;
    g_world_map_marker_state.angle = 0;
    g_world_map_marker_state.flags = WORLD_MAP_MARKER_UPDATE_POSITION | WORLD_MAP_MARKER_UPDATE_DIRECTION;
    count = g_window_render_object_count;
    g_window_render_object_queue[count++] = &marker->render;
    marker->render.flags = DISPLAY_OBJECT_ANIMATED;
    g_world_map_marker_state.marker.render.priority = 0xE;
    g_window_render_object_count = count;
    g_world_map_marker_state.marker.render.anim_counter = 0;
    g_world_map_marker_state.marker.render.frame_index = 0;
    g_world_map_marker_state.marker.render.color.r = 0x80;
    g_world_map_marker_state.marker.render.color.g = 0x80;
    g_world_map_marker_state.marker.render.color.b = 0x80;
    value = script_get_variable(EVENT_SCRIPT_VAR_TOWN_BACKGROUND);
    g_world_map_marker_state.marker.render.palette = 0;
    if (value == 1) {
        g_world_map_marker_state.marker.render.palette = 2;
    }
    if (value == 2) {
        g_world_map_marker_state.marker.render.palette = 3;
    }
    marker->position = g_world_map_dots[marker->location_id].position;
    world_map_negate_and_clamp(&g_world_map_marker_state.marker.position, &g_world_map_projection_view.origin);
    world_map_project_tiles();
    world_map_update_marker();
}

/* Apply pending direction and GTE projection updates to the player marker.
 *
 * The 32-bit direction value is the player marker's sequence: the facing octant picks
 * one of eight marker sequences from WINDOW_ANIM_MARKER_FIRST, or from
 * WINDOW_ANIM_MARKER_ALTERNATE_FIRST for the alternate set. The
 * marker's position is projected with RotTrans into its render position. */
void world_map_update_marker(void) {
    VECTOR output;
    world_map_marker_state_t* state = &g_world_map_marker_state;
    long transform_flag;
    s32 angle_direction;
    s32 direction;
    s32 flags;

    flags = state->flags;
    if (flags & WORLD_MAP_MARKER_UPDATE_DIRECTION) {
        angle_direction = ((state->angle + 0x100) >> 9) & 7;
        direction = angle_direction + WINDOW_ANIM_MARKER_FIRST;
        if (flags & WORLD_MAP_MARKER_ALTERNATE_DIRECTION_SET) {
            direction = angle_direction + WINDOW_ANIM_MARKER_ALTERNATE_FIRST;
        }
        if (state->marker.render.sequence != direction) {
            state->marker.render.sequence = direction;
            state->marker.render.anim_counter = 0;
            state->marker.render.frame_index = 0;
        }
        state->flags = flags ^ WORLD_MAP_MARKER_UPDATE_DIRECTION;
    }

    if (g_world_map_marker_state.flags & WORLD_MAP_MARKER_UPDATE_POSITION) {
        RotTrans(&g_world_map_marker_state.marker.position, &output, &transform_flag);
        g_world_map_marker_state.marker.render.position.x = output.vx;
        g_world_map_marker_state.marker.render.position.y = output.vy;
        g_world_map_marker_state.flags ^= WORLD_MAP_MARKER_UPDATE_POSITION;
    }
}

/* Set up render record `index` for a window image of `bounds`: unhide it, place it at bounds.x -
 * 0x80, bounds.y - 0x78, give it the window CLUT at (0, 0x1E1) and the texture page of the mode's
 * VRAM area, then draw the empty window frame into `image`. */
void window_build_render_record_image(s32 index, RECT bounds, s32 coordinate_mode, u32* image) {
    point32_t* clut;
    CVECTOR* color;
    point32_t transformed;

    g_window_gs_image_records[index].flags
        &= ~(DISPLAY_OBJECT_8BIT_CLUT | DISPLAY_OBJECT_BLANK | DISPLAY_OBJECT_HIDDEN);
    g_window_gs_image_records[index].base.x = bounds.x - 0x80;
    g_window_gs_image_records[index].base.y = bounds.y - 0x78;
    g_window_gs_image_records[index].coordinate_mode = coordinate_mode;
    /* Struct view of clut_x/clut_y: as plain s32 stores they could alias the
     * stacked image argument, whose load the target schedules after them. */
    clut = &g_window_gs_image_records[index].clut;
    clut->y = 0x1E1;
    clut->x = 0;
    g_window_gs_image_records[index].bounds = bounds;
    window_get_image_vram_position(coordinate_mode, bounds, &transformed);
    g_window_gs_image_records[index].tpage = GetTPage(0, 0, transformed.x, transformed.y);
    color = &g_window_gs_image_records[index].color;
    color->b = color->g = color->r = 0x80;
    g_window_gs_image_records[index].anim_counter = 0;
    screen_build_window_frame_image(bounds.w, bounds.h, (u16*)image);
}

/* The VRAM position of a window image at `bounds` in coordinate mode `mode`.
 * Images are 4-bit, four texels to a VRAM halfword: modes 0 and 1 put them in
 * the texture pages at x 0x180 and 0x1c0, mode 2 in the page at (0x240, 0x100).
 *
 * The callee receives the adjacent position and dimensions registers as one
 * eight-byte aggregate. Dimensions are preserved by the ABI but unused here.
 */
static void window_get_image_vram_position(s32 mode, RECT bounds, point32_t* output) {
    switch (mode) {
    case 0:
    case 1:
        output->x = bounds.x / 4 + 0x180 + mode * 64;
        output->y = bounds.y;
        break;
    case 2:
        output->x = (bounds.x / 4) + 0x240;
        output->y = bounds.y + 0x100;
        break;
    }
}

/* Upload a render record's window image at the VRAM position its coordinate
 * mode selects. The record's bounds are passed to the transform by value. */
void window_load_render_record_image(s32 index, u32* image) {
    point32_t transformed;
    RECT rect;

    window_get_image_vram_position(
        g_window_gs_image_records[index].coordinate_mode, g_window_gs_image_records[index].bounds, &transformed);
    rect.x = transformed.x;
    rect.y = transformed.y;
    rect.w = g_window_gs_image_records[index].bounds.w / 4;
    rect.h = g_window_gs_image_records[index].bounds.h;
    LoadImage(&rect, image);
}
