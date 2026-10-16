/* Dialog helpers: text entry lookup, digit counting, window quad tweens and crops, the pause instruction and unit
 * screen projection. */

#include "fft/battle_camera.h"
#include "fft/battle_unit.h"
#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/menu_text.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/pad.h"
#include "psx/types.h"

static void menu_lerp_rect_to_poly_ft4(POLY_FT4* poly, const RECT* from, const RECT* to, s32 factor);

u8* text_find_entry(u32 text_id) {
    return (u8*)text_skip_to_entry(g_text_section_pointers[(text_id & TEXT_ID_SECTION_MASK) >> TEXT_ID_SECTION_SHIFT],
        text_id & TEXT_ID_ENTRY_MASK);
}

/* Counts magnitude digits (at least one), not a minus sign. */
s32 text_count_decimal_digits(s32 value) {
    s32 digits;

    value /= 10;
    digits = 1;
    while (value != 0) {
        value /= 10;
        digits++;
    }
    return digits;
}

void gfx_flip_poly_ft4_uv_if_negative(POLY_FT4* primitive, s32 direction) {
    u8 temp;

    if (direction < 0) {
        if (primitive->u0 != primitive->u1) {
            temp = primitive->u0;
            primitive->u0 = primitive->u1;
            primitive->u1 = temp;
            temp = primitive->u2;
            primitive->u2 = primitive->u3;
            primitive->u3 = temp;
        } else {
            temp = primitive->v0;
            primitive->v0 = primitive->v1;
            primitive->v1 = temp;
            temp = primitive->v2;
            primitive->v2 = primitive->v3;
            primitive->v3 = temp;
        }
    }
}

/* Grow the first window quad from the rectangle center using the cursor crop steps. */
void menu_animate_window_quad_crop(menu_window_buffer_t* buffers, RECT* rect, RECT* unused_to) {
    SPRT sprite;
    void* source; /* never assigned; the target passes an uninitialized $s5 */
    s32 level;
    u8 u = buffers->quads[0].u0;
    u8 v = buffers->quads[0].v0;
    s32 unused[2]; /* two unreferenced locals account for the 0x50 frame */

    for (level = 0; level < 12; level += g_script_event_speed) {
        sprite.u0 = u;
        sprite.v0 = v;
        sprite.x0 = rect->x;
        sprite.y0 = rect->y;
        sprite.w = rect->w;
        sprite.h = rect->h;
        menu_zoom_cursor_frame(source, &sprite, level);
        setXY4(&buffers[g_menu_packet_buffer_index].quads[0], sprite.x0, sprite.y0, sprite.x0 + (sprite.w - 2),
            sprite.y0, sprite.x0, sprite.y0 + sprite.h, sprite.x0 + (sprite.w - 2), sprite.y0 + sprite.h);
        setUV4(&buffers[g_menu_packet_buffer_index].quads[0], sprite.u0, sprite.v0, sprite.u0 + (sprite.w - 2),
            sprite.v0, sprite.u0, sprite.v0 + sprite.h, sprite.u0 + (sprite.w - 2), sprite.v0 + sprite.h);
        thread_yield();
    }
}

/* Move both window quads through the easing table; table 4 also fades their color. */
void menu_tween_window_quads(menu_window_buffer_t* buffers, RECT* to, RECT* to2, s32 table) {
    RECT from;
    RECT from2;
    u16* easing;
    s32 i;
    u32 step;
    s32 fade;
    u16 x = buffers->quads[0].x0;
    u16 y;
    s32 buffer_index;

    from.x = x;
    y = buffers->quads[0].y0;
    from.y = y;
    from.w = buffers->quads[0].x3 - x;
    from.h = buffers->quads[0].y3 - y;
    if (to2->x != 0) {
        x = buffers->quads[1].x0;
        from2.x = x;
        y = buffers->quads[1].y0;
        from2.y = y;
        from2.w = buffers->quads[1].x3 - x;
        from2.h = buffers->quads[1].y3 - y;
    }
    easing = g_menu_window_tween_easing_tables[table];
    do {
        step = *easing;
        for (i = 0; i < 3 - g_script_event_speed; i++) {
            fade = (ONE - step) >> 5;
            thread_yield();
            if (table == 4) {
                if (*easing != ONE) {
                    buffer_index = g_menu_packet_buffer_index;
                    buffers[buffer_index].color.b = fade;
                    buffers[buffer_index].color.g = fade;
                    buffers[buffer_index].color.r = fade;
                }
            }
            menu_lerp_rect_to_poly_ft4(&buffers[g_menu_packet_buffer_index].quads[0], &from, to, step);
            if (to2->x != 0) {
                menu_lerp_rect_to_poly_ft4(&buffers[g_menu_packet_buffer_index].quads[1], &from2, to2, step);
            }
        }
    } while (*easing++ != ONE);
    thread_yield();
    menu_lerp_rect_to_poly_ft4(&buffers[g_menu_packet_buffer_index].quads[0], &from, to, step);
    if (to2->x != 0) {
        menu_lerp_rect_to_poly_ft4(&buffers[g_menu_packet_buffer_index].quads[1], &from2, to2, step);
    }
}

