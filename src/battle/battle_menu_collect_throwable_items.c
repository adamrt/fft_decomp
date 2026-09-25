#include "fft/battle.h"
#include "psx/types.h"

/* Collect the item ids the unit may throw into `out`, terminated with -1.
 *
 * The skillset's abilities select item types through g_main_throw_ability_item_types; every item of
 * a selected type that is throwable and that the party still owns is listed.
 * */
s32 battle_menu_collect_throwable_items(s32 unit_id, u8 skillset, u8* out) {
    u16 abilities[0x14];
    u8 item_types[0x14];
    u8 mp_costs[0x14];
    u8 charge_times[0x14];
    u8 ability_flags[0x14];
    u8 turn_counts[0x14];
    struct battle_stats* unit;
    s32 count;
    s32 ability_count;
    s32 i;
    s32 j;
    s32 found;
    item_data_t* item;
    u8* type_table;
    s16 throw_index;
    u8 type;
    u8 item_id;

    count = 0;
    unit = battle_unit_get_existing_pointer(unit_id);
    if (unit == 0) {
        return 0;
    }
    if (skillset != SKILLSET_ID_THROW
        || (ability_count = battle_menu_get_unit_skillset_ability_data(
                unit_id, SKILLSET_ID_THROW, (s16*)abilities, mp_costs, charge_times, 0, ability_flags, turn_counts))
            == 0) {
        *out = 0xff;
        return 0;
    }
    for (i = 0; i < ability_count; i++) {
        if ((u16)(abilities[i] - 0x17e) < 12) {
            type_table = g_main_throw_ability_item_types;
            throw_index = abilities[i];
            throw_index = throw_index - 0x17e;
            item_types[i] = type_table[throw_index];
        }
    }
    for (i = 0; i < 0x100; i++) {
        item = &g_main_item_primary_data[i];
        type = item->type;
        found = 0;
        for (j = 0; j < ability_count; j++) {
            if (type == item_types[j]) {
                found = 1;
                break;
            }
        }
        if (found != 0 && (item->type_flags & 0x80) != 0
            && (g_main_item_weapon_data[item->secondary_data_id].flags & 2) != 0) {
            item_id = i;
            if (battle_action_decrement_player_item_quantity(unit, item_id, 0) == 0) {
                *out = item_id;
                out++;
                count++;
            }
        }
    }
    *out = 0xff;
    return count + 0x100;
}
