/* Learned tests and MP/CT/JP display values. */

#include "fft/data.h"
#include "fft/main_unit.h"
#include "fft/menu_panel.h"
#include "fft/menu_script.h"
#include "psx/types.h"

s32 menu_ability_is_non_action_learned(s32 index) {
    s16* entries;
    s16* entry;
    s32 result;
    u16 ability;

    entries = g_menu_ability_entries;
    entry = &entries[index];
    g_menu_ability_type = main_ability_calculate_pointers_and_type(
        *entry & ABILITY_LIST_ENTRY_ID_MASK, &g_menu_selected_ability_data, &g_menu_selected_ability_secondary_data);
    result = 0;
    if (g_menu_ability_type != ABILITY_TYPE_DEFAULT) {
        ability = *entry;
        result = (ability >> ABILITY_LIST_ENTRY_DISABLED_SHIFT) == 0;
    }
    return result;
}

/* Returns 1 for an ability of any type other than ABILITY_TYPE_DEFAULT (by
 * the type the "learned" callback cached) whose entry has bit 14 or 15 set;
 * 0 otherwise. */
s32 menu_ability_is_non_action_unlearned(s32 index) {
    s32 result;

    result = 0;
    if (g_menu_ability_type != ABILITY_TYPE_DEFAULT) {
        result = (g_menu_ability_entries[index] >> ABILITY_LIST_ENTRY_DISABLED_SHIFT) != 0;
    }
    return result;
}

/* Action abilities occupy type zero in the ability-pointer classification. */
s32 menu_ability_is_action(s32 unused_row) {
    return g_menu_ability_type == ABILITY_TYPE_DEFAULT;
}

/* Returns the selected action ability's displayed MP cost, from the
 * secondary record: dashes for an entry whose details are hidden, with the
 * disabled flag added for a disabled entry. */
s32 menu_ability_get_mp_cost_display_value(s32 index) {
    s32 entry = g_menu_ability_entries[index];
    s32 result;

    if (entry & ABILITY_LIST_ENTRY_HIDE_DETAILS) {
        result = MENU_NUMBER_VALUE_DASHES;
    } else {
        result = g_menu_selected_ability_secondary_data.range->mp_cost;
    }
    if (entry >> ABILITY_LIST_ENTRY_DISABLED_SHIFT) {
        result |= MENU_NUMBER_VALUE_DISABLED;
    }
    return result;
}

/* Returns the selected action ability's displayed charge time:
 * ceil(100 / the secondary record's CT speed), dashes for an entry whose
 * details are hidden, with the disabled flag added for a disabled entry. */
s32 menu_ability_get_ct_display_value(s32 index) {
    s32 entry = g_menu_ability_entries[index];
    s32 result;

    if (entry & ABILITY_LIST_ENTRY_HIDE_DETAILS) {
        result = MENU_NUMBER_VALUE_DASHES;
    } else {
        result = g_menu_selected_ability_secondary_data.range->ct;
        result = (100 / result) + ((100 % result) != 0);
    }
    if (entry >> ABILITY_LIST_ENTRY_DISABLED_SHIFT) {
        result |= MENU_NUMBER_VALUE_DISABLED;
    }
    return result;
}

/* Returns the displayed JP cost of an ability list entry: the ability's JP
 * cost, dashes for an entry whose details are hidden, with the disabled flag
 * added for a disabled entry. */
s32 menu_ability_get_jp_cost_display_value(s32 index) {
    s32 entry = g_menu_ability_entries[index];
    s32 result;

    if (entry & ABILITY_LIST_ENTRY_HIDE_DETAILS) {
        result = MENU_NUMBER_VALUE_DASHES;
    } else {
        result = g_main_ability_data[entry & ABILITY_LIST_ENTRY_ID_MASK].jp_cost;
    }
    if (entry >> ABILITY_LIST_ENTRY_DISABLED_SHIFT) {
        result |= MENU_NUMBER_VALUE_DISABLED;
    }
    return result;
}