/* Interpolate with a 20.12 factor; ONE reaches end. Division truncates toward zero. */
static s32 menu_lerp_fixed12(s32 start, s32 end, s32 factor) {
    return ((end - start) * factor) / ONE + start;
}

/* Set a quad's corners to the rectangle interpolated between from and to by a 12-bit fraction. */
static void menu_lerp_rect_to_poly_ft4(POLY_FT4* poly, const RECT* from, const RECT* to, s32 factor) {
    setXY4(poly, menu_lerp_fixed12(from->x, to->x, factor), menu_lerp_fixed12(from->y, to->y, factor),
        menu_lerp_fixed12(from->x + from->w, to->x + to->w, factor), menu_lerp_fixed12(from->y, to->y, factor),
        menu_lerp_fixed12(from->x, to->x, factor), menu_lerp_fixed12(from->y + from->h, to->y + to->h, factor),
        menu_lerp_fixed12(from->x + from->w, to->x + to->w, factor),
        menu_lerp_fixed12(from->y + from->h, to->y + to->h, factor));
}

/* Pause: yield until Select, Cross or Circle is pressed, or until the thread's
 * task id becomes the stop request. */
void script_opcode_pause(void) {
    do {
        thread_yield();
        if (thread_get_current_task_id() == THREAD_TASK_STOP_REQUEST) {
            return;
        }
    } while ((g_script_event_input & (PSX_PAD_SELECT | PSX_PAD_CROSS | PSX_PAD_CIRCLE)) == 0);
}

SVECTOR* unit_copy_world_coords_to_scratch(u32 runtime_id) {
    SVECTOR* source = battle_unit_get_world_coords_by_runtime_id(runtime_id);
    SVECTOR* dest;

    if (source != (SVECTOR*)-1) {
        dest = &g_unit_runtime_coords;
        dest->vx = source->vx;
        g_unit_runtime_coords.vy = source->vy;
        g_unit_runtime_coords.vz = source->vz;
        return dest;
    }
    return (SVECTOR*)-1;
}

/* Projects a unit's position to display coordinates (x, then y).
 *
 * The missing-unit test compares with -1, but
 * battle_unit_get_world_coords_by_runtime_id returns 0 for a missing unit, so the
 * projection then reads address 0. */
void unit_project_to_screen(u32 runtime_id, SVECTOR* screen_coords) {
    SVECTOR* world_coords = battle_unit_get_world_coords_by_runtime_id(runtime_id);

    if (world_coords != (SVECTOR*)-1) {
        battle_camera_project_world_coords_to_screen(world_coords, screen_coords);
    }
}

/* Copies a unit's integer world position as three 16-bit values.
 *
 * The missing-unit test compares with -1, but
 * battle_unit_get_world_coords_by_runtime_id returns 0 for a missing unit, so the
 * copy then reads address 0. */
void unit_copy_world_coords(u32 runtime_id, SVECTOR* world_coords) {
    SVECTOR* source = battle_unit_get_world_coords_by_runtime_id(runtime_id);

    if (source != (SVECTOR*)-1) {
        world_coords->vx = source->vx;
        world_coords->vy = source->vy;
        world_coords->vz = source->vz;
    }
}
