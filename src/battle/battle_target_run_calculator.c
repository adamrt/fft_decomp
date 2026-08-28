#include "fft/battle.h"
#include "fft/main_heap.h"
#include "fft/main_runtime.h"
#include "fft/map.h"
#include "fft/unit_slots.h"
#include "psx/types.h"

/* Marks the tiles of every unit selected by a Calculator (Arithmeticks) action.
 *
 * The type ability picks the compared value (CT, capped at 100 and forced to
 * 100 for the caster, level, EXP, or whole-unit height, where half heights never
 * qualify); the multiplier ability tests it for primality or divisibility by
 * 5, 4 or 3. Units that are absent, crystallized, jumping, treasure, or
 * flagged 0x40 in mount_info are skipped. Returns the number of marked units,
 * or -1 when the command does not name one type and one multiplier ability. */
s32 battle_target_run_calculator(const battle_ai_command_action_t* source) {
    battle_unit_height_data_t height;
    battle_ai_command_action_t action;
    battle_stats_t* actor;
    battle_stats_t* unit;
    map_tile_t* tile;
    u16 type;
    u16 multiplier;
    u8* type_entry;
    u8* multiplier_entry;
    u8 type_flags;
    u8 multiplier_flags;
    u8 value;
    s32 count;
    s32 hit;
    s32 i;
    s32 j;

    count = 0;
    main_util_copy_action_data(source, &action);
    type = action.calculator_type;
    multiplier = action.calculator_multiplier;
    actor = &g_battle_unit_stats[action.unit_id];
    if ((u16)(type - ABILITY_ID_MATH_FIRST) >= CALCULATOR_ABILITY_COUNT) {
        return -1;
    }
    if ((u16)(multiplier - ABILITY_ID_MATH_FIRST) >= CALCULATOR_ABILITY_COUNT) {
        return -1;
    }
    type_entry = &g_main_math_rsm_ability_data_by_ability_id[(s16)type];
    multiplier_entry = &g_main_math_rsm_ability_data_by_ability_id[(s16)multiplier];
    type_flags = *type_entry;
    multiplier_flags = *multiplier_entry;
    if (!(type_flags & CALCULATOR_ATTRIBUTE_MASK) || !(multiplier_flags & CALCULATOR_MULTIPLE_MASK)
        || (type_flags & CALCULATOR_MULTIPLE_MASK) || (multiplier_flags & CALCULATOR_ATTRIBUTE_MASK)) {
        return -1;
    }
    battle_target_disable_green_panel_flags();
    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        unit = &g_battle_unit_stats[i];
        if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE) {
            continue;
        }
        if (unit->status_sets.current[0]
            & (BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_CRYSTAL) | BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_JUMP))) {
            continue;
        }
        if (unit->status_sets.current[1] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_TREASURE)) {
            continue;
        }
        if (unit->mount_info & BATTLE_MOUNT_INFO_FLAG_MOUNT) {
            continue;
        }
        if (type_flags & CALCULATOR_ATTRIBUTE_CT) {
            value = unit->ct;
            if (value > 100 || unit == actor) {
                value = 100;
            }
        } else if (type_flags & CALCULATOR_ATTRIBUTE_LEVEL) {
            value = unit->level;
        } else if (type_flags & CALCULATOR_ATTRIBUTE_EXP) {
            value = unit->experience;
        } else {
            battle_calculate_unit_height_data(&height, i);
            value = height.walking_height;
            if (value & 1) {
                continue;
            }
            value >>= 1;
        }
        if (value == 0) {
            continue;
        }
        hit = 0;
        if (multiplier_flags & CALCULATOR_MULTIPLE_PRIME) {
            for (j = 0; j < CALCULATOR_PRIME_COUNT; j++) {
                if (g_main_calculator_primes[j] == value) {
                    hit = 1;
                    break;
                }
            }
        } else if ((multiplier_flags & CALCULATOR_MULTIPLE_FIVE) && value % 5 == 0) {
            hit = 1;
        } else if ((multiplier_flags & CALCULATOR_MULTIPLE_FOUR) && (value & 3) == 0) {
            hit = 1;
        } else if ((multiplier_flags & CALCULATOR_MULTIPLE_THREE) && value % 3 == 0) {
            hit = 1;
        }
        if (hit != 0) {
            tile = &g_battle_map_tile_data[battle_map_calculate_location(unit)];
            count++;
            tile->ceiling_depth_and_marks |= MAP_TILE_FLAG_TARGETED;
        }
    }
    return count;
}
