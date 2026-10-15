#include "fft/battle_unit.h"
#include "fft/memory.h"

/* Transform a unit into a Morbol, keeping its name, position and facing. */
void battle_unit_transform_into_morbol(unit_stats_t* unit) {
    u8 saved_name[0x10];
    u8 palette[8];
    u8* name;
    u8 unit_id;
    u8 formation_index;
    u8 x;
    u8 y;
    u8 higher_elevation;
    u8 facing;

    /* Preserve the unit's encoded name across the reset. */
    name = unit->name;
    unit_id = unit->unit_id;
    unit->job_id = JOB_ID_MORBOL;
    unit->character_identity = CHARACTER_IDENTITY_MONSTER;
    mem_copy_byte_data_source_first(name, saved_name, 0x10);
    formation_index = unit->formation_index;
    x = unit->x;
    y = unit->position.y;
    higher_elevation = unit->position.higher_elevation;
    facing = unit->position.facing;
    unit->formation_index = BATTLE_FORMATION_INDEX_GENERATED_UNIT;
    main_unit_init_for_battle(unit, 0, 0, 0x82);
    unit->x = x;
    unit->position.y = y;
    unit->formation_index = formation_index;
    unit->position.higher_elevation = higher_elevation;
    unit->position.facing = facing;
    mem_copy_byte_data_source_first(saved_name, name, 0x10);
    unit->unit_id = unit_id;
    main_unit_calculate_palette_spritesheet(unit, palette);
}
