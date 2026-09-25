/*
 * OPTION dead-unit panel, variant A.
 *
 * Twin of option_menu_run_dead_unit_panel_b at 0x801c0894; that panel allocates its
 * 4bpp scratch image instead of using the fixed one, uses the second texture-window table,
 * and lays the border out with 8-pixel instead of 16-pixel corners.
 */
#include "fft/event_option.h"
#include "psx/types.h"

union battle_texture_prim;

void option_menu_run_dead_unit_panel_a(void) {
    RECT texture;
    /* The target holds the frame record base in one callee-saved register across
       the whole setup section. The address is a constant, so it picks up a
       REG_EQUIV note and reload rematerializes it at every use instead; the
       tied empty asm below breaks that equivalence. The two stride pointers are
       locals for the same reason: the target keeps each one live and re-derives
       the descriptor base from it (addiu -8) for the glyph calls. */
    option_panel_frame_t* prim = g_option_menu_panel_frames;
    s32* source_stride = &g_option_text_glyph_source.stride;
    s32* dest_stride = &g_option_text_glyph_dest.stride;
    /* The menu thread's first parameter. Only its leading window rectangle is
       used; every read in the target is an lhu. */
    urect16_t* menu;
    s32 texture_u;
    s32 texture_v;
    /* The two border thicknesses are loaded once and shared by every border
       sprite. */
    s32 border_wide;
    s32 border_thin;
    s32 size;
    s32 index;
    s32 frame;

    menu = battle_thread_get_current_parameter_1();

    battle_clear_menu_render_buffer(g_option_menu_panel_pixels, 0x400);
    texture.w = 0x80;
    texture.h = 0x10;
    battle_gfx_alloc_tpage7_vram(&texture, (union battle_texture_prim*)prim->texture_prim, (u32*)-1);
    *source_stride = 0x100;
    *dest_stride = 0x80;

    option_text_set_blit_source_rect(8, 8, 0x20, 0x10);
    option_text_set_blit_destination(0, 0);
    blit_text_glyph(
        g_battle_menu_glyph_image, g_option_menu_panel_pixels, &g_option_text_glyph_source, &g_option_text_glyph_dest);
    option_text_set_blit_source_rect(0, 8, 8, 0x10);
    option_text_set_blit_destination(0x20, 0);
    blit_text_glyph(
        g_battle_menu_glyph_image, g_option_menu_panel_pixels, &g_option_text_glyph_source, &g_option_text_glyph_dest);
    option_text_set_blit_source_rect(0xd8, 0, 0x28, 0x10);
    option_text_set_blit_destination(0x28, 0);
    blit_text_glyph(
        g_battle_menu_glyph_image, g_option_menu_panel_pixels, &g_option_text_glyph_source, &g_option_text_glyph_dest);
    option_text_set_blit_source_rect(0xd8, 0x10, 0x28, 0x10);
    option_text_set_blit_destination(0x58, 0);
    blit_text_glyph(
        g_battle_menu_glyph_image, g_option_menu_panel_pixels, &g_option_text_glyph_source, &g_option_text_glyph_dest);

    LoadImage(&texture, (u32*)g_option_menu_panel_pixels);
    battle_menu_init_sprite_array(prim->sprites, 9, 0x7c3c);
    texture_u = (texture.x & 0x3f) * 4;
    /* The target loads the returned rectangle's y once as a byte (lbu 0x1a(sp))
       and reuses it for every halfword and byte store below. */
    texture_v = (u8)texture.y;
    for (index = 0; index < 4; index++) {
        g_option_menu_panel_texture_windows[index].x = (texture_u + index) * 0x10;
        g_option_menu_panel_texture_windows[index].y = texture_v;
    }
    g_option_menu_panel_texture_windows[4].x = texture_u + 0x60;
    g_option_menu_panel_texture_windows[4].y = texture_v;

    for (index = 0; index < 7; index++) {
        SetDrawMode(&prim->modes[index], 1, 0, GetTPage(0, 0, 0x1c0, 0), &g_option_menu_panel_texture_windows[index]);
    }

    border_wide = 0x10;
    border_thin = 8;
    prim->sprites[0].x0 = menu->x + 8;
    prim->sprites[0].y0 = menu->y + 0x10;
    /* The subtractions go through an s32 temporary: assigning the difference
       straight into the halfword field lets combine narrow it to HImode, where
       the negative constant no longer fits an addiu and is loaded instead. */
    size = menu->w - 0x18;
    prim->sprites[0].w = size;
    size = menu->h - 0x20;
    prim->sprites[0].h = size;
    prim->sprites[1].x0 = menu->x + menu->w - 0x10;
    prim->sprites[1].y0 = menu->y + 0x10;
    prim->sprites[1].w = border_wide;
    size = menu->h - 0x20;
    prim->sprites[1].h = size;
    prim->sprites[2].x0 = menu->x;
    prim->sprites[2].y0 = menu->y + 0x10;
    prim->sprites[2].w = border_thin;
    size = menu->h - 0x20;
    prim->sprites[2].h = size;
    prim->sprites[3].x0 = menu->x + 8;
    prim->sprites[3].y0 = menu->y;
    size = menu->w - 0x18;
    prim->sprites[3].w = size;
    prim->sprites[3].h = border_wide;
    prim->sprites[4].x0 = menu->x + 8;
    prim->sprites[4].y0 = menu->y + menu->h - 0x10;
    size = menu->w - 0x18;
    prim->sprites[4].w = size;
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

    battle_menu_set_disabled_texture_window(&prim->cursor_mode);
    battle_copy_bytes(&g_option_menu_panel_frames[1], &g_option_menu_panel_frames[0], sizeof(option_panel_frame_t));

    frame = 0;
    while (battle_thread_get_current_parameter_3() == 0) {
        prim = &g_option_menu_panel_frames[frame & 1];
        battle_menu_build_zoom_draw_area_pair(&prim->cursor_mode, menu, frame, g_main_gfx_screen_polarity);
        /* The CLUT id the nine border sprites are drawn with this frame, which
           the target keeps in this variable's register once the texture column
           is dead. A separate local is allocated a different callee-saved
           register and does not reproduce the target. */
        texture_u = 0x7d3c;
        if (battle_thread_get_current_parameter_2() == 0 && g_event_mode == 0) {
            texture_u = 0x7c3c;
        }
        for (index = 8; index >= 0; index--) {
            prim->sprites[index].clut = texture_u;
        }
        battle_gfx_draw_or_append_gpu_primitive(&prim->cursor);
        battle_gfx_draw_or_append_gpu_primitive(&prim->modes[6]);
        for (index = 0; index < 5; index++) {
            battle_gfx_draw_or_append_gpu_primitive(&prim->sprites[index]);
            battle_gfx_draw_or_append_gpu_primitive(&prim->modes[index]);
        }
        for (index = 8; index >= 5; index--) {
            battle_gfx_draw_or_append_gpu_primitive(&prim->sprites[index]);
        }
        battle_gfx_draw_or_append_gpu_primitive(&prim->modes[5]);
        battle_gfx_draw_or_append_gpu_primitive(&prim->cursor_mode);
        frame++;
        battle_thread_yield();
    }
    battle_thread_yield();
    battle_gfx_free_tpage7_vram(&texture);
    battle_thread_exit_current();
}
