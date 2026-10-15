/* Unit and weapon SHP/SEQ tables by spritesheet id. */

#include "fft/battle_gfx.h"
#include "psx/types.h"

/* The SHP frame tables of a sheet: one of the five resident ones, or those of
 * the battle's ARUTE or KANZEN file. */
battle_gfx_unit_shp_frame_tables_t* battle_gfx_get_unit_shp_by_spritesheet_id(u32 spritesheet_id) {
    s32 value;

    value = g_battle_gfx_spritesheet_data[spritesheet_id & 0xffff].shp_id;
    if (value < 5) {
        return &g_battle_gfx_unit_shp_frame_tables[value];
    }
    return &g_battle_gfx_boss_shp_frame_tables;
}

/* The SEQ table of a sheet: one of the five resident ones, or the table of
 * the battle's RUKA, ARUTE or KANZEN file. */
battle_gfx_unit_seq_table_t* battle_gfx_get_unit_seq_by_spritesheet_id(u32 spritesheet_id) {
    s32 value;

    value = g_battle_gfx_spritesheet_data[spritesheet_id & 0xffff].seq_id;
    if (value < BATTLE_SEQ_RUKA) {
        return &g_battle_gfx_unit_seq_tables[value].header;
    }
    return &g_battle_gfx_boss_seq_tables.header;
}

/* The weapon SHP of a sheet. The weapon tables are 0xc40-byte records, a
 * battle_gfx_weapon_shp_t with 0x200 frames and then its 0x100 sequence
 * pointers: WEP1 for SHP type TYPE1 and WEP2 for TYPE2. The other SHP types
 * index past the two records; none of their sequences starts a weapon block. */
battle_gfx_weapon_shp_t* battle_gfx_get_weapon_shp_by_spritesheet_id(u32 spritesheet_id) {
    return &g_battle_gfx_weapon_graphics[g_battle_gfx_spritesheet_data[spritesheet_id & 0xffff].shp_id].shp;
}

/* The weapon SEQ table of a sheet: the second part of the record that
 * battle_gfx_get_weapon_shp_by_spritesheet_id describes. */
u8** battle_gfx_get_weapon_seq_by_spritesheet_id(u32 spritesheet_id) {
    return g_battle_gfx_weapon_graphics[g_battle_gfx_spritesheet_data[spritesheet_id & 0xffff].shp_id].seq;
}
