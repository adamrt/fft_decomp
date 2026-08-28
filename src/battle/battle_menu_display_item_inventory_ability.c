#include "fft/battle.h"
#include "psx/types.h"

/* Loads the unit's Item skillset entries through battle_menu_load_item_abilities and returns the
 * low byte of its result (the entry count). */
s32 battle_menu_display_item_inventory_ability(s32 unit_id, s32 skillset, u8* out_item_ids) {
    return battle_menu_load_item_abilities(unit_id, skillset & 0xFF, out_item_ids) & 0xFF;
}
