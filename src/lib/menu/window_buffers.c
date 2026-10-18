/* Menu window-buffer ownership, primitive initialization and active-window drawing. */

#include "fft/gfx.h"
#include "fft/gfx_image.h"
#include "fft/gfx_primitive.h"
#include "fft/menu.h"
#include "fft/menu_gfx.h"
#include "fft/menu_window.h"
#include "fft/portrait.h"
#include "fft/screen.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "psx/libgpu.h"
#include "psx/types.h"

menu_window_buffer_t g_menu_window_buffers[6];
static menu_window_buffer_t* g_menu_window_buffer_slots[6];

static void menu_clear_window_buffer_pair_by_thread(s32 thread_id);

/* Empty in BATTLE and WORLD; menu_init_subsystems calls it. */
void menu_init_stub(void) {
}

/* Initialize a menu quad off-screen at x=-512, with neutral shading,
 * a 4-bit texture page at VRAM (448, 0) and the menu CLUT at (960, 496). */
void menu_init_default_poly_ft4(POLY_FT4* poly) {
    SetPolyFT4(poly);
    SetShadeTex(poly, 0);
    setRGB0(poly, 0x80, 0x80, 0x80);
    setUV4(poly, 0, 0, 1, 0, 0, 1, 1, 1);
    setXY4(poly, -0x200, 0, -0x200, 0, -0x200, 0, -0x200, 0);
    poly->tpage = GetTPage(0, 0, 0x1c0, 0);
    poly->clut = getClut(0x3c0, 0x1f0);
}

/* Initialize the fixed sprite of the menu graphics; only BATTLE shades its texture. */
void menu_init_sprt(SPRT* sprite) {
    SetSprt(sprite);
#ifdef FFT_BATTLE
    SetShadeTex(sprite, 1);
#else
    SetShadeTex(sprite, 0);
#endif
    setRGB0(sprite, 0x80, 0x80, 0x80);
    sprite->x0 = 0x200;
    sprite->u0 = 0;
    sprite->v0 = 0;
    sprite->w = 0;
    sprite->h = 0;
    sprite->y0 = 0;
    sprite->clut = MENU_CLUT_NORMAL;
    SetSemiTrans(sprite, 1);
}

/* menu_init_sprt for count consecutive sprites, each then given clut. */
void menu_init_sprite_array(SPRT* sprites, s32 count, s32 clut) {
    s32 index;

    for (index = 0; index < count; index++) {
        menu_init_sprt(sprites);
        sprites->clut = clut;
        sprites++;
    }
}

/* Mark all three menu-buffer pointer pairs as unallocated. */
void menu_reset_window_buffer_slots(void) {
    menu_window_buffer_t* value = (menu_window_buffer_t*)-1;
    s32 index;

    for (index = 5; index >= 0; index--) {
        g_menu_window_buffer_slots[index] = value;
    }
}

/* Claim the first free pair of the three window buffer pairs (one record per packet buffer)
 * for the current thread and return its first record. The retail loop runs once, so only
 * the first record's sprites, draw mode and quads are initialized here. When no pair is free
 * the current thread stops.
 *
 * CLUT ids encode (y << 6) | (x >> 4): the ones below are palettes at x = 960, rows
 * 498-505. */
menu_window_buffer_t* menu_alloc_window_buffer_pair(void) {
    menu_window_buffer_t* buffer;
    s32 index;
    s32 i;

    for (index = 0; index < 6; index += 2) {
        if (g_menu_window_buffer_slots[index] == (menu_window_buffer_t*)-1) {
            g_menu_window_buffer_slots[index] = &g_menu_window_buffers[index];
            g_menu_window_buffer_slots[index + 1] = &g_menu_window_buffers[index + 1];
            buffer = g_menu_window_buffer_slots[index];
            for (i = 0; i < 1; i++) {
                buffer->active = 0;
                buffer->thread_id = g_thread_current_id;
                buffer->portrait_code = -1;
                buffer->color.r = 0x80;
                buffer->color.g = 0x80;
                buffer->color.b = 0x80;
                buffer->dialogue_selector = 0;
                buffer->portrait_flipped = 0;
                buffer->is_message_box = 0;
                buffer->icon_flags = 0;
                buffer->scroll_range = 1;
                buffer->scroll_position = 1;
                menu_init_sprite_array(buffer->sprites, 4, MENU_CLUT_NORMAL);
                menu_init_sprite_array(buffer->icon_sprites, 3, 0x7e7c);
                SetDrawMode(&buffer->draw_mode, 0, 0, GetTPage(0, 2, 0x3c0, 0x100), &g_gfx_texture_window);
                SetSemiTrans(&buffer->sprites[0], 0);
                SetSemiTrans(&buffer->sprites[1], 1);
                buffer->sprites[0].u0 = 0xa8;
                buffer->sprites[1].u0 = 0xb8;
                buffer->sprites[0].clut = 0x7d7c;
                buffer->sprites[1].clut = 0x7dbc;
                buffer->sprites[2].clut = 0x7cbc;
                buffer->icon_sprites[0].u0 = 0xd8;
                buffer->sprites[0].w = 0x10;
                buffer->sprites[0].h = 0x10;
                buffer->sprites[1].w = 0x10;
                buffer->sprites[1].h = 0x10;
                buffer->sprites[2].w = 0x10;
                buffer->sprites[2].h = 0x10;
                buffer->sprites[0].v0 = 0;
                buffer->sprites[1].v0 = 0;
                buffer->icon_sprites[0].w = 8;
                buffer->icon_sprites[0].h = 0x10;
                buffer->icon_sprites[1].w = 8;
                buffer->icon_sprites[1].h = 0x10;
                buffer->icon_sprites[2].w = 8;
                buffer->icon_sprites[2].h = 8;
                buffer->icon_sprites[0].v0 = 0;
                buffer->icon_sprites[1].u0 = 0xe0;
                buffer->icon_sprites[1].v0 = 0;
                buffer->icon_sprites[2].u0 = 0x10;
                buffer->icon_sprites[2].v0 = 0x10;
                menu_init_default_poly_ft4(&buffer->quads[0]);
                menu_init_default_poly_ft4(&buffer->quads[1]);
                buffer->quads[0].tpage = GetTPage(0, 0, 0x3c0, 0x100);
                buffer->quads[1].tpage = GetTPage(0, 0, 0x340, 0x100);
                buffer++;
            }
            return g_menu_window_buffer_slots[index];
        }
    }
    thread_exit_current();
}

