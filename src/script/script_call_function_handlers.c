/* Two CallFunction bodies: Brave +10 and paired-unit warp. */

#include "fft/battle_script.h"
#include "fft/battle_unit.h"
#include "fft/unit.h"
#include "psx/types.h"

/* Visit every blue-team slot, including empty ones; the byte addition precedes the cap. */
void unit_increase_blue_team_brave_by_10(void) {
    unit_stats_t* unit;
    s32 unit_id;

    for (unit_id = 0; unit_id < BATTLE_UNIT_SLOT_COUNT; unit_id++) {
        unit = battle_unit_get_stats_from_battle_id(unit_id);
        if ((unit->team_flags & BATTLE_TEAM_MASK) == BATTLE_TEAM_BLUE) {
            unit->brave += 10;
            if (unit->brave >= UNIT_BRAVE_FAITH_MAX) {
                unit->brave = UNIT_BRAVE_FAITH_MAX;
            }
        }
    }
}

/* CallFunction 0x0f and 0x10: place the runtime unit of battle id 3 on the tile and
 * facing of battle id 1, or that of battle id 4 on battle id 2, and cancel any
 * rotation in progress. Nothing happens when the warped battle id has no runtime
 * unit.
 *
 * The parameter arrives as the CallFunction selector and is reused for the runtime
 * id. */
void script_warp_unit_display_to_paired_unit(s32 runtime_id) {
    unit_stats_t* unit;
    unit_animation_state_t* state;
    s32 facing;
    s32 battle_id;

    if (runtime_id == EVENT_CALL_FUNCTION_WARP_UNIT_4_DISPLAY_TO_UNIT_2_POSITION_AND_FACING) {
        unit = battle_unit_get_stats_from_battle_id(1);
        battle_id = 3;
    } else {
        unit = battle_unit_get_stats_from_battle_id(2);
        battle_id = 4;
    }
    runtime_id = battle_unit_get_runtime_id_by_battle_id(battle_id);
    if (runtime_id != -1) {
        g_script_warp_coords.x = unit->x;
        g_script_warp_coords.layer = unit->position.higher_elevation;
        g_script_warp_coords.y = unit->position.y;
        facing = unit->position.facing << 2;
        state = &g_unit_animation_states[runtime_id];
        state->delay = 0;
        state->rotating = 0;
        state->target_facing = facing;
        battle_unit_place_in_new_location(runtime_id, &g_script_warp_coords, unit->position.facing);
        unit_set_facing(runtime_id, unit->position.facing << 2);
    }
}
