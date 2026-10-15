#include "fft/event_attack.h"

#include "psx/types.h"

/* gauges->team_kind. Accessed through the struct, GCC keeps its address in
 * a register across the three stores (1276 vs 1280 bytes); the target
 * addresses it as a standalone 16-bit value. */

/* ATTACK twin of require_editor_load_party_unit. Load a party roster unit into the ATTACK status/editor display.
 *
 * Party counterpart of require_editor_load_selected_unit: the unit is
 * rebuilt from the roster into a scratch unit_stats_t, shown at full HP/MP
 * and CT, and without the monster-skill or name-text refresh. */
void attack_editor_load_party_unit(s32 unused, s32 party_index) {
    unit_stats_t* unit;
    menu_status_panel_slot_state_t* editor;
    u16* innate_items;
    item_data_t* item;
    s32 i;
    s32 item_id;
    u8 item_flags;
    u8 field_value;
    u16 maximum_hp;
    u16 maximum_mp;
    menu_unit_status_record_t* gauges;

    main_unit_init_job_data(&g_attack_editor_party_unit_stats, party_index, 0);
    editor = &g_attack_editor_unit_fields.state;
    unit = &g_attack_editor_party_unit_stats;
    gauges = &g_attack_panel_selected_billboard;

    g_attack_panel_selected_unit_data.name_id = party_index;
    g_attack_panel_selected_unit_data.battle_id = party_index;
    gauges->team_kind = 0;
    g_attack_panel_selected_unit_data.job_id = unit->job_id;
    g_attack_panel_selected_unit_data.brave = unit->brave;
    g_attack_panel_selected_unit_data.faith = unit->faith;
    g_attack_panel_selected_unit_data.zodiac = unit->birthday.zodiac;
    if (unit->initial_team_flags & BATTLE_TEAM_MASK) {
        gauges->team_kind = 1;
    }
    if (!(unit->initial_team_flags & BATTLE_TEAM_OR_PLAYER_CONTROL_MASK)) {
        gauges->team_kind = 2;
    }
    g_attack_panel_selected_billboard.list_index = 0;
    g_attack_panel_selected_billboard.unit_count = 0;
    g_attack_panel_selected_billboard.unit_index = party_index + 0x200;
    g_attack_panel_selected_billboard.level = unit->level;
    g_attack_panel_selected_billboard.experience = unit->experience;
    maximum_hp = unit->max_hp;
    g_attack_panel_selected_billboard.gauges[MENU_UNIT_STATUS_GAUGE_HP].max = maximum_hp;
    if (maximum_hp == 0) {
        g_attack_panel_selected_billboard.gauges[MENU_UNIT_STATUS_GAUGE_HP].max = maximum_hp + 1;
    }
    g_attack_panel_selected_billboard.gauges[MENU_UNIT_STATUS_GAUGE_HP].value
        = g_attack_panel_selected_billboard.gauges[MENU_UNIT_STATUS_GAUGE_HP].max;
    maximum_mp = unit->max_mp;
    g_attack_panel_selected_billboard.gauges[MENU_UNIT_STATUS_GAUGE_MP].max = maximum_mp;
    if (maximum_mp == 0) {
        g_attack_panel_selected_billboard.gauges[MENU_UNIT_STATUS_GAUGE_MP].max = maximum_mp + 1;
    }
    g_attack_panel_selected_billboard.gauges[MENU_UNIT_STATUS_GAUGE_MP].value
        = g_attack_panel_selected_billboard.gauges[MENU_UNIT_STATUS_GAUGE_MP].max;
    g_attack_panel_selected_billboard.gauges[MENU_UNIT_STATUS_GAUGE_CT].max = UNIT_TURN_CT_THRESHOLD;
    g_attack_panel_selected_billboard.gauges[MENU_UNIT_STATUS_GAUGE_CT].value = UNIT_TURN_CT_THRESHOLD;

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
        if (editor->equipment[i] == 0 || editor->equipment[i] == 0xff)
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
        editor->ability_ids[3] = 0;
    } else {
        field_value = unit->primary_skillset;
        editor->ability_ids[0] = field_value;
        if (field_value == 0)
            g_attack_editor_primary_skillset_name[0] = 0xfe;
        field_value = unit->secondary_skillset;
        editor->ability_ids[1] = field_value;
        if (field_value == 0)
            g_attack_editor_secondary_skillset_name[1] = 0xfe;
        editor->ability_ids[2] = unit->abilities.reaction;
        editor->ability_ids[3] = unit->abilities.support;
        editor->ability_ids[4] = unit->abilities.movement;
    }

    item_id = 0;
    editor->two_hands = 0;
    if (editor->equipment[0] != -1) {
        if (editor->equipment[1] == -1)
            item_id = editor->equipment[0];
    } else if (editor->equipment[1] != -1) {
        item_id = editor->equipment[1];
    }

    if (item_id != 0) {
        item = main_item_get_data_pointer(item_id);
        if (item->type_flags & ITEM_TYPE_FLAG_WEAPON) {
            item_flags = g_main_item_weapon_data[item_id].flags;
            if ((item_flags & WEAPON_FLAG_FORCED_TWO_HANDS)
                || ((unit->ability_flags.support[2] & BATTLE_SUPPORT_SET_3_TWO_HANDS)
                    && (item_flags & WEAPON_FLAG_TWO_HANDS_COMPATIBLE)))
                editor->two_hands = 1;
        }
    }
}
