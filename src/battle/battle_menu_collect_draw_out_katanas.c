#include "fft/battle.h"
#include "fft/data.h"
#include "psx/types.h"

/* Collects the Draw Out weapon entry (ability id - 0x26) of each of the unit's
 * usable Draw Out abilities into out_item_ids, 0xff-terminated, and returns
 * 0x100 plus their count (0 when the unit or skillset is wrong or the list is
 * empty). */
s32 battle_menu_collect_draw_out_katanas(s32 unit_id, u8 skillset, u8* out_item_ids) {
    u16 abilities[0x10];
    u8 mp_costs[0x10];
    u8 ability_flags[0x10];
    u8 ability_ct[0x10];
    u8 turns[0x10];
    s32 ability_count;
    s32 i;
    s32 count;
    u32 ability_id;
    u32 item_id;

    count = 0;
    if (battle_unit_get_existing_pointer(unit_id) == 0) {
        return 0;
    }
    if (skillset != SKILLSET_ID_DRAW_OUT) {
        *out_item_ids = 0xFF;
        return 0;
    }
    ability_count = battle_menu_get_unit_skillset_ability_data(
        unit_id, 0x13, (s16*)abilities, mp_costs, ability_ct, 0, ability_flags, turns);
    if (ability_count == 0) {
        *out_item_ids = 0xFF;
        return 0;
    }
    for (i = 0; i < ability_count; i++) {
        ability_id = abilities[i];
        if (ability_id < ABILITY_ID_ITEM_FIRST) {
            item_id = ability_id - 0x26;
            *out_item_ids = item_id;
            out_item_ids++;
            count++;
        }
    }
    *out_item_ids = 0xFF;
    return count + 0x100;
}
