#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_unit.h"
#include "fft/memory.h"
#include "fft/unit.h"
#include "psx/types.h"

/* Fills a unit's action result for picking up a crystal or treasure
 * (BATTLE_CRYSTAL_* flags).
 *
 * The result starts as a canceled miss. TAKE_TREASURE adds
 * g_main_crystal_treasure_item_id to the inventory and returns that call's
 * result, ignoring the other flags. RESTORE_HP_MP turns the result into a hit
 * that heals the missing HP and MP. LEARN_ABILITIES ORs
 * g_main_crystal_learnable_abilities into the unit's learned-ability bits. These
 * return 0. */
s32 battle_unit_learn_from_crystal(unit_stats_t* unit, s32 flags) {
    s32 i;
    u8* dst;
    u8* src;
    u8* learned;
    u8* crystal_bits;
    u8* end;

    action_clear_current_data(&unit->action);
    unit->action.miss_type = ACTION_MISS_TYPE_CANCELED;
    unit->action.hit = 0;
    if (flags & BATTLE_CRYSTAL_TAKE_TREASURE) {
        return action_increment_item_quantity_for_steal_break(unit, g_main_crystal_treasure_item_id);
    }
    if (flags & BATTLE_CRYSTAL_RESTORE_HP_MP) {
        unit->action.hit = 1;
        unit->action.attack_type = ACTION_TYPE_HP_HEALING | ACTION_TYPE_MP_HEALING;
        unit->action.miss_type = ACTION_MISS_TYPE_HIT;
        unit->action.hp_healing = unit->max_hp - unit->hp;
        unit->action.mp_healing = unit->max_mp - unit->mp;
    }
    if (flags & BATTLE_CRYSTAL_LEARN_ABILITIES) {
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
                /* The signed compare is byte-required (slt, not sltu). */
            } while ((s32)learned < (s32)end);
            src += 3;
            i++;
            dst += 3;
        } while (i < UNIT_LEARNED_ABILITY_JOB_COUNT);
    }
    return 0;
}
