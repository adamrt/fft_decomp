#include "fft/battle.h"

void battle_gfx_set_thrown_item_graphic_palette(s32 graphic_id, battle_unit_misc_data_t* unit) {
    u8* g_main_item_item_flags;
    u32 palette_id;
    s32 row;
    s32 idx;
    battle_unit_misc_data_t* unit2;
    /* The last test re-reads the ability id rather than reusing the value the
     * range test loaded; assigning it through its own local stops cse folding
     * the two reads into one. */
    u16 ability_id;

    unit2 = unit;
    g_main_item_item_flags = get_item_data_pointer();
    palette_id = g_main_item_item_flags[5];
    if ((palette_id >= 0x20) || ((u32)(unit2->used_ability_id - 0x17E) < 0xC)
        || ((ability_id = unit->used_ability_id), ability_id == ABILITY_ID_BASIC_SKILL_THROW_STONE)) {
        palette_id = 0x15;
    }
    idx = graphic_id * 2;
    row = unit->unit_id;
    /* The header types this as u8[0x200]; the palette rows are 16 colours of
     * two bytes, so the index has to stay in halfwords or the stride halves. */
    battle_map_load_palette_data(
        &((u16*)g_battle_gfx_item_palettes)[(g_battle_gfx_item_graphic_data[idx] >> 4) * 16], 5, row, 0);
    battle_map_load_palette_data(
        &((u16*)g_battle_gfx_item_palettes)[(g_battle_gfx_item_graphic_data[idx] & 0xF) * 16], 6, row, 0);
    unit->used_item_or_weapon_id = graphic_id;
    unit->equipped_weapon_type = palette_id;
}
