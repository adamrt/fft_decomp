#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_effect.h"
#include "fft/battle_map.h"
#include "fft/battle_status.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/*
 * Save the actor's position or restore its pre-reaction action command.
 *
 * A Mime's weapons are cleared first. Normal actions save X/Y, elevation,
 * and facing; reactions restore the 20 saved command bytes at 0x16e.
 */
void action_store_acting_unit_data(unit_stats_t* unit) {
    if (unit->job_id == JOB_ID_MIME) {
        unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON] = ITEM_ID_NONE; /* right hand */
        unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON] = ITEM_ID_NONE;  /* left hand */
    } else if (g_action_context == ACTION_CONTEXT_PRIMARY) {
        g_action_current_attacker.facing = unit->position.facing;
    }

    if (g_action_context == ACTION_CONTEXT_PRIMARY) {
        g_battle_acting_unit_x = unit->x;
        g_battle_acting_unit_y = unit->position.y;
        g_battle_acting_unit_elevation = unit->position.higher_elevation;
    } else {
        main_util_copy_action_data(&g_action_saved_command, &unit->action_command);
        g_action_context = ACTION_CONTEXT_PRIMARY;
    }
}
