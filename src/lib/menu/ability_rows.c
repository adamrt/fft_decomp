/* Ability-list row callbacks: cached ability type, learned tests and MP/CT/JP display values. */

#include "fft/ability.h"
#include "fft/ability_list.h"
#include "fft/menu_script.h"
#include "psx/types.h"

extern ability_type_e g_ability_list_ability_type;
extern const ability_data_t* g_ability_list_selected_ability_data;
extern ability_secondary_pointer_t g_ability_list_selected_ability_secondary_data;

/* Row callback for ability lists: look up the listed ability (caching its data for the panel) and
 * return 1 when it is not an action ability and its entry carries no disabled bits. */
s32 ability_list_is_non_action_learned(s32 index) {
    s16* entries = g_ability_list_ability_entries;
    s16* entry = &entries[index];
    s32 result;
    u16 ability;

    g_ability_list_ability_type = ability_records_get(*entry & ABILITY_LIST_ENTRY_ID_MASK,
        &g_ability_list_selected_ability_data, &g_ability_list_selected_ability_secondary_data);
    result = 0;
    if (g_ability_list_ability_type != ABILITY_TYPE_DEFAULT) {
        ability = *entry;
        result = (ability >> ABILITY_LIST_ENTRY_DISABLED_SHIFT) == 0;
    }
    return result;
}

/* Returns 1 for an ability of any type other than ABILITY_TYPE_DEFAULT (by
 * the type the "learned" callback cached) whose entry has bit 14 or 15 set;
 * 0 otherwise. */
s32 ability_list_is_non_action_unlearned(s32 index) {
    s32 result = 0;

    if (g_ability_list_ability_type != ABILITY_TYPE_DEFAULT) {
        result = (g_ability_list_ability_entries[index] >> ABILITY_LIST_ENTRY_DISABLED_SHIFT) != 0;
    }
    return result;
}

/* Row callback: whether the ability the learned callback cached is an action ability
 * (ABILITY_TYPE_DEFAULT); the row is ignored. */
s32 ability_list_is_action(s32 unused_row) {
    return g_ability_list_ability_type == ABILITY_TYPE_DEFAULT;
}

/* Returns the selected action ability's displayed MP cost, from the
 * secondary record: dashes for an entry whose details are hidden, with the
 * disabled flag added for a disabled entry. */
s32 ability_list_get_mp_cost(s32 index) {
    s32 entry = g_ability_list_ability_entries[index];
    s32 result;

    if (entry & ABILITY_LIST_ENTRY_HIDE_DETAILS) {
        result = MENU_NUMBER_VALUE_DASHES;
    } else {
        result = g_ability_list_selected_ability_secondary_data.range->mp_cost;
    }
    if (entry >> ABILITY_LIST_ENTRY_DISABLED_SHIFT) {
        result |= MENU_NUMBER_VALUE_DISABLED;
    }
    return result;
}

/* Returns the selected action ability's displayed charge time:
 * ceil(100 / the secondary record's CT speed), dashes for an entry whose
 * details are hidden, with the disabled flag added for a disabled entry. */
s32 ability_list_get_ct(s32 index) {
    s32 entry = g_ability_list_ability_entries[index];
    s32 result;

    if (entry & ABILITY_LIST_ENTRY_HIDE_DETAILS) {
        result = MENU_NUMBER_VALUE_DASHES;
    } else {
        result = g_ability_list_selected_ability_secondary_data.range->ct;
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
s32 ability_list_get_jp_cost(s32 index) {
    s32 entry = g_ability_list_ability_entries[index];
    s32 result;

    if (entry & ABILITY_LIST_ENTRY_HIDE_DETAILS) {
        result = MENU_NUMBER_VALUE_DASHES;
    } else {
        result = g_ability_data[entry & ABILITY_LIST_ENTRY_ID_MASK].jp_cost;
    }
    if (entry >> ABILITY_LIST_ENTRY_DISABLED_SHIFT) {
        result |= MENU_NUMBER_VALUE_DISABLED;
    }
    return result;
}
