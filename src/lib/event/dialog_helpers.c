/* Dialog helpers: text entry lookup, digit counting, window quad tweens and crops, the pause instruction and unit
 * screen projection. */

#include "fft/camera.h"
#include "fft/gfx.h"
#include "fft/gfx_primitive.h"
#include "fft/menu_cursor.h"
#include "fft/menu_gfx.h"
#include "fft/menu_window.h"
#include "fft/script.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/unit_event.h"
#include "fft/unit_runtime.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Window interpolation fractions; ONE ends each curve. The first two overshoot
 * the final size, while the last curve also drives the closing fade. */
static u16 g_menu_window_tween_small_overshoot[10] = { 832, 1600, 2304, 2944, 3520, 4032, 4192, 4160, 4128, ONE };
static u16 g_menu_window_tween_large_overshoot[11]
    = { 1112, 2184, 3216, 4208, 4608, 4512, 4416, 4320, 4224, 4128, ONE };
static u16 g_menu_window_tween_linear[11] = { 384, 768, 1152, 1536, 1920, 2304, 2688, 3072, 3456, 3840, ONE };
static u16 g_menu_window_tween_ease_out[7] = { 832, 1600, 2304, 2944, 3520, 4032, ONE };
static u16 g_menu_window_tween_fade[4] = { 1024, 2048, 3072, ONE };

static u16* g_menu_window_tween_easing_tables[5]
    = { g_menu_window_tween_small_overshoot, g_menu_window_tween_large_overshoot, g_menu_window_tween_linear,
          g_menu_window_tween_ease_out, g_menu_window_tween_fade };

static void menu_lerp_rect_to_poly_ft4(POLY_FT4* poly, const RECT* from, const RECT* to, s32 factor);

/* Return the start of text entry text_id: its section field picks the section pointer and its
 * entry field the entry within that section. */
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

/* For a negative direction, mirror the quad's texture: swap u0/u1 and u2/u3, or the matching v
 * values when the texture runs vertically (u0 == u1). */
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

/* Zoom the current packet buffer's window quad open: for crop levels 0-11, stepping by the
 * event speed with one frame each, menu_zoom_cursor_frame crops rect around its center and
 * the quad's corners and UVs follow the crop, 2 pixels narrower. */
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

/* Tween the window quad from its current rectangle to `to` (and the portrait quad to `to2`
 * when to2->x is nonzero) along easing table `table`, whose 12-bit fractions end at ONE.
 * Each step lasts 3 - event speed frames. Table 4 also fades the window color with the
 * easing, from 128 toward 0. */
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
static s32 menu_lerp_q12(s32 start, s32 end, s32 factor) {
    return ((end - start) * factor) / ONE + start;
}

/* Set a quad's corners to the rectangle interpolated between from and to by a 12-bit fraction. */
static void menu_lerp_rect_to_poly_ft4(POLY_FT4* poly, const RECT* from, const RECT* to, s32 factor) {
    setXY4(poly, menu_lerp_q12(from->x, to->x, factor), menu_lerp_q12(from->y, to->y, factor),
        menu_lerp_q12(from->x + from->w, to->x + to->w, factor), menu_lerp_q12(from->y, to->y, factor),
        menu_lerp_q12(from->x, to->x, factor), menu_lerp_q12(from->y + from->h, to->y + to->h, factor),
        menu_lerp_q12(from->x + from->w, to->x + to->w, factor),
        menu_lerp_q12(from->y + from->h, to->y + to->h, factor));
}

/* Pause: yield until Select, Cross or Circle is pressed, or until the thread's
 * task id becomes the stop request. */
void script_opcode_16_pause(void) {
    do {
        thread_yield();
        if (thread_get_current_task_id() == THREAD_TASK_STOP_REQUEST) {
            return;
        }
    } while ((g_script_event_input & (PSX_PAD_SELECT | PSX_PAD_CROSS | PSX_PAD_CIRCLE)) == 0);
}

/* Copy a unit's integer world position into the shared g_unit_runtime_coords and return it. Like
 * unit_copy_world_coords it tests for -1, but a missing unit returns 0, so the copy then reads
 * address 0. */
SVECTOR* unit_copy_world_coords_to_scratch(u32 runtime_id) {
    SVECTOR* source = unit_get_world_coords_by_runtime_id(runtime_id);
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
 * unit_get_world_coords_by_runtime_id returns 0 for a missing unit, so the
 * projection then reads address 0. */
void unit_project_to_screen(u32 runtime_id, SVECTOR* screen_coords) {
    SVECTOR* world_coords = unit_get_world_coords_by_runtime_id(runtime_id);

    if (world_coords != (SVECTOR*)-1) {
        camera_project_world_coords_to_screen(world_coords, screen_coords);
    }
}

/* Copies a unit's integer world position as three 16-bit values.
 *
 * The missing-unit test compares with -1, but
 * unit_get_world_coords_by_runtime_id returns 0 for a missing unit, so the
 * copy then reads address 0. */
void unit_copy_world_coords(u32 runtime_id, SVECTOR* world_coords) {
    SVECTOR* source = unit_get_world_coords_by_runtime_id(runtime_id);

    if (source != (SVECTOR*)-1) {
        world_coords->vx = source->vx;
        world_coords->vy = source->vy;
        world_coords->vz = source->vz;
    }
}
