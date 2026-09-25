#include "fft/battle.h"

/*
 * Make only the candidate and acting tiles reachable in the selected scenario.
 *
 * Clear both 18-row levels before setting the candidate bit and adding the
 * acting bit. Integer address stages preserve the target's addition order;
 * direct typed indexing introduces extra instructions. Field offsets come
 * from the shared workspace declaration.
 */
void battle_ai_set_candidate_and_actor_reachable_tiles(void) {
    battle_ai_data_t* ai = &g_battle_ai_data_base;
    s32 row;

    for (row = 0; row < 18; row++) {
        u32 row_address = row * sizeof(u16);
        u32 scenario_address = (u32)ai + ai->movement_scenario * sizeof(ai->reachable_tiles[0]);
        *(u16*)(row_address + scenario_address + ((u32)&ai->reachable_tiles[0][0][0] - (u32)ai)) = 0;
        scenario_address = (u32)ai + ai->movement_scenario * sizeof(ai->reachable_tiles[0]);
        row_address += scenario_address;
        *(u16*)(row_address + ((u32)&ai->reachable_tiles[0][1][0] - (u32)ai)) = 0;
    }
    {
        u32 scenario_address = ai->movement_scenario * sizeof(ai->reachable_tiles[0]) + (u32)ai;
        u32 coords_address = (u32)ai + ai->movement_scenario * sizeof(ai->candidate_coords[0]);
        u32 level_address;
        u32 row_address;
        scenario_address += (u32)&ai->reachable_tiles - (u32)ai;
        level_address = *(u8*)(coords_address + ((u32)&ai->candidate_coords[0].bytes.elevation - (u32)ai)) * 36
            + scenario_address;
        row_address = *(u8*)(coords_address + ((u32)&ai->candidate_coords[0].bytes.y - (u32)ai)) * 2 + level_address;
        *(u16*)row_address = 0x8000 >> *(u8*)(coords_address + ((u32)&ai->candidate_coords[0].bytes.x - (u32)ai));
    }
    {
        u32 scenario_address = ai->movement_scenario * sizeof(ai->reachable_tiles[0]) + (u32)ai;
        u32 level_address;
        u32 row_address;
        scenario_address += (u32)&ai->reachable_tiles - (u32)ai;
        level_address = ai->acting_unit_coords.bytes.elevation * 36 + scenario_address;
        row_address = ai->acting_unit_coords.bytes.y * 2 + level_address;
        *(u16*)row_address |= 0x8000 >> ai->acting_unit_coords.bytes.x;
    }
}
