#include "fft/battle_unit.h"
#include "fft/script.h"
#include "fft/world.h"
#include "psx/types.h"

/* WarpUnit event instruction: moves the addressed unit to the tile encoded in
 * the four parameter bytes (x, y, elevation, facing) and, when the unit is on
 * the map, teleports its renderer record and resets its rotation state. */
void world_script_warp_unit(const event_warp_unit_parameters_t* instruction) {
    /* Reusing the operand cursor for placement preserves the target allocation. */
    const event_unit_placement_t* parameters = (const event_unit_placement_t*)instruction;
    map_tile_coordinates_t coordinates;
    s32 resolved_id;
    unit_stats_t* unit;
    unit_animation_state_t* state;
    s32 index;
    s32 unit_id;
    u8 facing;

    unit_id = script_read_s16(((const event_warp_unit_parameters_t*)parameters)->unit_id_le);
    parameters = &((const event_warp_unit_parameters_t*)parameters)->placement;
    unit = battle_unit_find_stats_by_entd_unit_id(unit_id, &resolved_id);
    index = 0;
    if (resolved_id == -2 || resolved_id >= 0) {
        for (; index < BATTLE_UNIT_SLOT_COUNT; index++) {
            if (battle_unit_get_stats_from_battle_id(index) == unit) {
                break;
            }
        }
        unit->x = parameters->x;
        unit->position.y = parameters->y;
        unit->position.higher_elevation = parameters->elevation;
        unit->position.facing = parameters->facing;
        index = battle_unit_get_runtime_id_by_battle_id(index);
        if (index != -1) {
            state = &g_unit_animation_states[index];
            coordinates.x = unit->x;
            coordinates.layer = unit->position.higher_elevation;
            coordinates.y = unit->position.y;
            facing = parameters->facing;
            state->delay = 0;
            state->rotating = 0;
            state->target_facing = facing * 4;
            battle_unit_place_in_new_location(index, &coordinates, parameters->facing);
            unit_set_facing(index, parameters->facing * 4);
        }
    }
}
