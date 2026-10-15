#include "fft/battle.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/* Collect the war trophies and bonus money of every present unit into
 * `result`, zero-filling the unused trophy slots.
 *
 * bonus_money is the sum of each present unit's bonus_money_modifier * 100.
 * level_sum_money is g_battle_enemy_level_sum * 100. level_bonus_money is
 * (highest enemy level - lowest level among the present party units) * 100, or
 * 0 when that is not positive or a player unit has fallen. */
void battle_unit_find_war_trophies_and_bonus_money(battle_war_result_t* result) {
    s32 count;
    s32 money;
    u8 lowest;
    s32 i;
    unit_stats_t* unit;
    u8 value;

    count = 0;
    money = 0;
    lowest = UNIT_LEVEL_MAX;
    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        unit = &g_battle_unit_stats[i];
        if (unit->existence != BATTLE_UNIT_EXISTENCE_NONE && unit->existence != BATTLE_UNIT_EXISTENCE_OFF_FIELD) {
            value = unit->war_trophy;
            if (value != ITEM_ID_NOTHING) {
                result->item_ids[count] = value;
                count++;
            }
            money += unit->bonus_money_modifier * 100;
            if (unit->formation_index != BATTLE_FORMATION_INDEX_NONE) {
                value = unit->level;
                if (value < lowest) {
                    lowest = value;
                }
            }
        }
    }
    result->item_count = count;
    for (i = count; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        result->item_ids[i] = ITEM_ID_NOTHING;
    }
    result->bonus_money = money;
    result->level_sum_money = g_battle_enemy_level_sum * 100;
    if (g_battle_player_unit_fallen != 0 || lowest >= g_battle_highest_enemy_level) {
        result->level_bonus_money = 0;
    } else {
        result->level_bonus_money = (g_battle_highest_enemy_level - lowest) * 100;
    }
}
