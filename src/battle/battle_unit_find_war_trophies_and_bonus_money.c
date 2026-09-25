#include "fft/battle.h"
#include "psx/types.h"

/*
 * Collect the war trophies and bonus money of every present unit into
 * `result`, zero-filling the unused trophy slots.
 */
void battle_unit_find_war_trophies_and_bonus_money(battle_war_result_t* result) {
    s32 count;
    s32 money;
    u8 lowest;
    s32 i;
    battle_stats_t* unit;
    u8 value;

    count = 0;
    money = 0;
    lowest = 99;
    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        unit = &g_battle_unit_stats[i];
        if (unit->existence != 0xFF && unit->existence != 0) {
            value = unit->war_trophy;
            if (value != ITEM_ID_NOTHING) {
                result->item_ids[count] = value;
                count++;
            }
            money += unit->bonus_money_modifier * 100;
            if (unit->formation_index != 0xFF) {
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
    result->level_sum_money = g_enemy_level_sum * 100;
    if (g_battle_player_unit_fallen != 0 || lowest >= g_highest_enemy_level) {
        result->level_bonus_money = 0;
    } else {
        result->level_bonus_money = (g_highest_enemy_level - lowest) * 100;
    }
}
