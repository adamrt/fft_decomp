#include "fft/battle.h"
#include "fft/world.h"
#include "psx/types.h"

s32 world_script_warp_unit_display_to_paired_unit(s32 misc_id) {
    battle_stats_t* stats;
    world_unit_animation_state_t* state;
    s32 facing;
    s32 battle_id;

    if (misc_id == 0xF) {
        stats = battle_unit_get_stats_from_battle_id(1);
        battle_id = 3;
    } else {
        stats = battle_unit_get_stats_from_battle_id(2);
        battle_id = 4;
    }
    misc_id = battle_unit_get_misc_id_by_battle_id(battle_id);
    if (misc_id != -1) {
        g_world_script_warp_unit_coords.x = stats->x;
        g_world_script_warp_unit_coords.elevation = stats->position.raw >> 15;
        g_world_script_warp_unit_coords.y = stats->position.bits.y;
        facing = (stats->position.raw >> 6) & 0x3C;
        state = &g_world_unit_animation_states[misc_id];
        state->delay = 0;
        state->rotating = 0;
        state->target_facing = facing;
        battle_unit_place_in_new_location(misc_id, (s16*)&g_world_script_warp_unit_coords, stats->position.bits.facing);
        world_unit_set_facing(misc_id, (stats->position.raw >> 6) & 0x3C);
    }
}
