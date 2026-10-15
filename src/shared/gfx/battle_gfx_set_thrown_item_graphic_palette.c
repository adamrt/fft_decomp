#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/main.h"

/* Make item `item_id` the one a unit's weapon and effect blocks draw.
 *
 * The item's two palettes of g_battle_gfx_item_graphic_data go to the unit's
 * CLUTs of the ITEM_A and ITEM_B rows, and its type selects the frame set of
 * the weapon SHP; a thrown weapon, any Throw ability and Throw Stone use
 * set 0x15. */
void battle_gfx_set_thrown_item_graphic_palette(s32 item_id, battle_unit_t* unit) {
    item_data_t* item;
    u32 weapon_type;
    s32 clut_index;
    /* The last test re-reads the ability id rather than reusing the value the
     * range test loaded; assigning it through its own local stops cse folding
     * the two reads into one. */
    u16 ability_id;

    item = main_item_get_data_pointer(item_id);
    weapon_type = item->type;
    if ((weapon_type >= ITEM_TYPE_SHURIKEN)
        || ((u32)(unit->used_ability_id - ABILITY_ID_THROW_FIRST) < ABILITY_ID_JUMP_FIRST - ABILITY_ID_THROW_FIRST)
        || ((ability_id = unit->used_ability_id), ability_id == ABILITY_ID_BASIC_SKILL_THROW_STONE)) {
        weapon_type = 0x15;
    }
    clut_index = unit->runtime_id;
    map_load_palette_data(&g_battle_gfx_weapon_clut[g_battle_gfx_item_graphic_data[item_id].palettes >> 4],
        MAP_CLUT_ROW_ITEM_A, clut_index, 0);
    map_load_palette_data(&g_battle_gfx_weapon_clut[g_battle_gfx_item_graphic_data[item_id].palettes & 0xF],
        MAP_CLUT_ROW_ITEM_B, clut_index, 0);
    unit->used_item_or_weapon_id = item_id;
    unit->equipped_weapon_type = weapon_type;
}
