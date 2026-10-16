/* Map dots, location menus, routes and path animation, route travel, projection motion and scrolling, random
 * encounters, the location marker and camera, and render record images. */

#include "fft/file.h"
#include "fft/gfx.h"
#include "fft/main.h"
#include "fft/main_file.h"
#include "fft/main_gfx.h"
#include "fft/main_sound.h"
#include "fft/menu_text.h"
#include "fft/script_variable.h"
#include "fft/wldcore.h"
#include "fft/wldcore_gfx.h"
#include "fft/wldcore_map.h"
#include "fft/wldcore_menu.h"
#include "fft/wldcore_menu_level.h"
#include "fft/wldcore_proposition.h"
#include "fft/wldcore_script.h"
#include "fft/wldcore_sound.h"
#include "fft/wldcore_text.h"
#include "fft/wldcore_window.h"
#include "fft/world.h"
#include "fft/world_menu.h"
#include "fft/world_sprite.h"
#include "psx/libc.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/libgs.h"
#include "psx/libgte.h"
#include "psx/types.h"

static void wldcore_map_negate_and_clamp_coordinates(const SVECTOR* input, VECTOR* output);
static s32 wldcore_map_roll_random_encounter(s32 location, s32 route);
static void wldcore_map_scroll_projection_if_focus_near_edge(point32_t* delta);
static void wldcore_window_transform_point_for_mode(s32 mode, RECT bounds, point32_t* output);

/* Build the world-map location dots from their packed source rows and queue them for
 * drawing. Castles use the next dot sequence, a set color variable selects the town or
 * field palette, and a clear visibility variable hides the dot. */
void wldcore_map_init_dots(void) {
    wldcore_map_dot_source_t* src;
    wldcore_map_dot_t* dots = g_wldcore_map_dots;
    wldcore_map_dot_t* dot;
    s32* flags_base;
    CVECTOR* color;
    s32 y;
    s32 is_castle;
    s32 queue_index;
    s32 i;

    /* Queue entries are built from the flags column base; &dots[i].render.flags does not match. */
    flags_base = &dots->render.flags;
    dot = dots;
    src = g_wldcore_map_dot_sources;
    g_wldcore_map_dot_pulse_direction = 0;
    g_wldcore_map_dot_pulse_phase = 0;
    g_wldcore_map_dot_count = 0;
    for (i = 0; i < WORLD_LOCATION_COUNT; i++) {
        g_wldcore_map_dots[i].kind = (u8)(src[i].position_and_kind >> 16);
        g_wldcore_map_dots[i].sub_kind = (u8)(src[i].position_and_kind >> 24);
        /* The x coordinate occupies the packed 32-bit value's low 16 bits. */
        g_wldcore_map_dots[i].position.vx = src[i].position_and_kind;
        y = src[i].y;
        g_wldcore_map_dots[i].render.flags = WORLD_RENDER_RECORD_ANIMATED;
        g_wldcore_map_dots[i].render.priority = 0xf;
        /* Direct nested access biases the base to render; retail addresses the color column. */
        color = (CVECTOR*)((u8*)&g_wldcore_map_dots[i] + offsetof(wldcore_map_dot_t, render.color));
        g_wldcore_map_dots[i].position.vz = 0;
        g_wldcore_map_dots[i].position.vy = y;
        color->r = 0x80;
        color->g = 0x80;
        color->b = 0x80;
        is_castle = g_wldcore_map_dots[i].kind == 2;
        g_wldcore_map_dots[i].render.sequence = is_castle + WLDCORE_ANIM_SEQUENCE_MAP_DOT;
        if (script_get_variable(i + EVENT_SCRIPT_VAR_LOCATION_COLOR_FIRST) != 0) {
            if (i < WORLD_LOCATION_FIELD_FIRST) {
                g_wldcore_map_dots[i].render.palette = 0xc;
            } else {
                g_wldcore_map_dots[i].render.palette = 0x10;
            }
        } else {
            g_wldcore_map_dots[i].render.palette = 0;
        }
        dot->render.anim_counter = 0;
        g_wldcore_map_dots[i].render.frame_index = 0;
        if (script_get_variable(i + EVENT_SCRIPT_VAR_LOCATION_VISIBLE_FIRST) == 0) {
            g_wldcore_map_dots[i].render.flags |= WORLD_RENDER_RECORD_HIDDEN;
        }
        dot++;
        queue_index = g_wldcore_window_render_object_count;
        g_wldcore_window_render_object_queue[queue_index] = (void*)(i * sizeof(wldcore_map_dot_t) + (s32)flags_base);
        g_wldcore_window_render_object_count = queue_index + 1;
    }

    g_wldcore_map_dot_count = WORLD_LOCATION_COUNT;
    wldcore_map_project_and_cull_dots();
}

/* Projects every world-map dot and hides the ones that are off screen, all of
 * them while MAIN_SYSTEM_FLAG_HIDE_MAP_DOTS is set, and those whose location
 * is not yet visible (EVENT_SCRIPT_VAR_LOCATION_VISIBLE_FIRST + location).
 *
 * The screen bounds are written as the target's unsigned range tests: x in
 * [-128, 128] and y in [-116, 120]. */
