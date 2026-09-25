#include "fft/battle.h"

enum {
    BATTLE_OUTCOME_SPECIAL_CLASS_MASK = 0x03,
    BATTLE_OUTCOME_SPECIAL_CLASS = 0x03,
};

/* Determine whether either side has met its battle-ending condition.
 *
 * Returns -1 when the blue side is defeated, 0 when all opposing units are
 * defeated, and 1 while both sides can continue. */
s32 battle_action_check_battle_outcome(void) {
    s32 player_unit_count;
    s32 enemy_unit_count;
    s32 unit_index;
    s32 selected;
    battle_stats_t* unit;

    player_unit_count = 0;
    enemy_unit_count = 0;
    unit_index = 0;
    do {
        unit = &g_battle_unit_stats[unit_index];
        if ((unit->initial_team_flags & BATTLE_OUTCOME_SPECIAL_CLASS_MASK) == BATTLE_OUTCOME_SPECIAL_CLASS) {
            if (unit->existence != 0xff) {
                selected = unit->existence == BATTLE_UNIT_EXISTENCE_DISABLED;
                selected += main_unit_has_status_in_set(unit, MAIN_STATUS_CHECK_SET_PROVISIONAL_TEAM_LOSS_ENEMY);
                if (selected != 0) {
                    if (unit->initial_team_flags & BATTLE_TEAM_MASK) {
                        enemy_unit_count++;
                    } else {
                        player_unit_count++;
                    }
                }
            }
        }
        unit_index++;
    } while (unit_index < BATTLE_UNIT_SLOT_COUNT);

    if (player_unit_count != 0) {
        return -1;
    }
    if (enemy_unit_count != 0) {
        return 0;
    }

    unit_index = 0;
    do {
        unit = &g_battle_unit_stats[unit_index];
        if (unit->entd_slot != BATTLE_ENTD_SLOT_NONE) {
            selected = unit->initial_team_flags & BATTLE_TEAM_MASK;
            if (main_unit_has_status_in_set(unit, MAIN_STATUS_CHECK_SET_KO) == 0) {
                if (selected != 0) {
                    enemy_unit_count++;
                } else {
                    player_unit_count++;
                }
            }
        }
        unit_index++;
    } while (unit_index < BATTLE_UNIT_SLOT_COUNT);

    if (player_unit_count == 0) {
        return -1;
    }
    return enemy_unit_count != 0;
}