/* Clear the pair whose first record's thread matches; the target forms one
 * 0x118-biased induction value across both records (compare 0x800e27f4).
 * BATTLE's copy is empty and never called. */
static void menu_clear_window_buffer_pair_by_thread(s32 thread_id) {
#ifdef FFT_WORLD
    s32 index;

    for (index = 0; index < 6; index += 2) {
        if (g_menu_window_buffers[index].thread_id == thread_id) {
            g_menu_window_buffers[index].active = 0;
            g_menu_window_buffers[index + 1].active = 0;
            return;
        }
    }
#endif
}

/* Release a pair handed out by menu_alloc_window_buffer_pair: free both slots and mark both
 * records inactive and unowned. -1 is ignored; a pointer that is not a pair's first record stops
 * the current thread. */
void menu_release_window_buffer_pair(menu_window_buffer_t* buffer) {
    s32 slot;
    menu_window_buffer_t* empty;

    if (buffer == (menu_window_buffer_t*)-1) {
        return;
    }

    slot = 0;
    empty = (menu_window_buffer_t*)-1;
    for (; slot < 6; slot += 2) {
        if (g_menu_window_buffer_slots[slot] == buffer) {
            g_menu_window_buffer_slots[slot] = empty;
            g_menu_window_buffer_slots[slot + 1] = empty;
            g_menu_window_buffers[slot].active = 0;
            g_menu_window_buffers[slot + 1].active = 0;
            g_menu_window_buffers[slot].thread_id = 0;
            g_menu_window_buffers[slot + 1].thread_id = 0;
            return;
        }
    }

    thread_exit_current();
}

/* Set the color in a GPU primitive's common header to one gray level. */
static void gfx_set_primitive_brightness(P_TAG* primitive, s32 brightness) {
    setRGB0(primitive, brightness, brightness, brightness);
}

/* Draws the active menu windows, last slot pair first. A window is drawn
 * while its buffer is active and its owner thread runs; BATTLE scales its
 * frame primitives by the window's own brightness, WORLD by the one menu
 * brightness, and WORLD dims a message box while a menu task is active. The
 * scroll icons sit 15 pixels in from the right edge (13 when the width is not
 * 4 more than a multiple of 8), shifted 4 pixels up or down for a portrait
 * box, and the scroll marker lies between them in proportion to
 * scroll_position / scroll_range. The portrait quad takes its texture from
 * portrait_code, or in BATTLE from the unit's portrait when that is -1. */
