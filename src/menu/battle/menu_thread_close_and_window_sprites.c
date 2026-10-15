/* BATTLE variants of the per-frame close test and window sprite builder. */

#include "fft/battle_event.h"
#include "fft/battle_gfx.h"
#include "fft/battle_menu.h"
#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/thread.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Polled by a menu thread each frame; input is the frame's 32-bit input value.
 *
 * While input is locked or a help window is open the input is cleared and 0
 * returned. If another running thread carries the stop-request task id, the input
 * is cleared too. Returns 1 only when no such thread exists and the current
 * thread's third parameter is set, which is how a menu thread is asked to close. */
s32 menu_should_close_thread_variant(u32* input) {
    s32 i;
    s32 task_state;

    if (g_menu_input_lock_mode != 0 || g_menu_help_open != 0) {
        *input = 0;
        return 0;
    }
    i = 0;
    task_state = 3;
    for (; i < 16; i++) {
        if (i != g_thread_current_id && thread_is_running_asm_variant(i) != 0
            && g_thread_contexts[i].task_id == task_state) {
            *input = 0;
            break;
        }
    }
    if (i == 16 && thread_get_current_parameter_3_variant() != 0) {
        return 1;
    }
    return 0;
}

/* Build the window's background, edge sprites and optional compound-style sprites.
 * WORLD links its own, longer body under this name. */
void menu_build_window_sprites_variant(RECT* rect, menu_entry_t* param, menu_icon_record_t* record) {
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
        gfx_init_image_loading(
            &record->base.sprites[3], &g_menu_texture_origin, &param->window, &g_menu_single_icon_image_params[style]);
    } else if (style >= 5 && style <= 15) {
        index = (style - 5) * 4;
        menu_init_sprite_array(record->icons[0], 1, 0x7cfc);
        menu_init_sprite_array(record->icons[1], 1, 0x7cfc);
        menu_init_sprite_array(record->icons[2], 1, 0x7cfc);
        gfx_init_image_loading(
            &record->base.sprites[3], &g_menu_texture_origin, &param->window, &g_menu_icon_strip_image_params[index]);
        gfx_init_image_loading(
            record->icons[0], &g_menu_texture_origin, &param->window, &g_menu_icon_strip_image_params[index + 1]);
        gfx_init_image_loading(
            record->icons[1], &g_menu_texture_origin, &param->window, &g_menu_icon_strip_image_params[index + 2]);
        gfx_init_image_loading(
            record->icons[2], &g_menu_texture_origin, &param->window, &g_menu_icon_strip_image_params[index + 3]);
    } else {
        record->icons[0] = 0;
        record->icons[1] = 0;
        record->icons[2] = 0;
    }
    gfx_set_draw_mode_from_origin(&record->base.first_draw_mode, &param->vram_origin);
    SetDrawMode(&record->base.draw_mode, 0, 0, GetTPage(0, 2, 0x3c0, 0x100), &g_gfx_texture_window);
}
