#include "fft/battle.h"
#include "fft/battle_map.h"
#include "fft/battle_menu.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/* Build the targeting panels for a weapon attack: the weapon's range around the
 * acting unit, without the unit's own tile. Returns the number of tiles in range. */
s32 target_set_weapon_attack_panels(const action_command_t* source) {
    action_command_t action;
    unit_stats_t* unit;
    s32 idx;

    main_util_copy_action_data(source, &action);
    unit = &g_battle_unit_stats[action.actor_id];
    idx = unit->position.y * g_map_max_x + unit->x;
    target_clear_panel_data();
    target_calculate_weapon_range(unit);
    {
        target_panel_t* p1;
        target_panel_t* p2;
        p1 = &g_target_panel_data[idx];
        p2 = &g_target_panel_data[idx + 0x100];
        p1->remaining_range = 0;
        p2->remaining_range = 0;
        return target_set_all_panels_targeted_if_targetable();
    }
}
