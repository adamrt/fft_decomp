#include "fft/battle.h"
#include "fft/event.h"
#include "fft/unit_slots.h"
#include "fft/world.h"
#include "psx/types.h"

/* WarpUnit event instruction: moves the addressed unit to the tile encoded in
 * the four parameter bytes (x, y, elevation, facing) and, when the unit is on
 * the map, teleports its renderer record and resets its rotation state. */
s32 world_script_warp_unit(void* instruction) {
    const u8* parameters = (const u8*)instruction;
    world_unit_coordinates_t coordinates;
    s32 resolved_id;
    battle_stats_t* unit;
    world_unit_animation_state_t* state;
    s32 index;
    s32 unit_id;
    u8 facing;

    unit_id = world_script_load_halfword(parameters);
    parameters += 2;
    unit = find_unit_by_id(unit_id, &resolved_id);
    index = 0;
    if (resolved_id == -2 || resolved_id >= 0) {
        for (; index < BATTLE_UNIT_SLOT_COUNT; index++) {
            if (battle_unit_get_stats_from_battle_id(index) == unit) {
                break;
            }
        }
        unit->x = parameters[0];
        unit->position.bits.y = parameters[1];
        unit->position.bits.higher_elevation = parameters[2];
        unit->position.bits.facing = parameters[3];
        index = battle_unit_get_misc_id_by_battle_id(index);
        if (index != -1) {
            state = &g_world_unit_animation_states[index];
            coordinates.x = unit->x;
            coordinates.elevation = unit->position.raw >> 15;
            coordinates.y = unit->position.bits.y;
            facing = parameters[3];
            state->delay = 0;
            state->rotating = 0;
            state->target_facing = facing * 4;
            battle_unit_place_in_new_location(index, (s16*)&coordinates, parameters[3]);
            world_unit_set_facing(index, parameters[3] * 4);
        }
    }
}
