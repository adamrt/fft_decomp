#include "fft/battle.h"
#include "psx/types.h"

s32 battle_unit_learn_from_crystal(battle_stats_t* unit, s32 flags) {
    s32 i;
    u8* dst;
    u8* src;
    u8* learned;
    u8* crystal_bits;
    u8* end;

    battle_action_clear_current_data(&unit->action);
    unit->action.miss_type = BATTLE_ACTION_MISS_TYPE_CANCELLED;
    unit->action.hit = 0;
    if (flags & 4) {
        return battle_action_increment_item_quantity_for_steal_break(unit, g_main_crystal_treasure_item_id);
    }
    if (flags & 2) {
        unit->action.hit = 1;
        unit->action.attack_type = BATTLE_ACTION_TYPE_HP_HEALING | BATTLE_ACTION_TYPE_MP_HEALING;
        unit->action.miss_type = BATTLE_ACTION_MISS_TYPE_HIT;
        unit->action.hp_healing = unit->max_hp - unit->hp;
        unit->action.mp_healing = unit->max_mp - unit->mp;
    }
    if (flags & 1) {
        /* OR the crystal's ability bits into learned_abilities, three bytes
         * per job for 19 jobs; walked as raw bytes to keep the biased pointers. */
        i = 0;
        src = g_main_crystal_learnable_abilities;
        dst = (u8*)unit;
        do {
            learned = dst + 0x99;
            crystal_bits = src;
            end = dst + 0x9C;
            do {
                *learned |= *crystal_bits;
                learned++;
                crystal_bits++;
            } while ((s32)learned < (s32)end);
            src += 3;
            i++;
            dst += 3;
        } while (i < UNIT_LEARNED_ABILITY_JOB_COUNT);
    }
    return 0;
}
