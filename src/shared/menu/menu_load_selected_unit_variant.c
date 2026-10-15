#include "fft/battle_action.h"
#include "fft/battle_menu.h"
#include "fft/main.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/thread.h"
#include "fft/unit.h"

/* Fill selected and comparison billboards, stats, equipment and ability names. */
void menu_load_selected_unit_variant(void) {
    unit_stats_t* unit;
    menu_status_panel_slot_state_t* editor;
    u16* innate_items;
    item_data_t* item;
    s32 i;
    s32 output_index;
    s32 item_id;
    u8 item_flags;
    u8 field_value;
    s16 list_value;

    unit = battle_unit_get_stats_from_battle_id(g_menu_comparison_unit_data.battle_id);
    editor = &g_menu_selected_unit_fields.state;
    if (unit != 0) {
        menu_copy_unit_data_to_status_billboard_variant(unit, &g_menu_comparison_unit_status);
    }

    unit = battle_unit_get_stats_from_battle_id(g_menu_selected_unit.battle_id);
    if (unit == 0)
        return;
    menu_copy_unit_data_to_status_billboard_variant(unit, &g_menu_selected_unit_status);

    g_menu_selected_unit.job_id = unit->job_id;
    g_menu_selected_unit.brave = unit->brave;
    g_menu_selected_unit.faith = unit->faith;
    /* The zodiac sign lives in the top nibble of the 16-bit value at 0x008. */
    g_menu_selected_unit.zodiac = unit->birthday.zodiac;

    editor->move = unit->move;
    editor->speed = unit->attributes[UNIT_ATTRIBUTE_SPEED];
    editor->jump = unit->jump;
    editor->right_weapon_evade = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_WEAPON_EVADE];
    editor->left_weapon_evade = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_LEFT_WEAPON_EVADE];
    editor->right_weapon_power = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_WEAPON_POWER];
    editor->left_weapon_power = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_LEFT_WEAPON_POWER];
    editor->physical_attack = unit->attributes[UNIT_ATTRIBUTE_PHYSICAL_ATTACK];
    editor->physical_class_evade = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_CLASS_PHYSICAL_EVADE];
    editor->physical_shield_evade = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_SHIELD_PHYSICAL_EVADE];
    if (unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_SHIELD_PHYSICAL_EVADE]
        <= unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_LEFT_SHIELD_PHYSICAL_EVADE])
        editor->physical_shield_evade = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_LEFT_SHIELD_PHYSICAL_EVADE];
    editor->physical_accessory_evade = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_ACCESSORY_PHYSICAL_EVADE];
    editor->magical_attack = unit->attributes[UNIT_ATTRIBUTE_MAGIC_ATTACK];
    editor->magical_class_evade = 0;
    editor->magical_shield_evade = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_SHIELD_MAGIC_EVADE];
    if (unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_SHIELD_MAGIC_EVADE]
        <= unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_LEFT_SHIELD_MAGIC_EVADE])
        editor->magical_shield_evade = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_LEFT_SHIELD_MAGIC_EVADE];
    editor->magical_accessory_evade = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_ACCESSORY_MAGIC_EVADE];

    for (i = 4; i >= 0; i--)
        editor->equipment[i] = 0;

    if (!(unit->unit_flags & UNIT_FLAG_MONSTER)) {
        editor->equipment[0] = unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON] != ITEM_ID_NONE
            ? unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON]
            : unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_SHIELD];
        editor->equipment[1] = unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON] != ITEM_ID_NONE
            ? unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON]
            : unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_SHIELD];
        editor->equipment[2] = unit->equipment[UNIT_EQUIPMENT_SLOT_HEAD];
        editor->equipment[3] = unit->equipment[UNIT_EQUIPMENT_SLOT_BODY];
        editor->equipment[4] = unit->equipment[UNIT_EQUIPMENT_SLOT_ACCESSORY];
    }

    for (i = 0; i < 5; i++) {
        if (editor->equipment[i] == ITEM_ID_NOTHING || editor->equipment[i] == ITEM_ID_NONE)
            editor->equipment[i] = -1;
    }

    editor->generic_monster = 0;
    if (unit->primary_skillset >= SKILLSET_ID_MONSTER_FIRST && unit->primary_skillset < SKILLSET_ID_END)
        editor->generic_monster = 1;

    if (editor->generic_monster != 0) {
        innate_items = main_ability_store_skillset_abilities(unit->primary_skillset, SKILLSET_ABILITY_FILTER_ACTION);
        i = 0;
        do {
            editor->ability_ids[i] = *innate_items++;
            i++;
        } while (i < 5);

        g_thread_call_target = battle_menu_init_monster_skill_check;
        if (thread_call_on_main_stack(unit) == 0)
            editor->ability_ids[3] = 0;

        output_index = 0;
        for (item_id = 0; item_id < 5; item_id++) {
            if (editor->ability_ids[item_id] != 0)
                editor->ability_ids[output_index++] = editor->ability_ids[item_id];
        }
        while (output_index < 5)
            editor->ability_ids[output_index++] = 0;
    } else {
        field_value = unit->primary_skillset;
        editor->ability_ids[0] = field_value;
        if (field_value == 0)
            g_menu_billboard_skillset_name[0] = 0xfe;
        field_value = unit->secondary_skillset;
        editor->ability_ids[1] = field_value;
        if (field_value == 0)
            g_menu_billboard_secondary_skillset_name[1] = 0xfe;
        editor->ability_ids[2] = unit->abilities.reaction;
        editor->ability_ids[3] = unit->abilities.support;
        editor->ability_ids[4] = unit->abilities.movement;
    }

    for (i = 0; i < 5; i++) {
        list_value = editor->ability_ids[i];
        if (list_value == 0)
            editor->ability_ids[i] = list_value - 1;
    }

    item_id = ITEM_ID_NOTHING;
    editor->two_hands = 0;
    if (editor->equipment[0] != -1) {
        if (editor->equipment[1] == -1)
            item_id = editor->equipment[0];
    } else if (editor->equipment[1] != -1) {
        item_id = editor->equipment[1];
    }

    if (item_id != ITEM_ID_NOTHING) {
        item = main_item_get_data_pointer(item_id);
        if (item->type_flags & ITEM_TYPE_FLAG_WEAPON) {
            item_flags = g_main_item_weapon_data[item_id].flags;
            if ((item_flags & WEAPON_FLAG_FORCED_TWO_HANDS)
                || ((unit->ability_flags.support[2] & BATTLE_SUPPORT_SET_3_TWO_HANDS)
                    && (item_flags & WEAPON_FLAG_TWO_HANDS_COMPATIBLE)))
                editor->two_hands = 1;
        }
    }

    text_fill_field_with_terminators(g_menu_billboard_unit_name);
    text_fill_field_with_terminators(g_menu_billboard_job_name);
    text_fill_field_with_terminators(g_menu_billboard_skillset_name);
    text_fill_field_with_terminators(g_menu_billboard_secondary_skillset_name);
    /* The editor uses 16-byte windows at each text destination, including
     * the two eight-byte skillset names; preserve those target copy lengths. */
    mem_copy_bytes_nonempty(g_menu_billboard_unit_name, unit->name, 0x10);
    mem_copy_bytes_nonempty(g_menu_billboard_job_name, unit->job_name, 0x10);
    mem_copy_bytes_nonempty(g_menu_billboard_skillset_name, unit->primary_skillset_name, 0x10);
    mem_copy_bytes_nonempty(g_menu_billboard_secondary_skillset_name, unit->secondary_skillset_name, 0x10);
}
