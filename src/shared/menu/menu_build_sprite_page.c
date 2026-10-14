#include "fft/battle_gfx.h"
#include "fft/battle_menu.h"
#include "fft/gfx.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/*
 * Build one of the two double-buffered menu sprite pages: five draw-mode
 * packets (texture window per sprite) followed by five sprites placed from
 * the parameter table, then submit them all in reverse order.
 *
 * BATTLE twin of menu_build_sprite_page.
 */
void menu_build_sprite_page(s32 page_index, RECT* base_screen) {
    s32 i;
    menu_sprite_page_t* page = &g_menu_scratch_buffer.sprite_pages[page_index & 1];
    SPRT* sprite;

    for (i = 0; i < 5; i++) {
        sprite = &page->sprites[i];
        battle_menu_init_semitransparent_sprt(sprite);
        SetDrawMode(&page->modes[i], 1, 0, GetTPage(0, 2, 0x3c0, 0x100), &g_menu_sprite_page_texture_windows[i]);
        gfx_init_image_loading(sprite, &g_menu_texture_origin, base_screen, &g_menu_sprite_page_image_params[i]);
    }
    for (i = 4; i >= 0; i--) {
        gfx_draw_or_append_gpu_primitive(&page->sprites[i]);
        gfx_draw_or_append_gpu_primitive(&page->modes[i]);
    }
}
