#include "fft/script.h"
#include "fft/wldcore.h"
#include "psx/libgte.h"
#include "psx/types.h"

/*
 * Builds the 43 world-map location dots and queues them for drawing.
 *
 * Each dot takes its kind, sub-kind and map position from
 * g_wldcore_map_dot_sources; kind 2 uses the second dot sequence. A location
 * whose color script variable is set gets palette 0xc (locations 0..23) or
 * 0x10 (the rest), any other the default palette, and one whose visibility
 * variable is 0 is hidden. Resets the pulse state and projects the dots.
 */
void wldcore_map_color_and_draw_dots(void) {
    wldcore_map_dot_source_t* src;
    wldcore_map_dot_t* dots;
    wldcore_map_dot_t* dot;
    s32* flags_base;
    CVECTOR* color;
    s32 y;
    s32 is_castle;
    s32 queue_index;
    s32 i;

    dots = g_wldcore_map_dots;
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

/* Projects every world-map dot and marks the ones that are off screen, hidden
 * by system flag 0x10, or disabled by their script variable with flag 0x10.
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

/* The 0x40 compare limit reuses the dead `flags` local. With its own `limit`
 * variable the allocator swaps the g_wldcore_map_dot_pulse_direction snapshot ($a0 in the target) and
 * the limit ($v1); reusing the function-scope allocno reproduces the target
 * pair. */

void wldcore_map_pulse_dot_colors(void) {
    s32 flags;
    s32 i;
    s32 phase;
    s32 bright;
    s32 dark;
    u8 state;
    s32 counter;
    CVECTOR* color;

    flags = g_main_system_flags;
    if (flags & MAIN_SYSTEM_FLAG_HIDE_DISPLAY_OBJECTS) {
        return;
    }
    if (!(flags & MAIN_SYSTEM_FLAG_FREEZE_ANIMATION)) {
        state = g_wldcore_map_dot_pulse_direction;
        flags = 0x40;
        if (state == 0) {
            counter = g_wldcore_map_dot_pulse_phase + 1;
            g_wldcore_map_dot_pulse_phase = counter;
            counter = counter & 0xff;
            if (counter == flags) {
                g_wldcore_map_dot_pulse_direction = state ^ 1;
            }
        } else {
            counter = g_wldcore_map_dot_pulse_phase - 1;
            g_wldcore_map_dot_pulse_phase = counter;
            counter = counter & 0xff;
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

/* Pick the highest-priority visible map dot whose screen position contains the
 * given point, returned as a one-based index (0 when nothing is hit).
 *
 * The point is passed by value in a0/a1; the target reads both members back
 * from the incoming argument home slots. */
s32 wldcore_map_find_dot_at_point(point32_t point) {
    s32 best;
    s32 i;
    s32 hit;
    s32 value;

    best = 0xFFFFFF;
    hit = 0;
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
 * Location 0x16 offers a run of consecutive entries 0xb8ed + i, one per unit
 * counted by script variable 0x65, plus one. Every other location offers the
 * three fixed entries 0xb85d-0xb85f when its state record's is_town byte is 1, and
 * appends a fourth entry 0xb860 when script variable 0x90 is set and the
 * location is Dorter, Warjilis or Zarghidas (the fur shop).
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
