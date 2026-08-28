#include "fft/battle.h"
#include "fft/data.h"
#include "fft/main_runtime.h"
#include "psx/types.h"

/*
 * Collects the item abilities the unit can use into out_item_ids, terminated
 * by 0xff, and returns 0x100 plus the number of entries (0 when the unit or
 * the skillset is wrong, or the menu is empty).
 *
 * Loading the ability id into `item_index` before subtracting ABILITY_ID_ITEM_FIRST
 * keeps the table index in its own s16 pseudo, so GCC emits the target's fresh
 * `addiu v1,-0x170` on the sign-extended halfword instead of reusing the
 * masked `id + 0xfe90` sum the range test already holds (the two differ only
 * above bit 15); spelled as one expression it is one instruction short. The
 * `+ 0x10000` in the range test forces that sum into a register, matching the
 * target's hoisted `li t0,0xfe90`.
 */
s32 battle_menu_load_item_abilities(s32 unit_id, u8 skillset, u8* out_item_ids) {
    battle_stats_t* unit;
    u16 abilities[0x14];
    u8 mp_costs[0x18];
    u8 ability_ct[0x18];
    u8 ability_flags[0x18];
    u8 turns[0x18];
    s32 count;
    s32 i;
    s32 found;
    u8* table;
    s16 item_index;

    found = 0;
    unit = battle_unit_get_existing_pointer(unit_id);
    if (unit == 0) {
        return 0;
    }
    if (skillset != SKILLSET_ID_ITEM) {
        *out_item_ids = 0xff;
        return 0;
    }
    count = battle_menu_get_unit_skillset_ability_data(
        unit_id, SKILLSET_ID_ITEM, (s16*)abilities, mp_costs, ability_ct, 0, ability_flags, turns);
    if (count == 0) {
        *out_item_ids = 0xff;
        return 0;
    }
    for (i = 0; i < count; i++) {
        if ((u16)(abilities[i] - ABILITY_ID_ITEM_FIRST + 0x10000) < 14) {
            table = g_main_item_ability_item_ids;
            item_index = abilities[i];
            item_index = item_index - ABILITY_ID_ITEM_FIRST;
            found++;
            *out_item_ids = table[item_index];
            out_item_ids++;
        }
    }
    *out_item_ids = 0xff;
    return found + 0x100;
}