void wldcore_map_project_and_cull_dots(void) {
    VECTOR position;
    long flag;
    s32 i;

    for (i = 0; i < g_wldcore_map_dot_count; i++) {
        RotTrans(&g_wldcore_map_dots[i].position, &position, &flag);
        g_wldcore_map_dots[i].render.position.x = position.vx;
        g_wldcore_map_dots[i].render.position.y = position.vy;
        if (position.vx < -128 || position.vx > 128 || position.vy < -116 || position.vy > 120
            || (g_main_system_flags & MAIN_SYSTEM_FLAG_HIDE_MAP_DOTS)
            || script_get_variable(i + EVENT_SCRIPT_VAR_LOCATION_VISIBLE_FIRST) == 0) {
            g_wldcore_map_dots[i].render.flags |= WORLD_RENDER_RECORD_HIDDEN;
        } else {
            g_wldcore_map_dots[i].render.flags &= ~WORLD_RENDER_RECORD_HIDDEN;
        }
    }
}

/* Pulse the map dots' brightness. The phase counts up to 0x40 and back down,
 * a step a frame unless MAIN_SYSTEM_FLAG_FREEZE_ANIMATION is set; dots with a
 * palette (colored towns and fields) run from 0x60 to 0xe0 with it and the
 * others the opposite way, so the two kinds pulse out of step.
 *
 * The 0x40 compare limit reuses the dead `flags` local. With its own `limit`
 * variable the allocator swaps the g_wldcore_map_dot_pulse_direction snapshot ($a0 in the target) and
 * the limit ($v1); reusing the function-scope allocno reproduces the target
 * pair. */
