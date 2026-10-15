#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Publish the icon strip's text-image upload rectangle to the caller, build the
 * four menu sprites of the icon record, and place the icon sprites that the
 * strip's icon kind selects. BATTLE links its own body under this name;
 * only WORLD has the icon kind 0x10 branch. */
void menu_build_window_sprites_variant(RECT* rect, menu_entry_t* param, menu_icon_record_t* record) {
    s32 kind;
    s32 index;
    /* HImode is load-bearing. `cse` keys its constant equivalence classes by
     * mode, so a u16 `size` is not value-equal to the SImode 0x10 of the
     * `kind == 0x10` test below; that restores the target's separate
     * `li v0,0x10` for the compare and keeps `size` live across the arm's
     * call, which is what parks it in callee-saved s1. `s32` here changes the allocation. */
    u16 size;
    /* The target's frame is 0x40 with its register saves at 0x20 and a 20-byte
     * outgoing-argument area, so `vars` is 8 bytes the code never touches.
     * Those bytes sit above the argument area and below the saves, which is
     * where a declared aggregate lands; an unreferenced one reproduces the
     * frame at zero instructions. Worth the whole prologue and epilogue. */
    u8 unused_scratch[8];

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
    size = 0x10;
    record->base.sprites[1].w = size;
    record->base.sprites[1].h = size;
    record->base.sprites[2].w = size;
    record->base.sprites[2].h = size;
    record->base.sprites[1].u0 = 0xa8;
    record->base.sprites[1].v0 = 0;
    record->base.sprites[2].u0 = 0xb8;
    record->base.sprites[2].v0 = 0;
    kind = param->header_id;
    /* The range test below reads its own 16-bit copy of the kind. */
    index = (s16)kind;
    if (kind == 0x10) {
        const image_load_parameters_t* image_params;

        menu_init_sprite_array(record->icons[0], 1, 0x7cfc);
        record->icons[0]->u0 = 0x30;
        record->icons[0]->v0 = 0x10;
        record->icons[0]->w = size;
        record->icons[0]->h = 8;
        image_params = param->context;
        record->icons[0]->x0 = image_params->height + param->window.x + 7;
        record->icons[0]->y0 = param->window.y - 1;
        gfx_init_image_loading(
            &record->base.sprites[3], &g_menu_texture_origin, &param->window, &g_menu_single_icon_image_params[1]);
    } else if (kind < 5) {
        gfx_init_image_loading(
            &record->base.sprites[3], &g_menu_texture_origin, &param->window, &g_menu_single_icon_image_params[kind]);
    } else {
        if (index >= 5 && index < 0x10) {
            menu_init_sprite_array(record->icons[0], 1, 0x7cfc);
            menu_init_sprite_array(record->icons[1], 1, 0x7cfc);
            menu_init_sprite_array(record->icons[2], 1, 0x7cfc);
            /* `kind` stays unscaled and the x4 is spelled `<< 2` inline. Written
             * `kind * 4 + n`, fold collapses it to one x48 whose 3-op synth is an
             * instruction short; `(kind << 2) + n` selects the target's 4-op form. */
            kind -= 5;
            gfx_init_image_loading(&record->base.sprites[3], &g_menu_texture_origin, &param->window,
                &g_menu_icon_strip_image_params[(kind << 2)]);
            gfx_init_image_loading(record->icons[0], &g_menu_texture_origin, &param->window,
                &g_menu_icon_strip_image_params[(kind << 2) + 1]);
            gfx_init_image_loading(record->icons[1], &g_menu_texture_origin, &param->window,
                &g_menu_icon_strip_image_params[(kind << 2) + 2]);
            gfx_init_image_loading(record->icons[2], &g_menu_texture_origin, &param->window,
                &g_menu_icon_strip_image_params[(kind << 2) + 3]);
        } else {
            record->icons[0] = 0;
            record->icons[1] = 0;
            record->icons[2] = 0;
        }
    }
    gfx_set_draw_mode_from_origin(&record->base.first_draw_mode, &param->vram_origin);
    SetDrawMode(&record->base.draw_mode, 0, 0, GetTPage(0, 2, 0x3c0, 0x100), &g_gfx_texture_window);
}
