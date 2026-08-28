#include "fft/thread.h"
#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* g_world_map_selected_tile_data.display_height and .depth, bound separately:
 * the member spellings let GCC address both from one base and change the loop. */
extern u16 g_world_selected_tile_display_height;
extern s16 g_world_selected_tile_depth;

/* Display thread at 0x800e8ab0: draws the selected tile's quad group
 * (the selected tile's display height, built by world_menu_build_quad_pieces) every frame and, when
 * the tile depth is non-zero, renders it as a one-digit
 * number into the icon sprite.  Runs until the third thread parameter is set. */
void world_map_draw_selected_tile_info_thread(void) {
    RECT rect;
    u8 buffer[0x100];
    s32 i;
    s32 j;
    s32 frame;
    world_menu_quad_page_t* page;
    POLY_GT4* quad;

    g_world_menu_hide_numeric_values = 0;
    page = &world_D_80173CBC[0];
    world_gfx_set_image_draw_mode(&page->modes[0], 1);
    world_gfx_set_image_draw_mode(&page->modes[1], 0);
    for (i = 0; i < 5; i++) {
        quad = &page->quads[i];
        SetPolyGT4(quad);
        SetShadeTex(quad, 0);
        SetSemiTrans(quad, 1);
        quad->tpage = GetTPage(0, 0, 0x380, 0x120);
        quad->clut = 0x7CFC;
    }
    world_menu_init_sprite_array(page->sprites, 2, 0x7CBC);
    page->sprites[1].clut = 0x7D7C;
    for (i = 0; i < 2; i++) {
        world_gfx_init_image_loading((POLY_FT4*)&page->sprites[i], &g_world_gfx_menu_image_source,
            (const world_image_location_t*)g_world_tile_info_image_location, &g_world_tile_info_image_params[i]);
    }
    world_menu_init_icon_slot(&rect, 0x18, 0x10, (world_texture_prim_t*)&page->sprites[0], 0);
    world_script_copy_bytes(&world_D_80173CBC[1], &world_D_80173CBC[0], sizeof(world_menu_quad_page_t));

    for (frame = 0;; frame++) {
        s16* value = &g_world_selected_tile_depth;
        s32* stride;

        page = &world_D_80173CBC[frame & 1];
        i = world_menu_build_quad_pieces((s16)g_world_selected_tile_display_height, page->quads) - 1;
        world_thread_yield();
        if (world_thread_find_running_by_task(NATIVE_THREAD_TASK_DARK_SCREEN) != 0) {
            continue;
        }
        if (world_thread_find_running_by_task(NATIVE_THREAD_TASK_DARK_SCREEN_HOLD) != 0) {
            continue;
        }
        g_world_menu_hide_numeric_values = 0;
        if (world_thread_get_current_parameter_3() != 0) {
            break;
        }
        for (j = i; j >= 0; j--) {
            world_gfx_draw_or_append_gpu_primitive(&page->quads[j]);
        }
        if (*value != 0) {
            world_clear_menu_render_buffer(buffer, 0x100);
            stride = &g_world_menu_text_state.stride;
            *stride = 0x18;
            world_menu_set_text_origin(0x10, 0);
            world_draw_menu_number_glyphs(*value, 1, buffer, (world_glyph_blit_t*)((s16*)stride - 4));
            if (g_world_gfx_draw_suppress_flag == 0) {
                LoadImage(&rect, buffer);
                page->sprites[0].clut = 0x7CBC;
                world_gfx_draw_or_append_gpu_primitive(&page->sprites[0]);
                world_gfx_draw_or_append_gpu_primitive(&page->modes[0]);
                world_gfx_draw_or_append_gpu_primitive(&page->sprites[1]);
                world_gfx_draw_or_append_gpu_primitive(&page->modes[1]);
            }
        }
    }
    world_thread_yield();
    world_gfx_free_texture_grid_rect(&rect);
    world_thread_exit_current();
}