#ifdef FFT_WORLD
#    define WINDOW_BRIGHTNESS g_screen_color_red
#else
#    define WINDOW_BRIGHTNESS window->color.r
#endif
void menu_draw_active_window_frames(void) {
    menu_window_buffer_t* window;
    menu_window_buffer_t* buffer;
    s32 height;
    s32 i;
    s32 offset;
    s32 top;
    s32 span;
    s32 flags;
    s32 bottom;
#ifdef FFT_BATTLE
    s32 u;
#endif

    for (i = 4; i >= 0; i -= 2) {
        if (g_menu_window_buffer_slots[i] == (menu_window_buffer_t*)-1) {
            continue;
        }
        buffer = g_menu_window_buffer_slots[i + g_menu_packet_buffer_index];
        if (buffer->active != 1 || g_thread_contexts[buffer->thread_id].is_running == 0) {
            continue;
        }
        window = buffer;
        gfx_set_primitive_brightness((P_TAG*)&window->sprites[0], WINDOW_BRIGHTNESS);
        gfx_set_primitive_brightness((P_TAG*)&window->sprites[1], WINDOW_BRIGHTNESS);
        gfx_set_primitive_brightness((P_TAG*)&window->sprites[2], WINDOW_BRIGHTNESS);
        gfx_set_primitive_brightness((P_TAG*)&window->sprites[3], WINDOW_BRIGHTNESS);
        gfx_set_primitive_brightness((P_TAG*)&window->quads[0], WINDOW_BRIGHTNESS);
        gfx_set_primitive_brightness((P_TAG*)&window->quads[1], WINDOW_BRIGHTNESS);
#ifdef FFT_WORLD
        if (window->is_message_box != 0) {
            if (g_menu_input_lock_mode != 0) {
                window->quads[0].clut = MENU_CLUT_DIMMED;
                setRGB0(&window->quads[1], 0x40, 0x50, 0x70);
            } else {
                window->quads[0].clut = MENU_CLUT_NORMAL;
            }
        }
#endif
        offset = window->quads[0].x1 - window->quads[0].x0;
        if (offset / 8 * 8 == offset - 4) {
            offset -= 15;
        } else {
            offset -= 13;
        }
        height = window->quads[0].y2 - window->quads[0].y0;
        window->icon_sprites[0].x0 = window->quads[0].x0 + offset;
        window->icon_sprites[0].y0 = window->quads[0].y0 + 8;
        window->icon_sprites[1].x0 = window->quads[0].x0 + offset;
        window->icon_sprites[1].y0 = window->quads[0].y0 + height - 0x1A;
        if (window->box_type == DIALOG_BOX_TYPE_PORTRAIT) {
            if (window->dialogue_selector == 1) {
                window->icon_sprites[0].y0 -= 4;
                window->icon_sprites[1].y0 -= 4;
            } else {
                window->icon_sprites[0].y0 += 4;
                window->icon_sprites[1].y0 += 4;
            }
        }
        bottom = window->icon_sprites[1].y0;
        top = window->icon_sprites[0].y0 + window->icon_sprites[0].h;
        bottom -= top;
        /* Reload the pointer cell: retaining buffer removes target loads. */
        span = (bottom - 4) * g_menu_window_buffer_slots[i + g_menu_packet_buffer_index]->scroll_position
            / g_menu_window_buffer_slots[i + g_menu_packet_buffer_index]->scroll_range;
        window->icon_sprites[2].x0 = window->quads[0].x0 + offset;
        window->icon_sprites[2].y0 = top + span;
        flags = g_menu_window_buffer_slots[i + g_menu_packet_buffer_index]->icon_flags;
        if (flags & MENU_WINDOW_ICON_SCROLL_UP) {
            gfx_draw_or_append_gpu_primitive(&window->icon_sprites[0]);
        }
        if (flags & MENU_WINDOW_ICON_SCROLL_DOWN) {
            gfx_draw_or_append_gpu_primitive(&window->icon_sprites[1]);
        }
        if (flags & MENU_WINDOW_ICON_SCROLL_MARKER) {
            gfx_draw_or_append_gpu_primitive(&window->icon_sprites[2]);
        }
        gfx_draw_or_append_gpu_primitive(&window->sprites[0]);
        gfx_draw_or_append_gpu_primitive(&window->sprites[1]);
        gfx_draw_or_append_gpu_primitive(&window->sprites[2]);
        gfx_draw_or_append_gpu_primitive(&window->sprites[3]);
        gfx_draw_or_append_gpu_primitive(&window->draw_mode);
        if (window->portrait_code != -1) {
#ifdef FFT_BATTLE
            /* Portrait slot n is 32 x 48 pixels at u = 32 n of the portrait page, with its
             * CLUT at x = 0x1C0 + 16 (n % 4), y = 0xFE + n / 4. */
            u = window->portrait_code << 5;
            setUV4(&window->quads[1], u, 0xCE, u + 0x1F, 0xCE, u, 0xFE, u + 0x1F, 0xFE);
            window->quads[1].tpage = GetTPage(0, 0, 0x1C0, 0);
            window->quads[1].clut = GetClut(window->portrait_code % 4 * 16 + 0x1C0, window->portrait_code / 4 + 0xFE);
#else
            portrait_build_wldface_poly(&window->quads[1], window->portrait_code);
#endif
        }
#ifdef FFT_BATTLE
        else {
            portrait_build_unit_poly(&window->quads[1], window->portrait_battle_id);
        }
        gfx_flip_poly_ft4_uv_if_negative(&window->quads[1], -window->portrait_flipped);
#endif
        gfx_draw_or_append_gpu_primitive(&window->quads[1]);
        gfx_draw_or_append_gpu_primitive(&window->quads[0]);
    }
}
