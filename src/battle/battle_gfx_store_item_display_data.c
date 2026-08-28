#include "fft/battle.h"
#include "fft/battle_gfx.h"

typedef struct item_graphic_data_t {
    u8 _pad0[0xc];
    u8 x_load_location;
    u8 y_load_location;
    u16 clut;
    s16 width;
    s16 height;
} item_graphic_data_t;

void battle_gfx_store_item_display_data(battle_gfx_render_unit_t* unit, u32 item_id) {
    item_graphic_data_t data;

    battle_get_item_graphic_data((SPRT*)&data, item_id);
    unit->found_item_display->clut = data.clut;
    /* s16/u8 view: the target sign-extends the offsets and sizes the u16
     * prototype would zero-extend. */
    ((void (*)(battle_gfx_sprite_display_data_t*, s32, s16, s16, u8, u8, s16, s16,
        s32))battle_gfx_store_sprite_display_data)(unit->found_item_display, 0, (16 - data.width) / 2 - 8,
        (16 - data.height) / 2 - 8, data.x_load_location, data.y_load_location, data.width, data.height, 0);
}
