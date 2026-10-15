#include "fft/gfx.h"
#include "fft/memory.h"
#include "fft/menu.h"
/*
 * WORLD copy of the EVENT OPTION window frame thread (option_menu_window_frame_thread,
 * byte-identical apart from addresses): the variant that allocates its scratch
 * pixel buffer, using the second WORLD blit descriptors and setters (as
 * world_screen_menu_thin_window_frame_thread) and its own texture-window table. Layout
 * notes below are the OPTION twin's; the WORLD helpers are the same routines.
 */
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

void world_screen_menu_window_frame_thread(void) {
    RECT texture;
    /* The stride pointers are locals because the target keeps each one live
       and re-derives the descriptor base from it (addiu -8) for the glyph
       calls. Unlike the OPTION/WORLD twins, this copy needs no register
       bindings or tied asm to match. */
    menu_panel_frame_t* prim = g_menu_scratch_buffer.panel_frames;
    s32* source_stride = &g_text_glyph_source.stride;
    s32* dest_stride = &g_text_glyph_dest.stride;
    urect16_t* menu;
    u8* pixels;
    s32 texture_u;
    s32 texture_v;
    s32 border_wide;
    s32 border_thin;
    s32 span;
    s32 index;
    s32 frame;

    menu = (urect16_t*)thread_get_current_parameter_1_variant();
    pixels = menu_alloc_memory(0x400);

    menu_clear_render_buffer(pixels, 0x400);
    texture.w = 0x80;
    texture.h = 0x10;
    gfx_alloc_texture_rect(&texture, &prim->texture_prim, (u32*)-1);
    *source_stride = 0x100;
    *dest_stride = 0x80;

    text_set_blit_source_rect(8, 8, 0x20, 0x10);
    world_gfx_set_blit_dest_position(0, 0);
    text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_text_glyph_source, &g_text_glyph_dest);
    text_set_blit_source_rect(0, 8, 8, 0x10);
    world_gfx_set_blit_dest_position(0x20, 0);
    text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_text_glyph_source, &g_text_glyph_dest);
    text_set_blit_source_rect(0xd8, 0, 0x28, 0x10);
    world_gfx_set_blit_dest_position(0x28, 0);
    text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_text_glyph_source, &g_text_glyph_dest);
    text_set_blit_source_rect(0xd8, 0x10, 0x28, 0x10);
    world_gfx_set_blit_dest_position(0x58, 0);
    text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_text_glyph_source, &g_text_glyph_dest);

    LoadImage(&texture, (u32*)pixels);
    menu_init_sprite_array(prim->sprites, 9, 0x7c3c);
    texture_u = (texture.x & 0x3f) * 4;
    texture_v = texture.y & 0xFF;
    for (index = 0; index < 4; index++) {
        g_world_screen_menu_panel_texture_windows[index].x = (texture_u + index) * 0x10;
        g_world_screen_menu_panel_texture_windows[index].y = texture_v;
    }
    g_world_screen_menu_panel_texture_windows[4].x = texture_u + 0x60;
    g_world_screen_menu_panel_texture_windows[4].y = texture_v;

    for (index = 0; index < 7; index++) {
        SetDrawMode(
            &prim->modes[index], 1, 0, GetTPage(0, 0, 0x1c0, 0), &g_world_screen_menu_panel_texture_windows[index]);
    }

    border_wide = 0x10;
    border_thin = 8;
    prim->sprites[0].x0 = menu->x + 8;
    prim->sprites[0].y0 = menu->y + 0x10;
    span = menu->w - 0x18;
    prim->sprites[0].w = span;
    span = menu->h - 0x20;
    prim->sprites[0].h = span;
    prim->sprites[1].x0 = menu->x + menu->w - 0x10;
    prim->sprites[1].y0 = menu->y + 0x10;
    prim->sprites[1].w = border_wide;
    span = menu->h - 0x20;
    prim->sprites[1].h = span;
    prim->sprites[2].x0 = menu->x;
    prim->sprites[2].y0 = menu->y + 0x10;
    prim->sprites[2].w = border_thin;
    span = menu->h - 0x20;
    prim->sprites[2].h = span;
    prim->sprites[3].x0 = menu->x + 8;
    prim->sprites[3].y0 = menu->y;
    span = menu->w - 0x18;
    prim->sprites[3].w = span;
    prim->sprites[3].h = border_wide;
    prim->sprites[4].x0 = menu->x + 8;
    prim->sprites[4].y0 = menu->y + menu->h - 0x10;
    span = menu->w - 0x18;
    prim->sprites[4].w = span;
    prim->sprites[4].h = border_wide;

    prim->sprites[5].x0 = menu->x;
    prim->sprites[5].y0 = menu->y;
    prim->sprites[5].u0 = texture_u + 0x28;
    prim->sprites[5].v0 = texture_v;
    prim->sprites[5].w = border_thin;
    prim->sprites[5].h = border_wide;
    prim->sprites[6].x0 = menu->x;
    prim->sprites[6].y0 = menu->y + menu->h - 0x10;
    prim->sprites[6].u0 = texture_u + 0x58;
    prim->sprites[6].v0 = texture_v;
    prim->sprites[6].w = border_thin;
    prim->sprites[6].h = border_wide;
    prim->sprites[7].x0 = menu->x + menu->w - 0x10;
    prim->sprites[7].y0 = menu->y;
    prim->sprites[7].u0 = texture_u + 0x40;
    prim->sprites[7].v0 = texture_v;
    prim->sprites[7].w = border_thin;
    prim->sprites[7].h = border_wide;
    prim->sprites[8].x0 = menu->x + menu->w - 0x10;
    prim->sprites[8].y0 = menu->y + menu->h - 0x10;
    prim->sprites[8].u0 = texture_u + 0x70;
    prim->sprites[8].v0 = texture_v;
    prim->sprites[8].w = border_thin;
    prim->sprites[8].h = border_wide;

    menu_init_scaled_draw_area_packets(&prim->draw_areas);
    mem_copy_bytes_nonempty(
        &g_menu_scratch_buffer.panel_frames[1], &g_menu_scratch_buffer.panel_frames[0], sizeof(menu_panel_frame_t));

    frame = 0;
    while (thread_get_current_parameter_3_variant() == 0) {
        prim = &g_menu_scratch_buffer.panel_frames[frame & 1];
        gfx_build_scaled_draw_area_pair_swapped(
            &prim->draw_areas, (const RECT*)menu, frame, g_active_graphics_buffer_index);
        texture_u = 0x7d3c;
        if (thread_get_current_parameter_2_variant() == 0 && g_menu_input_lock_mode == 0) {
            texture_u = 0x7c3c;
        }
        for (index = 8; index >= 0; index--) {
            prim->sprites[index].clut = texture_u;
        }
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
