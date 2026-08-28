#include "fft/battle.h"
#include "fft/data.h"
#include "fft/main_runtime.h"

/* Picks a random equippable item for the unit in one equipment slot.
 *
 * item_type_flag is an ITEM_TYPE_FLAG_* category, weapon_flag_mask is an
 * accepted WEAPON_FLAG_*, and required_item_type is the type required by an
 * Equip X skill, or ITEM_TYPE_FILTER_ANY for any type. */
s32 main_unit_calculate_random_equipment(
    battle_stats_t* unit, u8 item_type_flag, u8 weapon_flag_mask, u8 required_item_type) {
    u8 candidates[128];
    s32 first;
    s32 last;
    s32 i;
    u8 count;
    u8 best;
    u32 level;
    u8 value; /* item byte / candidate index scratch */
    u32 type;
    item_data_t* item;

    switch (item_type_flag) {
    case ITEM_TYPE_FLAG_WEAPON:
        first = ITEM_ID_DAGGER;
        last = ITEM_ID_SHIELD_FIRST;
        break;
    case ITEM_TYPE_FLAG_SHIELD:
        first = ITEM_ID_SHIELD_FIRST;
        last = ITEM_ID_HEADGEAR_FIRST;
        break;
    case ITEM_TYPE_FLAG_HEADGEAR:
        first = ITEM_ID_HEADGEAR_FIRST;
        last = ITEM_ID_BODY_ARMOR_FIRST;
        break;
    case ITEM_TYPE_FLAG_BODY_ARMOR:
        first = ITEM_ID_BODY_ARMOR_FIRST;
        last = ITEM_ID_ACCESSORY_FIRST;
        break;
    case ITEM_TYPE_FLAG_ACCESSORY:
        first = ITEM_ID_ACCESSORY_FIRST;
        last = ITEM_ID_CONSUMABLE_FIRST;
        break;
    default:
        first = ITEM_ID_NOTHING;
        last = ITEM_ID_COUNT;
        break;
    }

    count = 0;
    level = unit->level;
    best = 0;
    for (i = first; i < last; i++) {
        item = &g_main_item_primary_data[i];
        value = item->type_flags;
        if (value & ITEM_TYPE_FLAG_RARE) {
            continue;
        }
        value = item->type;
        if (required_item_type != ITEM_TYPE_FILTER_ANY && value != required_item_type) {
            continue;
        }
        type = value;
        if ((unit->equipment_categories[EQUIPPABLE_ITEM_TYPE_BYTE_INDEX(type)] & EQUIPPABLE_ITEM_TYPE_BYTE_MASK(type))
            == 0) {
            continue;
        }
        if (weapon_flag_mask != 0) {
            if ((weapon_flag_mask & g_main_item_weapon_data[item->secondary_data_id].flags) == 0) {
                continue;
            }
        }
        value = item->required_level;
        if (level < value) {
            continue;
        }
        if (best < value) {
            best = value;
            count = 0;
        }
        candidates[count++] = i;
    }

    if (count == 0) {
        return ENTD_EQUIPMENT_RANDOM;
    }
    value = (rand() * count) / 0x8000;
    return candidates[value];
}
