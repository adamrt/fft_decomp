#include "fft/battle_text.h"
#include "fft/main.h"
#include "fft/memory.h"
#include "fft/unit.h"
#include "fft/world.h"
#include "psx/libc.h"
#include "psx/types.h"

/* Classify an ability id and return pointers to its two table records.
 *
 * *out_ability_data always receives the common 8-byte record. *out_secondary_data
 * receives the record of the id's range: the range/effect record of an ordinary
 * ability, the item id of an Item ability, the item type of a Throw ability, the
 * Jump or Charge record, or the one-byte record shared by Math, reaction, support
 * and movement abilities. The result is the matching ability_type_e. */
s32 main_ability_calculate_pointers_and_type(
    s32 ability_id, ability_data_t** out_ability_data, main_ability_secondary_pointer_t* out_secondary_data) {
    ability_id &= ABILITY_ID_MASK;
    /* 0x8005ebf0: Ability Data 1, 8 bytes each. */
    *out_ability_data = &g_main_ability_data[ability_id];
    if (ability_id < ABILITY_ID_ITEM_FIRST) {
        out_secondary_data->range = &g_main_ability_range_data[ability_id];
        return ABILITY_TYPE_DEFAULT;
    }
    if (ability_id < ABILITY_ID_THROW_FIRST) {
        out_secondary_data->item_id = &g_main_item_ability_item_ids[ability_id - ABILITY_ID_ITEM_FIRST];
        return ABILITY_TYPE_ITEM;
    }
    if (ability_id < ABILITY_ID_JUMP_FIRST) {
        out_secondary_data->throw_item_type = &g_main_throw_ability_item_types[ability_id - ABILITY_ID_THROW_FIRST];
        return ABILITY_TYPE_THROW;
    }
    if (ability_id < ABILITY_ID_CHARGE_FIRST) {
        out_secondary_data->jump = &g_main_jump_ability_data[ability_id - ABILITY_ID_JUMP_FIRST];
        return ABILITY_TYPE_JUMP;
    }
    if (ability_id < ABILITY_ID_MATH_FIRST) {
        out_secondary_data->charge = &g_main_charge_ability_data[ability_id - ABILITY_ID_CHARGE_FIRST];
        return ABILITY_TYPE_CHARGE;
    }
    if (ability_id < ABILITY_ID_REACTION_FIRST) {
        out_secondary_data->math_rsm = &g_main_math_rsm_ability_data[ability_id - ABILITY_ID_MATH_FIRST];
        return ABILITY_TYPE_MATH;
    }
    if (ability_id < ABILITY_ID_SUPPORT_FIRST) {
        out_secondary_data->math_rsm = &g_main_math_rsm_ability_data[ability_id - ABILITY_ID_MATH_FIRST];
        return ABILITY_TYPE_REACTION;
    }
    if (ability_id < ABILITY_ID_MOVEMENT_FIRST) {
        out_secondary_data->math_rsm = &g_main_math_rsm_ability_data[ability_id - ABILITY_ID_MATH_FIRST];
        return ABILITY_TYPE_SUPPORT;
    }
    out_secondary_data->math_rsm = &g_main_math_rsm_ability_data[ability_id - ABILITY_ID_MATH_FIRST];
    return ABILITY_TYPE_MOVEMENT;
}