void wldcore_map_pulse_dot_colors(void) {
    s32 flags = g_main_system_flags;
    s32 i;
    s32 phase;
    s32 bright;
    s32 dark;
    u8 state;
    u8 counter;
    CVECTOR* color;

    if (flags & MAIN_SYSTEM_FLAG_HIDE_DISPLAY_OBJECTS) {
        return;
    }

    if (!(flags & MAIN_SYSTEM_FLAG_FREEZE_ANIMATION)) {
        state = g_wldcore_map_dot_pulse_direction;
        flags = 0x40;
        if (state == 0) {
            counter = g_wldcore_map_dot_pulse_phase + 1;
            g_wldcore_map_dot_pulse_phase = counter;
            counter = counter;
            if (counter == flags) {
                g_wldcore_map_dot_pulse_direction = state ^ 1;
            }
        } else {
            counter = g_wldcore_map_dot_pulse_phase - 1;
            g_wldcore_map_dot_pulse_phase = counter;
            counter = counter;
            if (counter == 0) {
                g_wldcore_map_dot_pulse_direction = state ^ 1;
            }
        }
    }

    phase = g_wldcore_map_dot_pulse_phase * 2;
    bright = phase + 0x60;
    dark = -0x20 - phase;
    for (i = 0; i < g_wldcore_map_dot_count; i++) {
        if (!(g_wldcore_map_dots[i].render.flags & WORLD_RENDER_RECORD_HIDDEN)) {
            /* Direct nested access biases the base to render; retail addresses the color column. */
            color = (CVECTOR*)((u8*)&g_wldcore_map_dots[i] + offsetof(wldcore_map_dot_t, render.color));
            if (g_wldcore_map_dots[i].render.palette != 0) {
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
s32 wldcore_map_find_dot_at_point(point32_t point) {
    s32 best = 0xFFFFFF;
    s32 i;
    s32 hit = 0;
    s32 value;

    for (i = 0; i < g_wldcore_map_dot_count; i++) {
        if (script_get_variable(i + EVENT_SCRIPT_VAR_LOCATION_VISIBLE_FIRST) == 0) {
            continue;
        }
        if ((g_wldcore_map_dots[i].render.flags & (WORLD_RENDER_RECORD_BLANK | WORLD_RENDER_RECORD_HIDDEN)) != 0) {
            continue;
        }
        value = g_wldcore_map_dots[i].render.position.x;
        if (point.x < value - 10 || value + 10 < point.x) {
            continue;
        }
        value = g_wldcore_map_dots[i].render.position.y;
        if (point.y < value - 14 || value + 6 < point.y) {
            continue;
        }
        value = g_wldcore_map_dots[i].render.priority;
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
s32 wldcore_map_get_dot_snap_step(point32_t point, s32* step_x, s32* step_y) {
    s32 hit;
    s32 dot_x;
    s32 dot_y;

    *step_x = 0;
    *step_y = 0;
    hit = wldcore_map_find_dot_at_point(point);
    if (hit != 0) {
        dot_x = g_wldcore_map_dots[hit - 1].render.position.x;
        dot_y = g_wldcore_map_dots[hit - 1].render.position.y - 4;
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
 * The Deep Dungeon offers its floors (WLDCORE_TEXT_ID_DEEP_DUNGEON_FLOOR_FIRST
 * + i), one more than EVENT_SCRIPT_VAR_DEEP_DUNGEON_LIST_LENGTH. A town offers
 * the bar, shop and soldier office, and the fur shop as a fourth entry at
 * Dorter, Warjilis and Zarghidas once EVENT_SCRIPT_VAR_FUR_SHOP_ENABLED is
 * set. Other locations offer nothing.
 */
s32 wldcore_map_build_location_menu_entries(s32 location_id, s32* out) {
    s32 count;
    s32 i;

    if (location_id == WORLD_LOCATION_DEEP_DUNGEON) {
        count = script_get_variable(EVENT_SCRIPT_VAR_DEEP_DUNGEON_LIST_LENGTH) + 1;
        for (i = 0; i < count; i++) {
            out[i] = WLDCORE_TEXT_ID_DEEP_DUNGEON_FLOOR_FIRST + i;
        }
        return count;
    }

    count = 0;
    if (g_wldcore_location_records[location_id].is_town == 1) {
        count = 3;
        out[0] = WLDCORE_TEXT_ID_BAR;
        out[1] = WLDCORE_TEXT_ID_SHOP;
        out[2] = WLDCORE_TEXT_ID_SOLDIER_OFFICE;
        if (script_get_variable(EVENT_SCRIPT_VAR_FUR_SHOP_ENABLED) != 0) {
            if ((location_id == WORLD_LOCATION_DORTER) || (location_id == WORLD_LOCATION_WARJILIS)
                || (location_id == WORLD_LOCATION_ZARGHIDAS)) {
                out[count] = WLDCORE_TEXT_ID_FUR_SHOP;
                count++;
            }
        }
    }
    return count;
}

/* Fill `out` with the map-name text ids of the current location's Choice destinations and return
 * how many: -1 at Murond Death City, 0 when the location's color variable is set or no Choice
 * applies; a single destination is taken as is, otherwise up to five whose hidden flag is clear. */
static s32 wldcore_location_collect_choice_text_ids(s32 location_id, s32* out) {
    s32 available;
    s32 i;
    s32 written;
    s32 entry;

    if (location_id == WORLD_LOCATION_MUROND_DEATH_CITY) {
        return -1;
    }
    if (script_get_variable(location_id + EVENT_SCRIPT_VAR_LOCATION_COLOR_FIRST) != 0) {
        return 0;
    }
    if (wldcore_script_process_conditional_set(g_wldcore_map_projection_state.marker.kind, WLDCORE_SCRIPT_ACTION_CHOICE)
        == 0) {
        return 0;
    }
    available = 0;
    for (i = 0; i < 4; i++) {
        if (g_wldcore_script_state.choice_vars[i] == 0) {
            break;
        }
        available++;
    }
    if (available == 1) {
        entry = g_wldcore_script_state.choice_vars[0];
        *out = entry + (TEXT_ID_MAP_NAME_BASE - 1);
        return 1;
    }
    written = 0;
    for (i = 0; i < available; i++) {
        entry = g_wldcore_script_state.choice_vars[i];
        if (script_get_variable(
                g_wldcore_choice_hidden_variable_offsets[entry] + EVENT_SCRIPT_VAR_LOCATION_CHOICE_HIDDEN_FIRST)
            == 0) {
            out[written++] = entry + (TEXT_ID_MAP_NAME_BASE - 1);
            if (written == 5) {
                break;
            }
        }
    }
    return written;
}

/* Empty. wldcore_init_subsystems calls it between drawing the map dots and placing the location marker. */
void wldcore_map_init_stub(void) {
}

/* Draws the visible world-map routes into the auxiliary ordering table as
 * textured semi-transparent ribbons. Each route record is a run of packed
 * (x, y) pairs holding the two edges of the ribbon; every point is projected
 * through RotTransSV into one of four rotating vertex slots and flagged
 * in-bounds when x + 127 and y + 111 fall inside 255 x 227. Once four slots
 * are full they become one POLY_FT4 quad, emitted when either edge-leading
 * vertex was in bounds, and the last pair slides down to slots 0 and 1 so the
 * next pair closes the following segment. Nothing is drawn while
 * MAIN_SYSTEM_FLAG_HIDE_MAP_ROUTES is set, and a route only once it is
 * revealed (EVENT_SCRIPT_VAR_PATH_VISIBLE_FIRST + route).
 *
 * The count is sign-extended into count_tmp, which the loop guard tests before
 * the copy into count; the `j < count_tmp` guard leaves an unused compare
 * pseudo whose stack slot is the target's otherwise empty 32-bit stack slot at 0x88,
 * and taking `vertex` in the loop header numbers the vertex-base pseudo ahead
 * of it so the base spills to 0x80 as in the target. The 2-byte alignment of
 * SVECTOR makes `verts[0] = verts[2]` take GCC's unaligned block-move path
 * (lwl/lwr, swl/swr) as in the target. */
void wldcore_map_draw_visible_routes(GsOT* ot) {
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

    if (g_main_system_flags & MAIN_SYSTEM_FLAG_HIDE_MAP_ROUTES) {
        return;
    }

    table = g_wldcore_map_route_polylines;
    SetPolyFT4(&poly);
    SetSemiTrans(&poly, 1);
    setUV4(&poly, 0x88, 0xE0, 0x97, 0xE0, 0x88, 0xEF, 0x97, 0xEF);
    setRGB0(
        &poly, g_wldcore_screen_brightness_rgb.r, g_wldcore_screen_brightness_rgb.g, g_wldcore_screen_brightness_rgb.b);
    poly.tpage = GetTPage(0, 0, 0x1C0, 0x100);
    poly.clut = GetClut(0x10, 0x1E8);

    for (i = 0, vertex = verts; i < WLDCORE_ROUTE_COUNT; i++) {
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
static s32 wldcore_location_begin_route_segment(wldcore_location_entry_state_t* state, u32 route, s32 step) {
    s32 unused[8];
    wldcore_route_record_t* record;
    s32 index;
    wldcore_route_point_t* point_down;
    wldcore_route_point_t* point_up;
    s32 word;
    s32 flags;

    /* Direct indexing or separate typed locals move the table load after the route-ID store. */
    record = (wldcore_route_record_t*)g_wldcore_map_route_tables;
    state->route_id = route & WLDCORE_ROUTE_PATH_ID_MASK;
    flags = state->flags;
    state->flags = flags | ((route >> 7) & WLDCORE_ROUTE_TRAVEL_REVERSED);
    record = ((wldcore_route_record_t**)record)[state->route_id];
    if (state->flags & WLDCORE_ROUTE_TRAVEL_REVERSED) {
        state->leg_end_location = record->start_location;
        state->leg_start_location = record->end_location;
        /* Direct point indexing moves the header addition past the shift. */
        index = (record->count - step - 1) * (sizeof(wldcore_route_point_t) / sizeof(u32))
            + offsetof(wldcore_route_record_t, points) / sizeof(u32);
        point_down = (wldcore_route_point_t*)(index * sizeof(u32) + (s32)record);
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
        index = step * (sizeof(wldcore_route_point_t) / sizeof(u32))
            + offsetof(wldcore_route_record_t, points) / sizeof(u32);
        point_up = (wldcore_route_point_t*)(index * sizeof(u32) + (s32)record);
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
 * g_wldcore_map_path_animation as a slideshow: copies its polyline, reversed when the record
 * runs from `to` to `from`, and times each segment by its length.
 *
 * `data` holds the polyline table and then the polyline itself, as the target
 * keeps both in one register. The header's second endpoint is read from the
 * whole 32-bit header value, as the target does. The segment differences read the
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
    for (i = 0; i < WLDCORE_ROUTE_COUNT; i++) {
        record = *records;
        first = record->start_location;
        second = (*(u32*)record & 0xFF0000) >> 16;
        if (first == from && second == to) {
            g_wldcore_map_path_animation.script_variable = i;
            break;
        }
        if (second == from && first == to) {
            g_wldcore_map_path_animation.script_variable = i;
            g_wldcore_map_path_animation.flags |= WLDCORE_SLIDESHOW_REVERSED;
            break;
        }
        records++;
    }

    if (i == WLDCORE_ROUTE_COUNT) {
        return;
    }

    g_wldcore_map_path_animation.flags |= WLDCORE_SLIDESHOW_RUNNING;
    first = record->count - 1;
    g_wldcore_map_path_animation.frame_timer = 0;
    g_wldcore_map_path_animation.page_index = 0;
    g_wldcore_map_path_animation.page_count = first;
    data = (u16*)g_wldcore_map_route_polylines;
    data = ((u16**)data)[g_wldcore_map_path_animation.script_variable];
    g_wldcore_map_path_animation.point_count = *data;
    data++;
    for (i = 0; i < g_wldcore_map_path_animation.point_count; i++) {
        if (g_wldcore_map_path_animation.flags & WLDCORE_SLIDESHOW_REVERSED) {
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

/* Draws the slideshow pages shown so far as textured semi-transparent quads:
 * every page up to page_index is projected from its four outline points, and
 * the current page's far edge is interpolated by frame_timer / page_frames so
 * it unrolls over the page's display time. A negative page_index or one equal
 * to page_count stops the slideshow instead. */
void wldcore_map_draw_path_animation(GsOT* ot) {
    SVECTOR verts[4];
    SVECTOR in;
    POLY_FT4 poly;
    s32 projected;
    s32 i;
    s32 j;
    s32 frames;

    if (!(g_wldcore_map_path_animation.flags & WLDCORE_SLIDESHOW_RUNNING)) {
        return;
    }

    SetPolyFT4(&poly);
    SetSemiTrans(&poly, 1);
    setUV4(&poly, 0x88, 0xE0, 0x97, 0xE0, 0x88, 0xEF, 0x97, 0xEF);
    setRGB0(
        &poly, g_wldcore_screen_brightness_rgb.r, g_wldcore_screen_brightness_rgb.g, g_wldcore_screen_brightness_rgb.b);
    poly.tpage = GetTPage(0, 0, 0x1C0, 0x100);
    poly.clut = GetClut(0x10, 0x1E8);
    if (g_wldcore_map_path_animation.page_index < 0
        || g_wldcore_map_path_animation.page_index == g_wldcore_map_path_animation.page_count) {
        g_wldcore_map_path_animation.flags &= ~WLDCORE_SLIDESHOW_RUNNING;
        return;
    }

    for (i = 0; i < g_wldcore_map_path_animation.page_index + 1; i++) {
        if (i == g_wldcore_map_path_animation.page_index) {
            frames = g_wldcore_map_path_animation.frame_timer;
        } else {
            frames = g_wldcore_map_path_animation.page_frames[i];
        }
        for (j = 0; j < 4; j++) {
            in.vx = g_wldcore_map_path_animation.coords[i * 4 + j * 2];
            in.vy = g_wldcore_map_path_animation.coords[i * 4 + j * 2 + 1];
            in.vz = 0;
            RotTransSV(&in, &verts[j], &projected);
        }
        if (frames != g_wldcore_map_path_animation.page_frames[i]) {
            verts[2].vx
                = verts[0].vx + (verts[2].vx - verts[0].vx) * frames / g_wldcore_map_path_animation.page_frames[i];
            verts[2].vy
                = verts[0].vy + (verts[2].vy - verts[0].vy) * frames / g_wldcore_map_path_animation.page_frames[i];
            verts[3].vx
                = verts[1].vx + (verts[3].vx - verts[1].vx) * frames / g_wldcore_map_path_animation.page_frames[i];
            verts[3].vy
                = verts[1].vy + (verts[3].vy - verts[1].vy) * frames / g_wldcore_map_path_animation.page_frames[i];
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
static void wldcore_map_find_best_route_path(s32 from, s32 to) {
    s32 unused[4];
    s32 cost;
    s32 route;
    wldcore_route_record_t** table;
    s32 i;
    wldcore_route_record_t* record;
    s32 start;
    s32 end;
    s32 length;
    s32 buffer_index;

    if (g_wldcore_route_search_depth >= WLDCORE_ROUTE_PATH_CAPACITY) {
        return;
    }

    table = g_wldcore_map_route_tables;
    for (route = 0; route < WLDCORE_ROUTE_COUNT; route++) {
        if (VSync(1) > 200) {
            main_file_poll_load(&g_main_file_cd_state);
            wldcore_sound_process_audio_queue();
            wldcore_map_pulse_dot_colors();
            wldcore_gfx_draw_world_frame();
            buffer_index = GsGetActiveBuff();
            g_main_gfx_active_buffer_index = buffer_index;
            GsSetWorkBase(g_wldcore_gfx_world_primitive_buffers + buffer_index * 0xE000);
        }
        if (script_get_variable(route + EVENT_SCRIPT_VAR_PATH_VISIBLE_FIRST) == 0) {
            continue;
        }
        for (i = 0; i < g_wldcore_route_search_depth; i++) {
            if ((g_wldcore_route_search_path.routes[i] & WLDCORE_ROUTE_PATH_ID_MASK) == route) {
                break;
            }
        }
        if (i < g_wldcore_route_search_depth) {
            continue;
        }
        record = table[route];
        length = g_wldcore_route_search_depth + 1;
        start = record->start_location;
        end = record->end_location;
        cost = record->cost;
        if (g_wldcore_location_entry_state.best_length_bound < length) {
            continue;
        }
        if (length == g_wldcore_location_entry_state.best_length_bound
            && g_wldcore_location_entry_state.best_cost < cost + g_wldcore_route_search_cost) {
            continue;
        }
        if (from == start && to == end) {
            g_wldcore_route_search_path.routes[g_wldcore_route_search_depth] = route;
            /* Both directions record the path through one copy of this
             * code (a second copy loop could not be cross-jumped away). */
            goto found;
        }
        if (to == start && from == end) {
            g_wldcore_route_search_path.routes[g_wldcore_route_search_depth] = route | WLDCORE_ROUTE_PATH_REVERSED;
        found:
            g_wldcore_location_entry_state.route_path = g_wldcore_route_search_path;
            g_wldcore_location_entry_state.best_cost = g_wldcore_route_search_cost + cost;
            g_wldcore_location_entry_state.best_length_bound = g_wldcore_location_entry_state.best_path_length
                = g_wldcore_route_search_depth + 1;
            g_wldcore_location_entry_state.flags |= WLDCORE_ROUTE_TRAVEL_ACTIVE;
            continue;
        }
        if (from == start) {
            g_wldcore_route_search_path.routes[g_wldcore_route_search_depth] = route;
            g_wldcore_route_search_depth++;
            g_wldcore_route_search_cost += cost;
            wldcore_map_find_best_route_path(end, to);
            g_wldcore_route_search_depth--;
            g_wldcore_route_search_cost -= cost;
        } else if (from == end) {
            g_wldcore_route_search_path.routes[g_wldcore_route_search_depth] = route | WLDCORE_ROUTE_PATH_REVERSED;
            g_wldcore_route_search_depth++;
            g_wldcore_route_search_cost += cost;
            wldcore_map_find_best_route_path(start, to);
            g_wldcore_route_search_depth--;
            g_wldcore_route_search_cost -= cost;
        }
    }
}

/* Start travel from from_location to to_location. When the current location's
 * map script stages LoadEvent, that event becomes
 * EVENT_SCRIPT_VAR_CURRENT_EVENT and the event transition level is pushed;
 * returns 0. Otherwise a staged SetVar2 is applied and
 * wldcore_map_find_best_route_path searches the cheapest path; when there is
 * one, its first leg starts: the marker turns to the leg's heading, the menu
 * window is hidden, the view pans when the marker is off screen and the
 * destination's picture starts loading. Returns 1.
 *
 * g_wldcore_route_search_depth is a separate scalar: as a field of the entry state it becomes
 * the address anchor that the later &g_wldcore_location_entry_state argument is formed from, and
 * the chained 99999 store makes best_cost that anchor as in the target. The
 * location load and the two argument addresses share one base register in the
 * target, which needs them to be members of the one g_wldcore_map_projection_state record. */
s32 wldcore_location_process_entry(s32 from_location, s32 to_location) {
    s32 depth;

    if (wldcore_script_process_conditional_set(
            g_wldcore_map_projection_state.marker.kind, WLDCORE_SCRIPT_ACTION_LOAD_EVENT)
        != 0) {
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1].window_index].flags
            |= WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_next_map_id[0] = 0;
        script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, g_wldcore_script_state.args[0]);
        wldcore_menu_push_event_transition_level(g_wldcore_script_state.args[1]);
        g_main_system_flags |= MAIN_SYSTEM_FLAG_HIDE_LOCATION_MARKER;
        return 0;
    } else {
        if (wldcore_script_process_conditional_set(
                g_wldcore_map_projection_state.marker.kind, WLDCORE_SCRIPT_ACTION_SET_VAR_2)
            != 0) {
            script_set_variable(g_wldcore_script_state.args[0], g_wldcore_script_state.args[1]);
        }
        g_wldcore_route_search_cost = 0;
        g_wldcore_route_search_depth = 0;
        g_wldcore_location_entry_state.best_cost = g_wldcore_location_entry_state.best_length_bound = 99999;
        g_wldcore_location_entry_state.flags = 0;
        wldcore_map_find_best_route_path(from_location, to_location);
        if (!(g_wldcore_location_entry_state.flags & WLDCORE_ROUTE_TRAVEL_ACTIVE)) {
            return 1;
        }
        g_wldcore_location_entry_state.route_index = 0;
        if (g_wldcore_location_entry_state.best_path_length == 0) {
            return 1;
        }
        g_wldcore_location_entry_state.segment_count = wldcore_location_begin_route_segment(
            &g_wldcore_location_entry_state, g_wldcore_location_entry_state.route_path.routes[0], 0);
        g_wldcore_location_entry_state.segment_index = 0;
        g_wldcore_map_projection_state.angle = g_wldcore_location_entry_state.heading;
        depth = g_wldcore_menu_stack_depth - 1;
        g_wldcore_location_entry_state.menu_level = depth;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].window_index].flags
            |= WORLD_RENDER_RECORD_HIDDEN;
        if (wldcore_map_start_projection_motion_if_outside_bounds(
                &g_wldcore_map_projection_state.marker.render.position, &g_wldcore_map_projection_state.marker.position)
            == 0) {
            g_wldcore_map_projection_state.flags
                |= WLDCORE_PROJECTION_FLAG_UPDATE_DIRECTION | WLDCORE_PROJECTION_FLAG_ALTERNATE_DIRECTION_SET;
        }
        g_wldcore_map_projection_state.marker.sub_kind = wldcore_proposition_load_picture(to_location);
        g_main_system_flags |= MAIN_SYSTEM_FLAG_HIDE_LOCATION_MARKER;
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
            file_wait_for_pending();
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

/* Start a projection move (a camera pan) toward a point outside the camera bounds.
 *
 * The motion record snapshots the current origin and stores the target
 * distance in the same 8-bit fractional scale used by its progress field. */
s32 wldcore_map_start_projection_motion_if_outside_bounds(const point32_t* point, const SVECTOR* coordinates) {
    s32 dx;
    s32 dy;

    if (wldcore_map_is_point_outside_projection_bounds(point) == 0) {
        return 0;
    }

    wldcore_map_negate_and_clamp_coordinates(coordinates, &g_wldcore_map_projection_motion.target);
    g_wldcore_map_projection_motion.flags |= WLDCORE_PROJECTION_MOTION_ACTIVE;
    g_wldcore_map_projection_motion.origin = g_wldcore_map_projection_origin;

    dx = g_wldcore_map_projection_motion.target.vx - g_wldcore_map_projection_motion.origin.vx;
    dy = g_wldcore_map_projection_motion.target.vy - g_wldcore_map_projection_motion.origin.vy;
    g_wldcore_map_projection_motion.progress = 0;
    g_wldcore_map_projection_motion.distance = csqrt(dx * dx + dy * dy) >> 8;
    return 1;
}

/* Whether a point lies outside the view far enough to pan the camera: beyond an
 * edge while the projection origin still has more than 0x20 of scroll room
 * toward it. Nothing wraps; at the scroll limit the point counts as inside. */
s32 wldcore_map_is_point_outside_projection_bounds(const point32_t* point) {
    if ((point->x < -0x40 && 0x80 - g_wldcore_map_projection_origin.vx >= 0x21)
        || (point->x >= 0x49 && g_wldcore_map_projection_origin.vx + 0x74 >= 0x21)) {
        return 1;
    }
    if ((point->y < -0x40 && 0x50 - g_wldcore_map_projection_origin.vy >= 0x21)
        || (point->y >= 0x41 && g_wldcore_map_projection_origin.vy + 0x40 >= 0x21)) {
        return 1;
    }
    return 0;
}

/* Negate and clamp a signed coordinate pair to the world-scroll limits. */
static void wldcore_map_negate_and_clamp_coordinates(const SVECTOR* input, VECTOR* output) {
    s32 x = -input->vx;
    s32 y = -input->vy;

    if (x < WLDCORE_SCROLL_MIN_X) {
        x = WLDCORE_SCROLL_MIN_X;
    }
    if (x > WLDCORE_SCROLL_MAX_X) {
        x = WLDCORE_SCROLL_MAX_X;
    }
    if (y < WLDCORE_SCROLL_MIN_Y) {
        y = WLDCORE_SCROLL_MIN_Y;
    }
    if (y > WLDCORE_SCROLL_MAX_Y) {
        y = WLDCORE_SCROLL_MAX_Y;
    }
    output->vx = x;
    output->vy = y;
}

/* Second names for three fields of g_wldcore_map_projection_motion: the
 * target loads each through its own address, in this order. */
extern s32 g_wldcore_map_projection_motion_progress;
extern s32 g_wldcore_map_projection_motion_origin_x;
extern s32 g_wldcore_map_projection_motion_target_x;

/* Advance one frame of the camera pan started by
 * wldcore_map_start_projection_motion_if_outside_bounds.
 *
 * Interpolates the live projection origin between the snapshotted origin and
 * the clamped target by progress/distance. While progress is still short of
 * distance it is advanced and 1 is returned. On the final frame the origin is
 * snapped to the target, the record's active flag is toggled off, and 0 is
 * returned.
 *
 * The final snap copies the target VECTOR's four 32-bit fields. */
s32 wldcore_map_step_projection_motion(void) {
    s32 progress;
    s32 distance;
    s32 target_x = g_wldcore_map_projection_motion_target_x;
    s32 origin_x = g_wldcore_map_projection_motion_origin_x;
    s32 delta_x;
    s32 origin_y;

    progress = g_wldcore_map_projection_motion_progress;
    distance = g_wldcore_map_projection_motion.distance;
    target_x = (target_x - origin_x) * progress;
    delta_x = target_x / distance;
    g_wldcore_map_projection_origin.vx = origin_x + delta_x;
    origin_y = g_wldcore_map_projection_motion.origin.vy;
    g_wldcore_map_projection_origin.vy
        = origin_y + (g_wldcore_map_projection_motion.target.vy - origin_y) * progress / distance;
    if (progress >= distance) {
        g_wldcore_map_projection_origin = g_wldcore_map_projection_motion.target;
        g_wldcore_map_projection_motion.flags ^= WLDCORE_PROJECTION_MOTION_ACTIVE;
        return 0;
    }
    g_wldcore_map_projection_motion.progress = progress + 1;
    return 1;
}

/* Applies a scroll delta to the projection origin when the resulting window
 * origin would leave [-64, 72] horizontally or [-64, 64] vertically, and
 * sets MAIN_SYSTEM_FLAG_DRAW_MAP if the origin's x or y changed. */
static void wldcore_map_scroll_projection_if_focus_near_edge(point32_t* delta) {
    point32_t position;
    VECTOR saved = g_wldcore_map_projection_origin;

    position.x = g_wldcore_map_projection_state.marker.render.position.x + delta->x;
    position.y = g_wldcore_map_projection_state.marker.render.position.y + delta->y;
    if ((delta->x < 0 && position.x < -0x40) || (delta->x > 0 && position.x >= 0x49)) {
        wldcore_map_apply_clamped_horizontal_scroll(&delta->x, &g_wldcore_map_projection_origin.vx);
    }
    if ((delta->y < 0 && position.y < -0x40) || (delta->y > 0 && position.y >= 0x41)) {
        wldcore_map_apply_clamped_vertical_scroll(delta, &g_wldcore_map_projection_origin);
    }
    if (saved.vx != g_wldcore_map_projection_origin.vx || saved.vy != g_wldcore_map_projection_origin.vy) {
        g_main_system_flags |= MAIN_SYSTEM_FLAG_DRAW_MAP;
    }
}

#define ENTRY_AT(byte_offset)   ((wldcore_encounter_entry_t*)((byte_offset) + (s32)base))
#define ENTRY_BASE(byte_offset) ((wldcore_encounter_entry_t*)((s32)base + (byte_offset)))

/* Roll for a random battle on one road into world-map location 24..42.
 *
 * The tier is the first of the road's four whose progress threshold is at or
 * above the road's script variable. A tier chance of 0 never triggers;
 * otherwise a battle happens when a roll of 0..99 is at or below the chance,
 * which is chance + 1 in 100. One of the ENTDs enabled in the tier's mask is
 * then drawn evenly. Returns 1 with the ENTD and map ids set; the squad id is
 * set as soon as the road is found. */
static s32 wldcore_map_roll_random_encounter(s32 location, s32 route) {
    wldcore_encounter_entry_t(*base)[3];
    s32 offset;
    wldcore_encounter_entry_t* row;
    u8* tier_column;
    s32 progress;
    s32 chance;
    s32 mask;
    s32 count;
    s32 pick;

    location -= WORLD_LOCATION_FIELD_FIRST;
    if (location < 0 || location >= WORLD_LOCATION_FIELD_COUNT) {
        return 0;
    }

    base = g_wldcore_random_battle_data;
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
    g_wldcore_random_battle_squad_id = row->deployment_squad_id + 0x200;
    progress = script_get_variable(row->script_variable_id);
    /* Direct tier fields cache the row pointer and remove retail base additions. */
    for (location = 0; location < 4; location++) {
        if (((u8*)(offset + location * sizeof(wldcore_encounter_tier_t)
                + (s32)base))[offsetof(wldcore_encounter_entry_t, tiers)
                + offsetof(wldcore_encounter_tier_t, progress_threshold)]
            >= progress) {
            break;
        }
    }

    if (location == 4) {
        return 0;
    }

    tier_column = (u8*)(location * sizeof(wldcore_encounter_tier_t) + offset + (s32)base);
    chance = tier_column[offsetof(wldcore_encounter_entry_t, tiers)
        + offsetof(wldcore_encounter_tier_t, encounter_chance)];
    if (chance == 0) {
        return 0;
    }

    pick = rand() * 100 >> 15;
    if (chance >= pick) {
        mask = tier_column[offsetof(wldcore_encounter_entry_t, tiers)
            + offsetof(wldcore_encounter_tier_t, encounter_mask)];
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
        g_wldcore_random_battle_entd_id
            = ((u8*)base)[offset + offsetof(wldcore_encounter_entry_t, entd_ids[7]) - location];
        g_wldcore_random_battle_map_id = ENTRY_AT(offset)->battle_map_id;
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
void wldcore_map_init_location_marker_and_camera(void) {
    VECTOR unused; /* unreferenced; reproduces the target's 0x38-byte frame */
    wldcore_map_dot_t* marker;
    SVECTOR* position;
    s32 index = script_get_variable(EVENT_SCRIPT_VAR_LOCATION);
    s32 count;
    s32 value;

    /* Only the kind, position and flags go through the pointer, as in the
     * target; the other fields are addressed through the symbol. */
    marker = &g_wldcore_map_projection_state.marker;
    marker->kind = index;
    position = &g_wldcore_map_dots[index].position;
    marker->position = *position;
    g_wldcore_map_projection_state.angle = 0;
    g_wldcore_map_projection_state.flags
        = WLDCORE_PROJECTION_FLAG_UPDATE_POSITION | WLDCORE_PROJECTION_FLAG_UPDATE_DIRECTION;
    count = g_wldcore_window_render_object_count;
    g_wldcore_window_render_object_queue[count++] = &marker->render;
    marker->render.flags = WORLD_RENDER_RECORD_ANIMATED;
    g_wldcore_map_projection_state.marker.render.priority = 0xE;
    g_wldcore_window_render_object_count = count;
    g_wldcore_map_projection_state.marker.render.anim_counter = 0;
    g_wldcore_map_projection_state.marker.render.frame_index = 0;
    g_wldcore_map_projection_state.marker.render.color.r = 0x80;
    g_wldcore_map_projection_state.marker.render.color.g = 0x80;
    g_wldcore_map_projection_state.marker.render.color.b = 0x80;
    value = script_get_variable(EVENT_SCRIPT_VAR_TOWN_BACKGROUND);
    g_wldcore_map_projection_state.marker.render.palette = 0;
    if (value == 1) {
        g_wldcore_map_projection_state.marker.render.palette = 2;
    }
    if (value == 2) {
        g_wldcore_map_projection_state.marker.render.palette = 3;
    }
    marker->position = g_wldcore_map_dots[marker->kind].position;
    wldcore_map_negate_and_clamp_coordinates(
        &g_wldcore_map_projection_state.marker.position, &g_wldcore_map_projection_origin);
    wldcore_map_project_and_cull_tiles();
    wldcore_map_update_camera_direction_and_projection();
}

/* Apply pending direction and GTE projection updates to the camera state.
 *
 * The 32-bit direction value is the player marker's sequence: the facing octant picks
 * one of eight marker sequences from WLDCORE_ANIM_SEQUENCE_MARKER_FIRST, or from
 * WLDCORE_ANIM_SEQUENCE_MARKER_ALTERNATE_FIRST for the alternate set. The
 * marker's position is projected with RotTrans into its render position. */
void wldcore_map_update_camera_direction_and_projection(void) {
    VECTOR output;
    wldcore_projection_state_t* state = &g_wldcore_map_projection_state;
    long transform_flag;
    s32 angle_direction;
    s32 direction;
    s32 flags;

    flags = state->flags;
    if (flags & WLDCORE_PROJECTION_FLAG_UPDATE_DIRECTION) {
        angle_direction = ((state->angle + 0x100) >> 9) & 7;
        direction = angle_direction + WLDCORE_ANIM_SEQUENCE_MARKER_FIRST;
        if (flags & WLDCORE_PROJECTION_FLAG_ALTERNATE_DIRECTION_SET) {
            direction = angle_direction + WLDCORE_ANIM_SEQUENCE_MARKER_ALTERNATE_FIRST;
        }
        if (state->marker.render.sequence != direction) {
            state->marker.render.sequence = direction;
            state->marker.render.anim_counter = 0;
            state->marker.render.frame_index = 0;
        }
        state->flags = flags ^ WLDCORE_PROJECTION_FLAG_UPDATE_DIRECTION;
    }

    if (g_wldcore_map_projection_state.flags & WLDCORE_PROJECTION_FLAG_UPDATE_POSITION) {
        RotTrans(&g_wldcore_map_projection_state.marker.position, &output, &transform_flag);
        g_wldcore_map_projection_state.marker.render.position.x = output.vx;
        g_wldcore_map_projection_state.marker.render.position.y = output.vy;
        g_wldcore_map_projection_state.flags ^= WLDCORE_PROJECTION_FLAG_UPDATE_POSITION;
    }
}

/* Set up render record `index` for a window image of `bounds`: unhide it, place it at bounds.x -
 * 0x80, bounds.y - 0x78, give it the window CLUT at (0, 0x1E1) and the texture page of the mode's
 * VRAM area, then draw the empty window frame into `image`. */
void wldcore_window_build_render_record_image(s32 index, RECT bounds, s32 coordinate_mode, u32* image) {
    point32_t* clut;
    CVECTOR* color;
    point32_t transformed;

    g_wldcore_window_render_records[index].flags
        &= ~(WORLD_RENDER_RECORD_8BIT_CLUT | WORLD_RENDER_RECORD_BLANK | WORLD_RENDER_RECORD_HIDDEN);
    g_wldcore_window_render_records[index].base.x = bounds.x - 0x80;
    g_wldcore_window_render_records[index].base.y = bounds.y - 0x78;
    g_wldcore_window_render_records[index].coordinate_mode = coordinate_mode;
    /* Struct view of clut_x/clut_y: as plain s32 stores they could alias the
     * stacked image argument, whose load the target schedules after them. */
    clut = &g_wldcore_window_render_records[index].clut;
    clut->y = 0x1E1;
    clut->x = 0;
    g_wldcore_window_render_records[index].bounds = bounds;
    wldcore_window_transform_point_for_mode(coordinate_mode, bounds, &transformed);
    g_wldcore_window_render_records[index].tpage = GetTPage(0, 0, transformed.x, transformed.y);
    color = &g_wldcore_window_render_records[index].color;
    color->b = color->g = color->r = 0x80;
    g_wldcore_window_render_records[index].anim_counter = 0;
    world_menu_build_window_frame_image(bounds.w, bounds.h, (u16*)image);
}

/* The VRAM position of a window image at `bounds` in coordinate mode `mode`.
 * Images are 4-bit, four texels to a VRAM halfword: modes 0 and 1 put them in
 * the texture pages at x 0x180 and 0x1c0, mode 2 in the page at (0x240, 0x100).
 *
 * The callee receives the adjacent position and dimensions registers as one
 * eight-byte aggregate. Dimensions are preserved by the ABI but unused here.
 */
static void wldcore_window_transform_point_for_mode(s32 mode, RECT bounds, point32_t* output) {
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
void wldcore_window_load_image_record_to_vram(s32 index, u32* image) {
    point32_t transformed;
    RECT rect;

    wldcore_window_transform_point_for_mode(g_wldcore_window_render_records[index].coordinate_mode,
        g_wldcore_window_render_records[index].bounds, &transformed);
    rect.x = transformed.x;
    rect.y = transformed.y;
    rect.w = g_wldcore_window_render_records[index].bounds.w / 4;
    rect.h = g_wldcore_window_render_records[index].bounds.h;
    LoadImage(&rect, image);
}
