/* The thread drawing a window frame of nine thin border sprites until asked to stop. */

#include "fft/gfx.h"
#include "fft/gfx_image.h"
#include "fft/gfx_primitive.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/menu_gfx.h"
#include "fft/menu_input.h"
#include "fft/menu_window.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Texture windows of the frame pieces this thread uploads. */
extern RECT g_menu_thin_panel_texture_windows[7];

/* menu_window_frame_thread with thin border pieces: the same nine-sprite frame, but its top and
 * bottom edges are 8 pixels tall instead of 16, and the border image is built in a buffer
 * allocated from the menu arena.
 *
 * One libgpu texture window per DR_MODE packet: the static table already holds each window's
 * w/h; this function only fills in the runtime x/y. Indexing the array is also what reproduces
 * the target's symbol-plus-byte-offset addressing. */
void menu_thin_window_frame_thread(void) {
    RECT texture;
    /* Keeping both stride fields live retains their descriptor bases. */
    menu_panel_frame_t* prim = g_menu_scratch_buffer.panel_frames;
    s32* source_stride = &g_text_glyph_source.stride;
    s32* dest_stride = &g_text_glyph_dest.stride;
    urect16_t* menu;
    u8* pixels;
    s32 texture_u;
    s32 texture_v;
    /* The two border thicknesses feed both the primitive fields and the
     * argument registers throughout, even though
     * menu_init_scaled_draw_area_packets takes only the primitive pointer
     * and reads none of them. */
    s32 border_wide;
    s32 border_thin;
    s32 span;
    s32 index;
    s32 frame;

    menu = (urect16_t*)thread_get_current_parameter_1();
    pixels = menu_alloc_memory(0x400);

    menu_clear_render_buffer(pixels, 0x400);
    texture.w = 0x80;
    texture.h = 0x10;
    gfx_alloc_texture_rect(&texture, &prim->texture_prim, (u32*)-1);
    *source_stride = 0x100;
    *dest_stride = 0x80;

    /* Copy the border pieces from the menu glyph sheet into one 128x16 image and upload it. */
    text_set_blit_source_rect(8, 8, 0x20, 0x10);
    text_set_blit_destination(0, 0);
    text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_text_glyph_source, &g_text_glyph_dest);
    text_set_blit_source_rect(0, 8, 8, 0x10);
    text_set_blit_destination(0x20, 0);
    text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_text_glyph_source, &g_text_glyph_dest);
    text_set_blit_source_rect(0, 0, 0x28, 8);
    text_set_blit_destination(0x28, 0);
    text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_text_glyph_source, &g_text_glyph_dest);
    text_set_blit_source_rect(0, 0x18, 0x28, 8);
    text_set_blit_destination(0x28, 8);
    text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_text_glyph_source, &g_text_glyph_dest);

    LoadImage(&texture, (u32*)pixels);
    menu_init_sprite_array(prim->sprites, 9, MENU_CLUT_NORMAL);

    texture_u = (texture.x & 0x3f) * 4;
    /* The target loads the returned rectangle's y once as a byte (lbu) and
     * reuses it for every 16-bit and byte store below. */
    texture_v = texture.y & 0xFF;
    for (index = 0; index < 4; index++) {
        g_menu_thin_panel_texture_windows[index].x = (texture_u + index) * 0x10;
        g_menu_thin_panel_texture_windows[index].y = texture_v;
    }
    g_menu_thin_panel_texture_windows[4].x = texture_u + 0x30;
    g_menu_thin_panel_texture_windows[4].y = texture_v + 8;

    for (index = 0; index < 7; index++) {
        SetDrawMode(&prim->modes[index], 1, 0, GetTPage(0, 0, 0x1c0, 0), &g_menu_thin_panel_texture_windows[index]);
    }

    /* Sprite 0 fills the interior and sprites 1-4 are the right, left, top and bottom edges;
     * each tiles its piece through its own texture window. */
    border_wide = 0x10;
    border_thin = 8;
    prim->sprites[0].x0 = menu->x + 8;
    prim->sprites[0].y0 = menu->y + 8;
    /* The subtractions go through an s32 temporary: assigning the difference
     * straight into the 16-bit field lets combine narrow it to HImode, where
     * the negative constant no longer fits an addiu and is loaded instead. */
    span = menu->w - 0x18;
    prim->sprites[0].w = span;
    span = menu->h - 0x10;
    prim->sprites[0].h = span;
    prim->sprites[1].x0 = menu->x + menu->w - 0x10;
    prim->sprites[1].y0 = menu->y + 8;
    prim->sprites[1].w = border_wide;
    span = menu->h - 0x10;
    prim->sprites[1].h = span;
    prim->sprites[2].x0 = menu->x;
    prim->sprites[2].y0 = menu->y + 8;
    prim->sprites[2].w = border_thin;
    span = menu->h - 0x10;
    prim->sprites[2].h = span;
    prim->sprites[3].x0 = menu->x + 8;
    prim->sprites[3].y0 = menu->y;
    span = menu->w - 0x18;
    prim->sprites[3].w = span;
    prim->sprites[3].h = border_thin;
    prim->sprites[4].x0 = menu->x + 8;
    prim->sprites[4].y0 = menu->y + menu->h - 8;
    span = menu->w - 0x18;
    prim->sprites[4].w = span;
    prim->sprites[4].h = border_thin;

    /* Sprites 5-8 are the corners, cut from the corner pieces of the uploaded image. */
    prim->sprites[5].x0 = menu->x;
    prim->sprites[5].y0 = menu->y;
    prim->sprites[5].u0 = texture_u + 0x28;
    prim->sprites[5].v0 = texture_v;
    prim->sprites[5].w = border_thin;
    prim->sprites[5].h = border_thin;
    prim->sprites[6].x0 = menu->x;
    prim->sprites[6].y0 = menu->y + menu->h - 8;
    prim->sprites[6].u0 = texture_u + 0x28;
    prim->sprites[6].v0 = texture_v + 8;
    prim->sprites[6].w = border_thin;
    prim->sprites[6].h = border_thin;
    prim->sprites[7].x0 = menu->x + menu->w - 0x10;
    prim->sprites[7].y0 = menu->y;
    prim->sprites[7].u0 = texture_u + 0x40;
    prim->sprites[7].v0 = texture_v;
    prim->sprites[7].w = border_wide;
    prim->sprites[7].h = border_thin;
    prim->sprites[8].x0 = menu->x + menu->w - 0x10;
    prim->sprites[8].y0 = menu->y + menu->h - 8;
    prim->sprites[8].u0 = texture_u + 0x40;
    prim->sprites[8].v0 = texture_v + 8;
    prim->sprites[8].w = border_wide;
    prim->sprites[8].h = border_thin;

    menu_init_scaled_draw_area_packets(&prim->draw_areas);
    mem_copy_bytes_nonempty(
        &g_menu_scratch_buffer.panel_frames[1], &g_menu_scratch_buffer.panel_frames[0], sizeof(menu_panel_frame_t));

    frame = 0;
    while (thread_get_current_parameter_3() == 0) {
        prim = &g_menu_scratch_buffer.panel_frames[frame & 1];
        gfx_build_scaled_draw_area_pair_swapped(&prim->draw_areas, (const RECT*)menu, frame, g_gfx_screen_polarity);

        /* The CLUT id the nine border sprites are drawn with this frame, which
         * the target keeps in this variable's register once the texture column
         * is dead. A separate local is allocated a different callee-saved
         * register and does not reproduce the target. */
        texture_u = MENU_CLUT_DIMMED;
        if (thread_get_current_parameter_2() == 0 && g_menu_input_lock_mode == 0) {
            texture_u = MENU_CLUT_NORMAL;
        }
        for (index = 8; index >= 0; index--) {
            prim->sprites[index].clut = texture_u;
        }

        /* Linked in reverse draw order: each edge's texture-window mode draws just before its
         * sprite, modes[5] before the corners, and modes[6] and areas[1] restore the defaults last. */
        gfx_draw_or_append_gpu_primitive(&prim->draw_areas.areas[1]);
        gfx_draw_or_append_gpu_primitive(&prim->modes[6]);
        for (index = 0; index < 5; index++) {
            gfx_draw_or_append_gpu_primitive(&prim->sprites[index]);
            gfx_draw_or_append_gpu_primitive(&prim->modes[index]);
        }
        for (index = 8; index >= 5; index--) {
            gfx_draw_or_append_gpu_primitive(&prim->sprites[index]);
        }
        gfx_draw_or_append_gpu_primitive(&prim->modes[5]);
        gfx_draw_or_append_gpu_primitive(&prim->draw_areas.areas[0]);

        frame++;
        thread_yield();
    }

    thread_yield();
    menu_free_memory(pixels);
    gfx_free_texture_rect(&texture);
    thread_exit_current();
}
