#include "fft/battle.h"
#include "psx/types.h"

void battle_formula_apply_elemental(void) {
    u8 element;
    battle_action_data_t* action;
    battle_stats_t* unit;
    s32 mount_info;

    element = g_current_ability.range_data.element;
    /* Oil (status 3 bit 0x80) doubles fire (element bit 0x80) and is removed. */
    if ((g_battle_action_target->status_sets.current[2] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_OIL))
        && (element & BATTLE_ELEMENT_FIRE)) {
        action = g_battle_action_target_data;
        g_current_ability.xa = (s16)g_current_ability.xa * 2;
        action->status_removal[2] |= BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_OIL);
        if (battle_status_modify_inflictions(0) != 0) {
            g_battle_action_target_data->attack_type = BATTLE_ACTION_TYPE_STATUS_CHANGE;
        }
    }
    unit = g_battle_action_target;
    mount_info = unit->mount_info;
    if (mount_info & BATTLE_MOUNT_INFO_FLAG_RIDER) {
        unit = &g_battle_unit_stats[mount_info & BATTLE_MOUNT_INFO_PARTNER_ID_MASK];
    }
    /* Float (status 3 bit 0x40) nullifies earth (element bit 0x08). */
    if ((unit->status_sets.current[2] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_FLOAT))
        && (element & BATTLE_ELEMENT_EARTH)) {
        /* The target loads mount_info into a0 although the callee takes none. */
        ((void (*)(s32))battle_formula_nullify_action)(mount_info);
        g_battle_action_target_data->miss_type = BATTLE_ACTION_MISS_TYPE_FORCED_FAILURE;
    }
    battle_formula_modify_damage_for_element(element);
}
