#include "fft/battle.h"
#include "psx/types.h"

/* Fills the preview-target and selected-unit billboards and the small
 * battle display fields (job, brave, faith, zodiac, stats, equipment and
 * ability names) for the selected unit.
 *
 * BATTLE twin of debugchr_editor_load_selected_unit and
 * require_editor_load_selected_unit. */
void battle_menu_store_units_small_in_battle_display_data(void) {
    battle_stats_t* unit;
    s16* editor;
    u16* innate_items;
    s16* ability_slot;
    item_data_t* item;
    s32 i;
    s32 output_index;
    s32 item_id;
    u8 item_flags;
    u8 field_value;
    s16 list_value;

    unit = battle_unit_get_stats_from_battle_id(g_battle_preview_target_unit_id);
    editor = g_battle_menu_billboard_data;
    if (unit != 0) {
        battle_menu_copy_unit_data_to_status_billboard(unit, &g_battle_menu_status_billboard);
    }

    unit = battle_unit_get_stats_from_battle_id(g_battle_active_turn_unit.battle_id);
    if (unit == 0)
        return;
    battle_menu_copy_unit_data_to_status_billboard(unit, &g_battle_menu_active_turn_banner);

    g_battle_active_turn_unit.job_id = unit->job_id;
    g_battle_active_turn_unit.brave = unit->brave;
    g_battle_active_turn_unit.faith = unit->faith;
    /* The zodiac sign lives in the top nibble of the halfword at 0x008. */
    /* Direct union-member access hoists the halfword load above the faith
     * store; keep the typed field's aliased word view. */
    g_battle_active_turn_unit.zodiac = *(u16*)&unit->birthday >> 12;

    editor[0] = unit->move;
    editor[1] = unit->attributes[UNIT_ATTRIBUTE_SPEED];
    editor[2] = unit->jump;
    editor[5] = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_WEAPON_EVADE];
    editor[6] = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_LEFT_WEAPON_EVADE];
    editor[3] = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_WEAPON_POWER];
    editor[4] = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_LEFT_WEAPON_POWER];
    editor[9] = unit->attributes[UNIT_ATTRIBUTE_PHYSICAL_ATTACK];
    editor[10] = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_CLASS_PHYSICAL_EVADE];
    editor[11] = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_SHIELD_PHYSICAL_EVADE];
    if (unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_SHIELD_PHYSICAL_EVADE]
        <= unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_LEFT_SHIELD_PHYSICAL_EVADE])
        editor[11] = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_LEFT_SHIELD_PHYSICAL_EVADE];
    editor[12] = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_ACCESSORY_PHYSICAL_EVADE];
    editor[14] = unit->attributes[UNIT_ATTRIBUTE_MAGIC_ATTACK];
    editor[15] = 0;
    editor[16] = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_SHIELD_MAGIC_EVADE];
    if (unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_SHIELD_MAGIC_EVADE]
        <= unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_LEFT_SHIELD_MAGIC_EVADE])
        editor[16] = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_LEFT_SHIELD_MAGIC_EVADE];
    editor[17] = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_ACCESSORY_MAGIC_EVADE];

    for (i = 4; i >= 0; i--)
        editor[18 + i] = 0;

    if (!(unit->unit_flags & UNIT_FLAG_MONSTER)) {
        editor[18] = unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON] != ITEM_ID_NONE
            ? unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON]
            : unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_SHIELD];
        editor[19] = unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON] != ITEM_ID_NONE
            ? unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON]
            : unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_SHIELD];
        editor[20] = unit->equipment[UNIT_EQUIPMENT_SLOT_HEAD];
        editor[21] = unit->equipment[UNIT_EQUIPMENT_SLOT_BODY];
        editor[22] = unit->equipment[UNIT_EQUIPMENT_SLOT_ACCESSORY];
    }

    for (i = 0; i < 5; i++) {
        if (editor[18 + i] == ITEM_ID_NOTHING || editor[18 + i] == ITEM_ID_NONE)
            editor[18 + i] = -1;
    }

    editor[7] = 0;
    if ((u8)(unit->primary_skillset + 0x50) < 0x30)
        editor[7] = 1;

    if (editor[7] != 0) {
        innate_items = main_ability_store_skillset_abilities(unit->primary_skillset, SKILLSET_ABILITY_FILTER_ACTION);
        i = 0;
        ability_slot = editor;
        do {
            ability_slot[23] = *innate_items++;
            i++;
            ability_slot++;
        } while (i < 5);

        g_battle_thread_call_target = battle_menu_init_monster_skill_check;
        if (battle_thread_call_on_main_stack(unit) == 0)
            editor[26] = 0;

        output_index = 0;
        for (item_id = 0; item_id < 5; item_id++) {
            if (editor[23 + item_id] != 0)
                editor[23 + output_index++] = editor[23 + item_id];
        }
        while (output_index < 5)
            editor[23 + output_index++] = 0;
    } else {
        field_value = unit->primary_skillset;
        editor[23] = field_value;
        if (field_value == 0)
            g_battle_menu_billboard_skillset_name[0] = 0xfe;
        field_value = unit->secondary_skillset;
        editor[24] = field_value;
        if (field_value == 0)
            g_battle_menu_billboard_secondary_skillset_name[1] = 0xfe;
        editor[25] = unit->reaction_ability;
        editor[26] = unit->support_ability;
        editor[27] = unit->movement_ability;
    }

    for (i = 0; i < 5; i++) {
        list_value = editor[23 + i];
        if (list_value == 0)
            editor[23 + i] = list_value - 1;
    }

    item_id = ITEM_ID_NOTHING;
    editor[8] = 0;
    if (editor[18] != -1) {
        if (editor[19] == -1)
            item_id = editor[18];
    } else if (editor[19] != -1) {
        item_id = editor[19];
    }

    if (item_id != ITEM_ID_NOTHING) {
        item = main_item_get_data_pointer(item_id);
        if (item->type_flags & ITEM_TYPE_FLAG_WEAPON) {
            item_flags = g_main_item_weapon_data[item_id].flags;
            if ((item_flags & WEAPON_FLAG_FORCED_TWO_HANDS)
                || ((unit->support_abilities[2] & BATTLE_SUPPORT_SET_3_TWO_HANDS)
                    && (item_flags & WEAPON_FLAG_TWO_HANDS_COMPATIBLE)))
                editor[8] = 1;
        }
    }

    battle_text_fill_field_with_terminators(g_battle_menu_billboard_unit_name);
    battle_text_fill_field_with_terminators(g_battle_menu_billboard_job_name);
    battle_text_fill_field_with_terminators(g_battle_menu_billboard_skillset_name);
    battle_text_fill_field_with_terminators(g_battle_menu_billboard_secondary_skillset_name);
    /* The editor uses 16-byte windows at each text destination, including
     * the two eight-byte skillset names; preserve those target copy lengths. */
    battle_copy_bytes(g_battle_menu_billboard_unit_name, unit->name, 0x10);
    battle_copy_bytes(g_battle_menu_billboard_job_name, unit->job_name, 0x10);
    battle_copy_bytes(g_battle_menu_billboard_skillset_name, unit->primary_skillset_name, 0x10);
    battle_copy_bytes(g_battle_menu_billboard_secondary_skillset_name, unit->secondary_skillset_name, 0x10);
}
