#include "fft/battle.h"

/* Transform a unit into a Morbol, keeping its name, position and facing. */
void battle_unit_modify_entd_unit(battle_stats_t* unit) {
    u8 saved_name[0x10];
    u8 palette[8];
    u8* name;
    u8 unit_id;
    u8 formation_index;
    u8 x;
    /* Pin: position_low takes $s4 and x $s3; unpinned, the two swap. */
    register u8 position_low __asm__("$20");
    u32 position;
    s32 higher_elevation;
    s32 facing;

    /* Preserve the unit's encoded name across the reset. */
    name = unit->name;
    unit_id = unit->unit_id;
    unit->job_id = JOB_ID_MORBOL;
    unit->character_identity = CHARACTER_IDENTITY_MONSTER;
    main_util_copy_byte_data(name, saved_name, 0x10);
    formation_index = unit->formation_index;
    position = unit->position.raw;
    x = unit->x;
    position_low = unit->position.bytes[0];
    unit->formation_index = 0xFE;
    higher_elevation = position >> 15;
    facing = (position >> 8) & 0xF;
    main_unit_init_for_battle(unit, 0, 0, 0x82);
    unit->position.bytes[0] = position_low;
    unit->x = x;
    unit->formation_index = formation_index;
    unit->position.raw = (((unit->position.raw & 0x7FFF) | (higher_elevation << 15)) & 0xF0FF) | (facing << 8);
    main_util_copy_byte_data(saved_name, name, 0x10);
    unit->unit_id = unit_id;
    main_unit_calculate_palette_spritesheet(unit, palette);
}
