#include "fft/battle_ability.h"
#include "fft/main_runtime.h"
#include "psx/types.h"

/*
 * The remaining secondary-data bases are pre-biased so that indexing them by
 * the raw ability id lands on the SCUS Data Tables entries:
 *   0x80060ea0 + 0x170     = 0x80061010  item_t abilities (1 byte each)
 *   0x80060ea2 + 0x17e     = 0x80061020  Throw abilities (1 byte each)
 *   0x80060d18 + 0x18a * 2 = 0x8006102c  Jump, Charge and Math (2 bytes each)
 *   0x80060eb6 + 0x1a6     = 0x8006105c  R/S/M abilities (1 byte each)
 * They are not objects in their own right, so they keep address names.
 */
extern u8 g_main_math_rsm_ability_data_by_ability_id[];

s32 main_ability_calculate_pointers_and_type(s32 ability_id, u8** out_ability_data, u8** out_secondary_data) {
    ability_id &= ABILITY_ID_MASK;
    /* 0x8005ebf0: Ability Data 1, 8 bytes each. */
    *out_ability_data = (u8*)&g_main_ability_data[ability_id];
    if (ability_id < ABILITY_ID_ITEM_FIRST) {
        *out_secondary_data = (u8*)&g_main_ability_range_data[ability_id];
        return ABILITY_TYPE_DEFAULT;
    }
    if (ability_id < ABILITY_ID_THROW_FIRST) {
        *out_secondary_data = &g_main_item_ability_item_ids_by_ability_id[ability_id];
        return ABILITY_TYPE_ITEM;
    }
    if (ability_id < ABILITY_ID_JUMP_FIRST) {
        *out_secondary_data = &g_main_throw_ability_item_types_by_ability_id[ability_id];
        return ABILITY_TYPE_THROW;
    }
    if (ability_id < ABILITY_ID_CHARGE_FIRST) {
        *out_secondary_data = &g_main_jump_charge_ability_data_by_ability_id[ability_id * 2];
        return ABILITY_TYPE_JUMP;
    }
    if (ability_id < ABILITY_ID_MATH_FIRST) {
        *out_secondary_data = &g_main_jump_charge_ability_data_by_ability_id[ability_id * 2];
        return ABILITY_TYPE_CHARGE;
    }
    if (ability_id < ABILITY_ID_REACTION_FIRST) {
        *out_secondary_data = &g_main_math_rsm_ability_data_by_ability_id[ability_id];
        return ABILITY_TYPE_MATH;
    }
    if (ability_id < ABILITY_ID_SUPPORT_FIRST) {
        *out_secondary_data = &g_main_math_rsm_ability_data_by_ability_id[ability_id];
        return ABILITY_TYPE_REACTION;
    }
    if (ability_id < ABILITY_ID_MOVEMENT_FIRST) {
        *out_secondary_data = &g_main_math_rsm_ability_data_by_ability_id[ability_id];
        return ABILITY_TYPE_SUPPORT;
    }
    *out_secondary_data = &g_main_math_rsm_ability_data_by_ability_id[ability_id];
    return ABILITY_TYPE_MOVEMENT;
}
