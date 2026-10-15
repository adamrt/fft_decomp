#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_effect.h"
#include "fft/battle_map.h"
#include "fft/battle_status.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/* Narrow an ability's panels to the straight lines that leave the acting unit
 * toward its target: mode 1 for a linear attack, mode 3 for a three-direction
 * attack.
 *
 * On each axis where the target tile differs from the unit's, the line in that
 * direction is kept; mode 3 also keeps the two perpendicular lines. Every panel
 * off the kept lines loses its range, and the marks that identified the lines are
 * cleared. The call also turns on nearest-first ordering of the target list
 * (g_battle_sort_targets_nearest_first). */
void target_build_directional_attack_panels(const action_command_t* source, u8 mode) {
    action_command_t action;
    unit_stats_t* unit;
    target_panel_t* panel;
    s32 target_x;
    s32 target_y;
    s32 x;
    s32 y;
    s32 i;

    main_util_copy_action_data(source, &action);
    g_battle_sort_targets_nearest_first = 1;
    unit = &g_battle_unit_stats[action.actor_id];
    x = unit->x;
    y = unit->position.y;
    target_x = action.target_position.vx;
    target_y = action.target_position.vz;
    if (x < target_x) {
        target_calculate_linear_attack_tiles(TARGET_LINE_EAST, x, y);
        if (mode == 3) {
            target_calculate_linear_attack_tiles(TARGET_LINE_NORTH, x, y);
            target_calculate_linear_attack_tiles(TARGET_LINE_SOUTH, x, y);
        }
    }
    if (target_x < x) {
        target_calculate_linear_attack_tiles(TARGET_LINE_WEST, x, y);
        if (mode == 3) {
            target_calculate_linear_attack_tiles(TARGET_LINE_NORTH, x, y);
            target_calculate_linear_attack_tiles(TARGET_LINE_SOUTH, x, y);
        }
    }
    if (y < target_y) {
        target_calculate_linear_attack_tiles(TARGET_LINE_NORTH, x, y);
        if (mode == 3) {
            target_calculate_linear_attack_tiles(TARGET_LINE_EAST, x, y);
            target_calculate_linear_attack_tiles(TARGET_LINE_WEST, x, y);
        }
    }
    if (target_y < y) {
        target_calculate_linear_attack_tiles(TARGET_LINE_SOUTH, x, y);
        if (mode == 3) {
            target_calculate_linear_attack_tiles(TARGET_LINE_WEST, x, y);
            target_calculate_linear_attack_tiles(TARGET_LINE_EAST, x, y);
        }
    }
    for (i = 0; i < 0x200; i++) {
        panel = &g_target_panels[i];
        if (panel->mark == 0) {
            panel->remaining_range = 0;
        } else {
            panel->mark = 0;
        }
    }
}
