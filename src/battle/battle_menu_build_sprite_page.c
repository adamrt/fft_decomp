#include "fft/battle.h"
#include "fft/battle_gfx.h"
#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

/*
 * Build one of the two double-buffered menu sprite pages: five draw-mode
 * packets (texture window per sprite) followed by five sprites placed from
 * the parameter table, then submit them all in reverse order.
 *
 * BATTLE twin of world_menu_build_sprite_page.
 */
void battle_menu_build_sprite_page(s32 page_index, void* base_screen) {
    s32 i;
    menu_sprite_page_t* page = &g_battle_menu_scratch_buffer.sprite_pages[page_index & 1];
    SPRT* sprite;

    for (i = 0; i < 5; i++) {
        sprite = &page->sprites[i];
        battle_menu_init_semitransparent_sprt(sprite);
        SetDrawMode(
            &page->modes[i], 1, 0, (u16)GetTPage(0, 2, 0x3c0, 0x100), &g_battle_menu_sprite_page_texture_windows[i]);
        battle_gfx_init_image_loading((POLY_FT4*)sprite, &g_battle_menu_texture_location, base_screen,
            &g_battle_menu_sprite_page_image_params[i]);
    }
    for (i = 4; i >= 0; i--) {
        battle_gfx_draw_or_append_gpu_primitive(&page->sprites[i]);
        battle_gfx_draw_or_append_gpu_primitive(&page->modes[i]);
    }
}
