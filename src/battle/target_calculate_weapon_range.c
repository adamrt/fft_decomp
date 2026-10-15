#include "fft/battle.h"
#include "fft/battle_map.h"
#include "fft/battle_menu.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/*
 * Mark the targeting panels in range of the unit's weapon.
 *
 * Frog uses the bare-hand entry; an empty right hand falls back to the left
 * hand, and non-weapon items also use entry 0. Striking, lunging, and direct
 * weapon flags select the special range shapes; direct also removes the close
 * range.
 */
void target_calculate_weapon_range(const unit_stats_t* unit) {
    s32 y;
    s32 x;
    s32 index;
    u8 weapon;
    u8 range;
    u8 flags;
    weapon_data_t* data;
    target_panel_t* panel;
    target_panel_t* upper;

    y = unit->position.y;
    weapon = unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON];
    x = unit->x;
    index = x + y * g_map_max_x;
    if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FROG)] & STATUS_BYTE_MASK(STATUS_ID_FROG)) {
        weapon = ITEM_ID_NOTHING;
    }
    if (weapon == ITEM_ID_NONE) {
        weapon = unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON];
        if (weapon == ITEM_ID_NONE) {
            weapon = ITEM_ID_NOTHING;
        }
    }
    if (weapon >= ITEM_ID_SHIELD_FIRST) {
        weapon = ITEM_ID_NOTHING;
    }
    data = &g_main_item_weapon_data[weapon];
    range = data->range;
    flags = data->flags;
    panel = &g_target_panels[index];
    upper = &g_target_panels[index + 0x100];
    panel->remaining_range = range + 1;
    upper->remaining_range = range + 1;
    panel->mark = 1;
    if (flags & (WEAPON_FLAG_STRIKING | WEAPON_FLAG_LUNGING)) {
        target_calculate_strike_lunge_range(unit, flags);
    } else if (flags & WEAPON_FLAG_DIRECT) {
        target_spread_panels(range, 0);
        target_remove_close_range(x, y, 2);
    } else {
        target_calculate_arc_range(unit, range, flags);
    }
    target_clear_range_on_untargetable_tiles();
}
