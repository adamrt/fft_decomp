#include "fft/battle_gfx.h"
#include "fft/battle_menu.h"
#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/thread.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Build the window's background, edge sprites and optional compound-style sprites.
 * WORLD links its own, longer body under this name. */
void menu_build_window_sprites(RECT* rect, menu_entry_t* param, menu_icon_record_t* record) {
    s16 style;
    s32 index;

    thread_set_current_task_id(THREAD_TASK_MENU_WINDOW_BUILD);
    rect->x = param->vram_origin.x;
    rect->y = param->vram_origin.y;
    rect->w = param->inner_width >> 2;
    rect->h = param->inner_height;
    record->_unknown_74 = 0;
    record->_unknown_78 = 0;
    menu_init_sprite_array(record->base.sprites, 4, 0x7cfc);
    gfx_init_image_loading(record->base.sprites, &param->vram_origin, &param->window, &param->text_image);
    SetSemiTrans(&record->base.sprites[1], 0);
    SetSemiTrans(&record->base.sprites[2], 1);
    record->base.sprites[1].w = 0x10;
    record->base.sprites[1].h = 0x10;
    record->base.sprites[2].w = 0x10;
    record->base.sprites[2].h = 0x10;
    record->base.sprites[1].u0 = 0xa8;
    record->base.sprites[1].v0 = 0;
    record->base.sprites[2].u0 = 0xb8;
    record->base.sprites[2].v0 = 0;
    style = param->header_id;
    if (style < 5) {
        gfx_init_image_loading(&record->base.sprites[3], &g_menu_texture_origin, &param->window,
            &g_battle_menu_single_icon_image_params[style]);
    } else if (style >= 5 && style <= 15) {
        index = (style - 5) * 4;
        menu_init_sprite_array(record->icons[0], 1, 0x7cfc);
        menu_init_sprite_array(record->icons[1], 1, 0x7cfc);
        menu_init_sprite_array(record->icons[2], 1, 0x7cfc);
        gfx_init_image_loading(&record->base.sprites[3], &g_menu_texture_origin, &param->window,
            &g_battle_menu_icon_strip_image_params[index]);
        gfx_init_image_loading(record->icons[0], &g_menu_texture_origin, &param->window,
            &g_battle_menu_icon_strip_image_params[index + 1]);
        gfx_init_image_loading(record->icons[1], &g_menu_texture_origin, &param->window,
            &g_battle_menu_icon_strip_image_params[index + 2]);
        gfx_init_image_loading(record->icons[2], &g_menu_texture_origin, &param->window,
            &g_battle_menu_icon_strip_image_params[index + 3]);
    } else {
        record->icons[0] = 0;
        record->icons[1] = 0;
        record->icons[2] = 0;
    }
    gfx_set_draw_mode_from_origin(&record->base.first_draw_mode, &param->vram_origin);
    SetDrawMode(&record->base.draw_mode, 0, 0, GetTPage(0, 2, 0x3c0, 0x100), &g_gfx_texture_window);
}
