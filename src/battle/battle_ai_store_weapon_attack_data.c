#include "fft/battle_ai.h"
#include "fft/data.h"
#include "fft/main_runtime.h"

/*
 * Merge one weapon's attack properties into the unit's AI weapon summary.
 *
 * Range and weapon ID are replaced; elements and trajectory/formula flags
 * accumulate. Calling for the left hand last gives its range precedence.
 */
void battle_ai_store_weapon_attack_data(battle_ai_weapon_data_t* out, s32 item_id, s32* flags_out) {
    weapon_data_t* weapon;
    s32 flags;
    s32 formula;

    if (item_id == ITEM_ID_NONE) {
        return;
    }
    weapon = &g_main_item_weapon_data[item_id];
    flags = weapon->flags;
    if (flags & WEAPON_FLAG_ARC) {
        out->bytes.flags_3 |= ABILITY_SECONDARY_FLAG_3_REFLECTABLE;
    } else if (flags & WEAPON_FLAG_DIRECT) {
        out->bytes.flags_3 |= ABILITY_SECONDARY_FLAG_3_CALCULATOR_ELIGIBLE;
    }
    formula = weapon->formula;
    switch (formula) {
    case 6:
        *flags_out |= 2;
        break;
    case 7:
        *flags_out |= 4;
        break;
    default:
        *flags_out |= 1;
        break;
    }
    out->bytes.weapon_id = (u8)item_id;
    out->bytes.range = weapon->range;
    out->bytes.element |= weapon->element;
}
