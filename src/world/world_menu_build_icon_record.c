#include "fft/menu.h"
#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Publish the icon strip's text-image upload rectangle to the caller, build the
 * four menu sprites of the icon record, and place the icon sprites that the
 * strip's icon kind selects. */
void world_menu_build_icon_record(RECT* rect, world_menu_icon_thread_param_t* param, world_menu_icon_record_t* record) {
    s32 kind;
    /* Pins: cse merges a plain copy of kind; the target tests a second copy in $v1 into $v0. */
    register s32 index __asm__("$3");
    register s32 test __asm__("$2");
    u16 width;
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

    world_thread_set_current_task_id(NATIVE_THREAD_TASK_MENU_WINDOW_BUILD);
    rect->x = param->x;
    rect->y = param->y;
    width = *(u16*)&param->width;
    rect->w = (s16)width >> 2;
    rect->h = param->height;
    record->unknown_74 = 0;
    record->unknown_78 = 0;
    world_menu_init_sprite_array(record->base.sprites, 4, 0x7cfc);
    world_gfx_init_image_loading((POLY_FT4*)record->base.sprites, (const world_image_location_t*)param,
        (const world_image_location_t*)&param->icon_x, &param->text_image);
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
    kind = param->icon_kind;
    index = kind;
    if (kind == 0x10) {
        world_menu_init_sprite_array(record->icons[0], 1, 0x7cfc);
        record->icons[0]->u0 = 0x30;
        record->icons[0]->v0 = 0x10;
        record->icons[0]->w = size;
        record->icons[0]->h = 8;
        record->icons[0]->x0
            = ((const world_gfx_image_load_parameters_t*)param->redraw_flag)->height + param->icon_x + 7;
        record->icons[0]->y0 = param->flags - 1;
        world_gfx_init_image_loading((POLY_FT4*)&record->base.sprites[3], &g_world_gfx_menu_image_source,
            (const world_image_location_t*)&param->icon_x, &g_world_menu_single_icon_image_params[1]);
    } else if (kind < 5) {
        world_gfx_init_image_loading((POLY_FT4*)&record->base.sprites[3], &g_world_gfx_menu_image_source,
            (const world_image_location_t*)&param->icon_x, &g_world_menu_single_icon_image_params[kind]);
    } else {
        test = index - 5;
        if ((u32)test < 0xb) {
            world_menu_init_sprite_array(record->icons[0], 1, 0x7cfc);
            world_menu_init_sprite_array(record->icons[1], 1, 0x7cfc);
            world_menu_init_sprite_array(record->icons[2], 1, 0x7cfc);
            /* `kind` stays unscaled and the x4 is spelled `<< 2` inline. Written
             * `kind * 4 + n`, fold collapses it to one x48 whose 3-op synth is an
             * instruction short; `(kind << 2) + n` selects the target's 4-op form. */
            kind -= 5;
            world_gfx_init_image_loading((POLY_FT4*)&record->base.sprites[3], &g_world_gfx_menu_image_source,
                (const world_image_location_t*)&param->icon_x, &g_world_menu_icon_strip_image_params[(kind << 2)]);
            world_gfx_init_image_loading((POLY_FT4*)record->icons[0], &g_world_gfx_menu_image_source,
                (const world_image_location_t*)&param->icon_x, &g_world_menu_icon_strip_image_params[(kind << 2) + 1]);
            world_gfx_init_image_loading((POLY_FT4*)record->icons[1], &g_world_gfx_menu_image_source,
                (const world_image_location_t*)&param->icon_x, &g_world_menu_icon_strip_image_params[(kind << 2) + 2]);
            world_gfx_init_image_loading((POLY_FT4*)record->icons[2], &g_world_gfx_menu_image_source,
                (const world_image_location_t*)&param->icon_x, &g_world_menu_icon_strip_image_params[(kind << 2) + 3]);
        } else {
            record->icons[0] = 0;
            record->icons[1] = 0;
            record->icons[2] = 0;
        }
    }
    world_gfx_set_draw_mode_from_rect(record, (u16*)param);
    SetDrawMode(&record->base.draw_mode, 0, 0, (u16)GetTPage(0, 2, 0x3c0, 0x100), &g_world_gfx_texture_window);
}
