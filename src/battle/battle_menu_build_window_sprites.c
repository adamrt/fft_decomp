#include "fft/battle.h"
#include "fft/battle_menu_window.h"
#include "fft/thread.h"

/* Build the window's background, edge sprites and optional compound-style sprites. */
void battle_menu_build_window_sprites(
    battle_menu_window_header_t* header, battle_menu_window_spec_t* spec, battle_menu_window_record_t* record) {
    s32 style;
    /* Pin: the 5..15 range test reads a separate $v1 copy of style. */
    register s32 range_style __asm__("$3");
    u32 range_index;
    s32 stride;
    /* Pins: the argument registers are loaded before the barrier below. */
    register POLY_FT4* primitive __asm__("$4");
    const battle_image_location_t* load;
    register const battle_image_location_t* call_load __asm__("$5");
    register const battle_image_location_t* screen __asm__("$6");
    volatile u8 unused_stack[8];

    battle_thread_set_current_task_id(NATIVE_THREAD_TASK_MENU_WINDOW_BUILD);
    header->texture_x = spec->load.x;
    header->texture_y = spec->load.y;
    header->half_width = (s16)(spec->width) >> 2;
    header->height = spec->height;
    record->unknown_74 = 0;
    record->unknown_78 = 0;
    battle_menu_init_sprite_array(record->sprites, 4, 0x7cfc);
    battle_gfx_init_image_loading((POLY_FT4*)record->sprites, &spec->load, &spec->screen, &spec->params);
    SetSemiTrans(&record->sprites[1], 0);
    SetSemiTrans(&record->sprites[2], 1);
    record->sprites[1].w = 0x10;
    record->sprites[1].h = 0x10;
    record->sprites[2].w = 0x10;
    record->sprites[2].h = 0x10;
    record->sprites[1].u0 = 0xa8;
    record->sprites[1].v0 = 0;
    record->sprites[2].u0 = 0xb8;
    record->sprites[2].v0 = 0;
    style = spec->style;
    range_style = style;
    if (style < 5) {
        battle_gfx_init_image_loading((POLY_FT4*)&record->sprites[3], &g_battle_menu_texture_location, &spec->screen,
            &g_battle_menu_single_icon_image_params[style]);
    } else if ((range_index = range_style - 5) < 0xb) {
        battle_menu_init_sprite_array(record->extra[0], 1, 0x7cfc);
        battle_menu_init_sprite_array(record->extra[1], 1, 0x7cfc);
        battle_menu_init_sprite_array(record->extra[2], 1, 0x7cfc);
        style -= 5;
        primitive = (POLY_FT4*)&record->sprites[3];
        load = &g_battle_menu_texture_location;
        call_load = load;
        screen = &spec->screen;
        /* Keeps the argument setup ahead of the displacement calculation. */
        __asm__("" : "=r"(style) : "0"(style), "r"(primitive), "r"(call_load), "r"(screen));
        range_index = style << 2;
        stride = (style << 3) + range_index;
        /* A typed 48-byte row index emits 3 scaling instructions instead of
         * the target's 4. The explicit word displacement preserves the match. */
        battle_gfx_init_image_loading(primitive, call_load, screen,
            (world_gfx_image_load_parameters_t*)((u32*)&g_battle_menu_icon_strip_image_params[0] + stride));
        battle_gfx_init_image_loading((POLY_FT4*)record->extra[0], load, &spec->screen,
            (world_gfx_image_load_parameters_t*)((u32*)&g_battle_menu_icon_strip_image_params[1] + stride));
        battle_gfx_init_image_loading((POLY_FT4*)record->extra[1], load, &spec->screen,
            (world_gfx_image_load_parameters_t*)((u32*)&g_battle_menu_icon_strip_image_params[2] + stride));
        battle_gfx_init_image_loading((POLY_FT4*)record->extra[2], load, &spec->screen,
            (world_gfx_image_load_parameters_t*)((u32*)&g_battle_menu_icon_strip_image_params[3] + stride));
    } else {
        record->extra[0] = (SPRT*)0;
        record->extra[1] = (SPRT*)0;
        record->extra[2] = (SPRT*)0;
    }
    battle_gfx_set_draw_mode_from_rect(record, (const u16*)spec);
    SetDrawMode(&record->mode1, 0, 0, GetTPage(0, 2, 0x3c0, 0x100) & 0xffff, &g_battle_menu_disabled_texture_window);
}
