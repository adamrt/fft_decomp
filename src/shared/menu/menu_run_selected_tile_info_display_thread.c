#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_menu.h"
#include "fft/battle_text.h"
#include "fft/gfx.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/thread.h"
#include "psx/libgpu.h"

/* Draw the selected tile height and depth while its panel thread is active. */
void menu_run_selected_tile_info_display_thread(void) {
    RECT rect;
    u8 buffer[0x100];
    s32 i;
    s32 j;
    s32 frame;
    menu_quad_page_t* page;
    POLY_GT4* quad;

    g_menu_hide_numeric_values = 0;
    page = &g_battle_menu_height_display_pages[0];
    gfx_set_draw_mode_for_texture_page(&page->modes[0], MENU_TEXTURE_PAGE_ALLOCATED_IMAGES);
    gfx_set_draw_mode_for_texture_page(&page->modes[1], MENU_TEXTURE_PAGE_FRAME);
    for (i = 0; i < 5; i++) {
        quad = &page->quads[i];
        SetPolyGT4(quad);
        SetShadeTex(quad, 0);
        SetSemiTrans(quad, 1);
        quad->tpage = GetTPage(0, 0, 0x380, 0x120);
        quad->clut = 0x7CFC;
    }
    menu_init_sprite_array(page->sprites, 2, 0x7CBC);
    page->sprites[1].clut = 0x7D7C;
    for (i = 0; i < 2; i++) {
        gfx_init_image_loading(&page->sprites[i], &g_menu_texture_origin, &g_battle_tile_info_image_location,
            &g_battle_tile_info_image_params[i]);
    }
    text_configure_sprite_vram(&rect, 0x18, 0x10, &page->sprites[0], 0);
    mem_copy_bytes_nonempty(
        &g_battle_menu_height_display_pages[1], &g_battle_menu_height_display_pages[0], sizeof(menu_quad_page_t));

    for (frame = 0;; frame++) {
        page = &g_battle_menu_height_display_pages[frame & 1];
        /* The target loads the display height signed (lh). */
        i = menu_build_quad_pieces(g_map_selected_tile_data.display_height, page->quads) - 1;
        thread_yield();
        if (battle_thread_find_running_by_task(THREAD_TASK_DARK_SCREEN) != 0) {
            continue;
        }
        if (battle_thread_find_running_by_task(THREAD_TASK_DARK_SCREEN_HOLD) != 0) {
            continue;
        }
        g_menu_hide_numeric_values = 0;
        if (battle_thread_get_current_parameter_3() != 0) {
            break;
        }
        for (j = i; j >= 0; j--) {
            gfx_draw_or_append_gpu_primitive(&page->quads[j]);
        }
        if (g_map_selected_tile_data.depth != 0) {
            battle_clear_menu_render_buffer(buffer, 0x100);
            g_menu_text_state.pen.stride = 0x18;
            menu_set_text_origin(0x10, 0);
            menu_draw_number_glyphs(g_map_selected_tile_data.depth, 1, buffer, &g_menu_text_state.pen);
            if (g_menu_height_display_suppressed == 0) {
                LoadImage(&rect, buffer);
                page->sprites[0].clut = 0x7CBC;
                gfx_draw_or_append_gpu_primitive(&page->sprites[0]);
                gfx_draw_or_append_gpu_primitive(&page->modes[0]);
                gfx_draw_or_append_gpu_primitive(&page->sprites[1]);
                gfx_draw_or_append_gpu_primitive(&page->modes[1]);
            }
        }
    }
    thread_yield();
    gfx_free_texture_rect(&rect);
    thread_exit_current();
}
