/*
 * WORLD copy of the EVENT OPTION dead-unit panel (option_menu_run_dead_unit_panel_b,
 * byte-identical apart from addresses). Layout notes below are the OPTION
 * twin's; the WORLD helpers are the same routines.
 */
#include "fft/event.h"
#include "fft/main_runtime.h"
#include "fft/open.h"
#include "fft/world.h"
#include "psx/types.h"

/* One libgpu texture window per DR_MODE packet. The static table already
   holds each window's w/h (0x10x0x10, 8x0x10, 0x10x8 and two full 0x100x0x100
   windows); this function only fills in the runtime x/y. Indexing the array
   is also what reproduces the target's symbol-plus-byte-offset addressing. */

void world_menu_run_dead_unit_panel_b(void) {
    RECT texture;
    /* The target holds the frame record base in one callee-saved register across
       the whole setup section. The address is a constant, so it picks up a
       REG_EQUIV note and reload rematerializes it at every use instead; the
       tied empty asm below breaks that equivalence. The two stride pointers are
       locals for the same reason: the target keeps each one live and re-derives
       the descriptor base from it (addiu -8) for the glyph calls. */
    world_panel_frame_t* prim = g_world_menu_panel_buffer.panel_frames;
    s32* source_stride = &g_world_gfx_image_blit_source.stride;
    s32* dest_stride = &g_world_gfx_image_blit_destination.stride;
    world_menu_window_t* menu;
    u8* pixels;
    s32 texture_u;
    s32 texture_v;
    /* The two border thicknesses feed both the primitive fields and the
       argument registers throughout, even though the callee at 0x80138adc
       takes only the primitive pointer:
       world_gfx_reset_record_texture_window reads none of them. */
    s32 border_wide;
    s32 border_thin;
    s32 span;
    s32 index;
    s32 frame;

    menu = world_thread_get_current_parameter_1();
    pixels = world_menu_alloc_ui_buffer(0x400);

    world_clear_menu_render_buffer(pixels, 0x400);
    texture.w = 0x80;
    texture.h = 0x10;
    world_gfx_alloc_texture_grid_rect(&texture, &prim->texture_prim, (u32*)-1);
    *source_stride = 0x100;
    *dest_stride = 0x80;

    world_gfx_set_image_blit_source_rect(8, 8, 0x20, 0x10);
    world_gfx_set_image_blit_destination_origin(0, 0);
    world_text_blit_glyph(
        g_world_menu_glyph_sheet, pixels, &g_world_gfx_image_blit_source, &g_world_gfx_image_blit_destination);
    world_gfx_set_image_blit_source_rect(0, 8, 8, 0x10);
    world_gfx_set_image_blit_destination_origin(0x20, 0);
    world_text_blit_glyph(
        g_world_menu_glyph_sheet, pixels, &g_world_gfx_image_blit_source, &g_world_gfx_image_blit_destination);
    world_gfx_set_image_blit_source_rect(0, 0, 0x28, 8);
    world_gfx_set_image_blit_destination_origin(0x28, 0);
    world_text_blit_glyph(
        g_world_menu_glyph_sheet, pixels, &g_world_gfx_image_blit_source, &g_world_gfx_image_blit_destination);
    world_gfx_set_image_blit_source_rect(0, 0x18, 0x28, 8);
    world_gfx_set_image_blit_destination_origin(0x28, 8);
    world_text_blit_glyph(
        g_world_menu_glyph_sheet, pixels, &g_world_gfx_image_blit_source, &g_world_gfx_image_blit_destination);

    LoadImage(&texture, (u32*)pixels);
    world_menu_init_sprite_array(prim->sprites, 9, 0x7c3c);
    texture_u = (texture.x & 0x3f) * 4;
    /* The target loads the returned rectangle's y once as a byte (lbu) and
       reuses it for every halfword and byte store below. */
    texture_v = (u8)texture.y;
    for (index = 0; index < 4; index++) {
        g_world_menu_panel_texture_windows_b[index].x = (texture_u + index) * 0x10;
        g_world_menu_panel_texture_windows_b[index].y = texture_v;
    }
    g_world_menu_panel_texture_windows_b[4].x = texture_u + 0x30;
    g_world_menu_panel_texture_windows_b[4].y = texture_v + 8;

    for (index = 0; index < 7; index++) {
        SetDrawMode(&prim->modes[index], 1, 0, GetTPage(0, 0, 0x1c0, 0), &g_world_menu_panel_texture_windows_b[index]);
    }

    border_wide = 0x10;
    border_thin = 8;
    prim->sprites[0].x0 = menu->x + 8;
    prim->sprites[0].y0 = menu->y + 8;
    /* The subtractions go through an s32 temporary: assigning the difference
       straight into the halfword field lets combine narrow it to HImode, where
       the negative constant no longer fits an addiu and is loaded instead. */
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

    world_gfx_reset_record_texture_window(&prim->cursor_mode);
    world_script_copy_bytes(&g_world_menu_panel_buffer.panel_frames[1], &g_world_menu_panel_buffer.panel_frames[0],
        sizeof(world_panel_frame_t));

    frame = 0;
    while (world_thread_get_current_parameter_3() == 0) {
        prim = &g_world_menu_panel_buffer.panel_frames[frame & 1];
        world_gfx_build_scaled_draw_area_pair_swapped(&prim->cursor_mode, menu, frame, g_active_graphics_buffer_index);
        /* The CLUT id the nine border sprites are drawn with this frame, which
           the target keeps in this variable's register once the texture column
           is dead. A separate local is allocated a different callee-saved
           register and does not reproduce the target. */
        texture_u = 0x7d3c;
        if (world_thread_get_current_parameter_2() == 0 && g_world_thread_task_active == 0) {
            texture_u = 0x7c3c;
        }
        for (index = 8; index >= 0; index--) {
            prim->sprites[index].clut = texture_u;
        }
        world_gfx_draw_or_append_gpu_primitive(&prim->cursor);
        world_gfx_draw_or_append_gpu_primitive(&prim->modes[6]);
        for (index = 0; index < 5; index++) {
            world_gfx_draw_or_append_gpu_primitive(&prim->sprites[index]);
            world_gfx_draw_or_append_gpu_primitive(&prim->modes[index]);
        }
        for (index = 8; index >= 5; index--) {
            world_gfx_draw_or_append_gpu_primitive(&prim->sprites[index]);
        }
        world_gfx_draw_or_append_gpu_primitive(&prim->modes[5]);
        world_gfx_draw_or_append_gpu_primitive(&prim->cursor_mode);
        frame++;
        world_thread_yield();
    }
    world_thread_yield();
    world_menu_free_memory(pixels);
    world_gfx_free_texture_grid_rect(&texture);
    world_thread_exit_current();
}
