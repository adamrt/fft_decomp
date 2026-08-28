#include "fft/battle.h"
#include "fft/event.h"
#include "fft/world.h"
#include "psx/types.h"

extern world_unit_animation_state_t g_battle_unit_misc_rotation_data[];

/*
 * WarpUnit event instruction: move an ENTD unit to a new tile and facing.
 *
 * Parameters: 0x00 ENTD unit id (halfword), 0x02 x, 0x03 y, 0x04 higher
 * elevation, 0x05 facing.
 */
void battle_script_warp_unit(const u8* parameters) {
    battle_stats_t* unit;
    s32 battle_id;
    s32 misc_id;
    s32 result;
    s16 unit_id;
    s16 tile[3];
    world_unit_animation_state_t* rotation_state;

    unit_id = battle_script_load_halfword(parameters);
    /* Remaining operands: x, y, higher elevation, facing. */
    parameters += 2;
    unit = battle_find_unit_data_pointer_for_entd_unit_id(unit_id, &result);
    battle_id = 0;
    if (result != -2 && result < 0) {
        return;
    }
    for (; battle_id < EVENT_UNIT_SLOT_COUNT; battle_id++) {
        if (battle_unit_get_stats_from_battle_id(battle_id) == unit) {
            break;
        }
    }
    unit->x = parameters[0];
    unit->position.bits.y = parameters[1];
    unit->position.bits.higher_elevation = parameters[2];
    unit->position.bits.facing = parameters[3];
    misc_id = battle_unit_get_misc_id_by_battle_id(battle_id);
    if (misc_id == -1) {
        return;
    }
    tile[0] = unit->x;
    tile[1] = unit->position.bits.higher_elevation;
    tile[2] = unit->position.bits.y;
    rotation_state = &g_battle_unit_misc_rotation_data[misc_id];
    rotation_state->target_facing = parameters[3] * 4;
    rotation_state->delay = 0;
    rotation_state->rotating = 0;
    battle_unit_place_in_new_location(misc_id, tile, parameters[3]);
    battle_unit_init_for_store_anim_facing_move(misc_id, parameters[3] * 4);
}
