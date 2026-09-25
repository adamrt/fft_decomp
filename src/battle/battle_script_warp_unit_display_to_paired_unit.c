#include "fft/battle.h"
#include "fft/battle_unit_rotation.h"
#include "fft/world.h"
#include "psx/types.h"

/* Provisional: three-halfword map coordinate staged for
 * battle_unit_place_in_new_location. The WORLD twin stages the same triple in
 * g_world_script_warp_unit_coords. */
extern world_unit_coordinates_t g_battle_script_warp_coords;

void battle_script_warp_unit_display_to_paired_unit(s32 misc_id) {
    battle_stats_t* unit;
    battle_unit_rotation_state_t* state;
    s32 facing;
    s32 battle_id;

    if (misc_id == 0xF) {
        unit = battle_unit_get_stats_from_battle_id(1);
        battle_id = 3;
    } else {
        unit = battle_unit_get_stats_from_battle_id(2);
        battle_id = 4;
    }
    misc_id = battle_unit_get_misc_id_by_battle_id(battle_id);
    if (misc_id != -1) {
        g_battle_script_warp_coords.x = unit->x;
        g_battle_script_warp_coords.elevation = unit->position.bits.higher_elevation;
        g_battle_script_warp_coords.y = unit->position.bits.y;
        facing = (unit->position.raw >> 6) & 0x3C;
        state = &g_battle_unit_misc_rotation_data[misc_id];
        state->delay = 0;
        state->rotating = 0;
        state->target_facing = facing;
        battle_unit_place_in_new_location(misc_id, (s16*)&g_battle_script_warp_coords, unit->position.bits.facing);
        battle_unit_init_for_store_anim_facing_move(misc_id, (unit->position.raw >> 6) & 0x3C);
    }
}
