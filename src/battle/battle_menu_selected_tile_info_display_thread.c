#include "fft/battle.h"
#include "fft/battle_gfx.h"
#include "fft/battle_text.h"
#include "fft/main_heap.h"
#include "fft/menu.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

typedef char battle_menu_quad_page_size_must_be_0x144[sizeof(world_menu_quad_page_t) == 0x144 ? 1 : -1];

/* Double-buffered 0x144-byte pages at 0x80173cbc: five quads, two sprites,
 * and two draw-mode packets for their texture windows. */
extern world_menu_quad_page_t g_battle_menu_height_display_pages[2];

/* Display thread for the selected tile: draws its quad group
 * (the selected tile's display height, built by
 * battle_menu_build_quad_pieces) every frame and, when its depth is
 * non-zero, renders it as a one-digit number into the icon sprite. Runs
 * until the third thread parameter is set. BATTLE twin of
 * world_map_draw_selected_tile_info_thread. */
void battle_menu_selected_tile_info_display_thread(void) {
    RECT rect;
    u8 buffer[0x100];
    s32 i;
    s32 j;
    s32 frame;
    world_menu_quad_page_t* page;
    POLY_GT4* quad;

    g_battle_menu_hide_numeric_values = 0;
    page = &g_battle_menu_height_display_pages[0];
    battle_gfx_set_draw_mode_for_texture_page(&page->modes[0], 1);
    battle_gfx_set_draw_mode_for_texture_page(&page->modes[1], 0);
    for (i = 0; i < 5; i++) {
        quad = &page->quads[i];
        SetPolyGT4(quad);
        SetShadeTex(quad, 0);
        SetSemiTrans(quad, 1);
        quad->tpage = GetTPage(0, 0, 0x380, 0x120);
        quad->clut = 0x7CFC;
    }
    battle_menu_init_sprite_array(page->sprites, 2, 0x7CBC);
    page->sprites[1].clut = 0x7D7C;
    for (i = 0; i < 2; i++) {
        battle_gfx_init_image_loading((POLY_FT4*)&page->sprites[i], &g_battle_menu_texture_location,
            (const battle_image_location_t*)g_battle_tile_info_image_location, &g_battle_tile_info_image_params[i]);
    }
    battle_text_configure_sprite_vram(&rect, 0x18, 0x10, &page->sprites[0], 0);
    battle_copy_bytes(
        &g_battle_menu_height_display_pages[1], &g_battle_menu_height_display_pages[0], sizeof(world_menu_quad_page_t));

    for (frame = 0;; frame++) {
        s32* stride;

        page = &g_battle_menu_height_display_pages[frame & 1];
        /* The target loads the display height signed (lh). */
        i = battle_menu_build_quad_pieces((s16)g_battle_map_selected_tile_data.display_height, page->quads) - 1;
        battle_thread_yield();
        if (battle_thread_find_running_by_task(NATIVE_THREAD_TASK_DARK_SCREEN) != 0) {
            continue;
        }
        if (battle_thread_find_running_by_task(NATIVE_THREAD_TASK_DARK_SCREEN_HOLD) != 0) {
            continue;
        }
        g_battle_menu_hide_numeric_values = 0;
        if (battle_thread_get_current_parameter_3() != 0) {
            break;
        }
        for (j = i; j >= 0; j--) {
            battle_gfx_draw_or_append_gpu_primitive(&page->quads[j]);
        }
        if (g_battle_map_selected_tile_data.depth != 0) {
            battle_clear_menu_render_buffer(buffer, 0x100);
            stride = &g_menu_text_state.stride;
            *stride = 0x18;
            battle_menu_set_text_origin(0x10, 0);
            battle_draw_menu_number_glyphs(
                g_battle_map_selected_tile_data.depth, 1, buffer, (battle_rect_t*)((s16*)stride - 4));
            if (*(u16*)&g_battle_menu_height_display_suppressed == 0) {
                LoadImage(&rect, buffer);
                page->sprites[0].clut = 0x7CBC;
                battle_gfx_draw_or_append_gpu_primitive(&page->sprites[0]);
                battle_gfx_draw_or_append_gpu_primitive(&page->modes[0]);
                battle_gfx_draw_or_append_gpu_primitive(&page->sprites[1]);
                battle_gfx_draw_or_append_gpu_primitive(&page->modes[1]);
            }
        }
    }
    battle_thread_yield();
    battle_gfx_free_tpage7_vram(&rect);
    battle_thread_exit_current();
}
