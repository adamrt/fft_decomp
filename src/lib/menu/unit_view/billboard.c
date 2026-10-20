/* Field filling with terminators, unit data copies to the status billboard and selected unit loading. */

#include "fft/ability.h"
#include "fft/at_list.h"
#include "fft/item.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/skillset.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "fft/unit_panel.h"
#include "fft/unit_runtime.h"
#include "psx/types.h"

static void unit_panel_fill_billboard(unit_stats_t* unit, unit_panel_summary_t* output);

/* Fill a 16-byte text field with terminators. */
static void text_fill_field_with_terminators(u8* text) {
    u8 value = TEXT_END_WAIT_FOR_CONFIRM;
    s32 index;

    for (index = 15; index >= 0; index--) {
        *text++ = value;
    }
}

/* Fill the unit's level, team and HP/MP/CT gauges. BATTLE and DEBUGCHR also report AT-list position; WORLD counts
 * active units without an AT-list lookup, and REQUIRE leaves the list fields empty. */
static void unit_panel_fill_billboard(unit_stats_t* unit, unit_panel_summary_t* output) {
#ifndef FFT_EVENT_REQUIRE
    s32 resolved_id;
    s32 count;
    s32 i;
    s32 unit_id;
#endif
    u16 value;

    output->level = unit->level;
    output->team_kind = 0;
    if (unit->initial_team_flags & UNIT_TEAM_MASK)
        output->team_kind = 1;
    if (!(unit->initial_team_flags & UNIT_TEAM_OR_PLAYER_CONTROL_MASK))
        output->team_kind = 2;
    if (unit->auto_battle_setting != AI_AUTOBATTLE_MANUAL)
        output->team_kind = 3;
    output->experience = unit->experience;
    value = unit->max_hp;
    output->gauges[MENU_UNIT_STATUS_GAUGE_HP].max = value;
    if (value == 0)
        output->gauges[MENU_UNIT_STATUS_GAUGE_HP].max = value + 1;
    output->gauges[MENU_UNIT_STATUS_GAUGE_HP].value = unit->hp;
    output->gauges[MENU_UNIT_STATUS_GAUGE_HP].bonus = 0;
    value = unit->max_mp;
    output->gauges[MENU_UNIT_STATUS_GAUGE_MP].max = value;
    if (value == 0)
        output->gauges[MENU_UNIT_STATUS_GAUGE_MP].max = value + 1;
    output->gauges[MENU_UNIT_STATUS_GAUGE_MP].value = unit->mp;
    output->gauges[MENU_UNIT_STATUS_GAUGE_MP].bonus = 0;
    output->gauges[MENU_UNIT_STATUS_GAUGE_CT].max = UNIT_TURN_CT_THRESHOLD;
    output->gauges[MENU_UNIT_STATUS_GAUGE_CT].value = unit->ct;
#ifdef FFT_EVENT_REQUIRE
    output->unit_index = 0;
#endif
    output->list_index = 0;
    output->unit_count = 0;
#if defined(FFT_BATTLE) || defined(FFT_EVENT_DEBUGCHR)
    g_thread_call_target = (void (*)(void))at_list_find_unit;
    output->list_index = thread_call_on_main_stack(unit);
    if (unit->has_turn != 0)
        output->gauges[MENU_UNIT_STATUS_GAUGE_CT].value = UNIT_TURN_CT_THRESHOLD;
#endif
#ifndef FFT_EVENT_REQUIRE
    if (output->list_index >= 0)
        output->list_index++;
    output->unit_count = UNIT_SLOT_COUNT;
    count = 0;
    for (i = 0; i < UNIT_SLOT_COUNT; i++) {
        if (unit_has_runtime_id(i) != 0) {
            unit_id = unit_runtime_battle_id_get(i);
            unit = unit_stats_get(unit_id);
            unit = unit_entd_find_stats(unit->unit_id, &resolved_id);
            if (resolved_id >= 0 && unit_id == resolved_id && unit->unit_id != 0) {
                count++;
            }
        }
    }
    output->unit_count = count;
#endif
}

/* Fill the comparison and selected-unit billboards, stats, equipment and ability rows.
 * Empty secondary skillsets mark byte 1 of the name buffer before the names are copied. */
void unit_panel_load_selected(void) {
    unit_stats_t* unit = unit_stats_get(g_unit_panel_comparison_unit_data.battle_id);
    menu_status_panel_slot_state_t* fields = &g_unit_panel_selected_unit_fields.state;
    u16* ability_ids;
    const item_data_t* item;
    s32 i;
    s32 output_index;
    s32 item_id;
    u8 item_flags;
    u8 field_value;
    s16 list_value;

    if (unit != 0) {
        unit_panel_fill_billboard(unit, &g_unit_panel_comparison_unit_status);
    }

    unit = unit_stats_get(g_unit_panel_selected_unit.battle_id);
    if (unit == 0)
        return;
    unit_panel_fill_billboard(unit, &g_unit_panel_selected_unit_status);

    g_unit_panel_selected_unit.job_id = unit->job_id;
    g_unit_panel_selected_unit.brave = unit->brave;
    g_unit_panel_selected_unit.faith = unit->faith;
    /* The zodiac sign is the top nibble of the 16-bit birthday value. */
    g_unit_panel_selected_unit.zodiac = unit->birthday.zodiac;

    fields->move = unit->move;
    fields->speed = unit->attributes[UNIT_ATTRIBUTE_SPEED];
    fields->jump = unit->jump;
    fields->right_weapon_evade = unit->equipment_stats[UNIT_EQUIPMENT_STAT_RIGHT_WEAPON_EVADE];
    fields->left_weapon_evade = unit->equipment_stats[UNIT_EQUIPMENT_STAT_LEFT_WEAPON_EVADE];
    fields->right_weapon_power = unit->equipment_stats[UNIT_EQUIPMENT_STAT_RIGHT_WEAPON_POWER];
    fields->left_weapon_power = unit->equipment_stats[UNIT_EQUIPMENT_STAT_LEFT_WEAPON_POWER];

    fields->physical_attack = unit->attributes[UNIT_ATTRIBUTE_PHYSICAL_ATTACK];
    fields->physical_class_evade = unit->equipment_stats[UNIT_EQUIPMENT_STAT_CLASS_PHYSICAL_EVADE];
    fields->physical_shield_evade = unit->equipment_stats[UNIT_EQUIPMENT_STAT_RIGHT_SHIELD_PHYSICAL_EVADE];
    if (unit->equipment_stats[UNIT_EQUIPMENT_STAT_RIGHT_SHIELD_PHYSICAL_EVADE]
        <= unit->equipment_stats[UNIT_EQUIPMENT_STAT_LEFT_SHIELD_PHYSICAL_EVADE])
        fields->physical_shield_evade = unit->equipment_stats[UNIT_EQUIPMENT_STAT_LEFT_SHIELD_PHYSICAL_EVADE];
    fields->physical_accessory_evade = unit->equipment_stats[UNIT_EQUIPMENT_STAT_ACCESSORY_PHYSICAL_EVADE];

    fields->magical_attack = unit->attributes[UNIT_ATTRIBUTE_MAGIC_ATTACK];
    fields->magical_class_evade = 0;
    fields->magical_shield_evade = unit->equipment_stats[UNIT_EQUIPMENT_STAT_RIGHT_SHIELD_MAGIC_EVADE];
    if (unit->equipment_stats[UNIT_EQUIPMENT_STAT_RIGHT_SHIELD_MAGIC_EVADE]
        <= unit->equipment_stats[UNIT_EQUIPMENT_STAT_LEFT_SHIELD_MAGIC_EVADE])
        fields->magical_shield_evade = unit->equipment_stats[UNIT_EQUIPMENT_STAT_LEFT_SHIELD_MAGIC_EVADE];
    fields->magical_accessory_evade = unit->equipment_stats[UNIT_EQUIPMENT_STAT_ACCESSORY_MAGIC_EVADE];

    for (i = 4; i >= 0; i--)
        fields->equipment[i] = 0;

    if (!(unit->unit_flags & UNIT_FLAG_MONSTER)) {
        fields->equipment[0] = unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON] != ITEM_ID_NONE
            ? unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON]
            : unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_SHIELD];
        fields->equipment[1] = unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON] != ITEM_ID_NONE
            ? unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON]
            : unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_SHIELD];
        fields->equipment[2] = unit->equipment[UNIT_EQUIPMENT_SLOT_HEAD];
        fields->equipment[3] = unit->equipment[UNIT_EQUIPMENT_SLOT_BODY];
        fields->equipment[4] = unit->equipment[UNIT_EQUIPMENT_SLOT_ACCESSORY];
    }

    for (i = 0; i < 5; i++) {
        if (fields->equipment[i] == ITEM_ID_NOTHING || fields->equipment[i] == ITEM_ID_NONE)
            fields->equipment[i] = -1;
    }

    fields->generic_monster = 0;
    if (unit->primary_skillset >= SKILLSET_ID_MONSTER_FIRST && unit->primary_skillset < SKILLSET_ID_END)
        fields->generic_monster = 1;

    if (fields->generic_monster != 0) {
        ability_ids = ability_skillset_list_build(unit->primary_skillset, SKILLSET_ABILITY_FILTER_ACTION);
        i = 0;
        do {
            fields->ability_ids[i] = *ability_ids++;
            i++;
        } while (i < 5);

        /* The fourth ability (index 3) stays only when the monster skill check passes; the
         * check runs on the main stack. */
        g_thread_call_target = skillset_monster_count_supporters;
        if (thread_call_on_main_stack(unit) == 0)
            fields->ability_ids[3] = 0;

        output_index = 0;
        for (item_id = 0; item_id < 5; item_id++) {
            if (fields->ability_ids[item_id] != 0)
                fields->ability_ids[output_index++] = fields->ability_ids[item_id];
        }
        while (output_index < 5)
            fields->ability_ids[output_index++] = 0;
    } else {
        /* Both name marks are overwritten when the names are refilled and copied at the end. */
        field_value = unit->primary_skillset;
        fields->ability_ids[0] = field_value;
        if (field_value == 0)
            g_unit_panel_billboard_skillset_name[0] = 0xfe;
        field_value = unit->secondary_skillset;
        fields->ability_ids[1] = field_value;
        if (field_value == 0)
            g_unit_panel_billboard_secondary_skillset_name[1] = 0xfe;
        fields->ability_ids[2] = unit->abilities.reaction;
        fields->ability_ids[3] = unit->abilities.support;
        fields->ability_ids[4] = unit->abilities.movement;
    }

    /* Empty ability rows become -1, as empty equipment slots do. */
    for (i = 0; i < 5; i++) {
        list_value = fields->ability_ids[i];
        if (list_value == 0)
            fields->ability_ids[i] = list_value - 1;
    }

    /* Two hands applies only when exactly one hand holds an item. */
    item_id = ITEM_ID_NOTHING;
    fields->two_hands = 0;
    if (fields->equipment[0] != -1) {
        if (fields->equipment[1] == -1)
            item_id = fields->equipment[0];
    } else if (fields->equipment[1] != -1) {
        item_id = fields->equipment[1];
    }

    if (item_id != ITEM_ID_NOTHING) {
        item = item_data_get(item_id);
        if (item->type_flags & ITEM_TYPE_FLAG_WEAPON) {
            item_flags = g_item_weapon_data[item_id].flags;
            if ((item_flags & WEAPON_FLAG_FORCED_TWO_HANDS)
                || ((unit->ability_flags.support[2] & UNIT_SUPPORT_SET_3_TWO_HANDS)
                    && (item_flags & WEAPON_FLAG_TWO_HANDS_COMPATIBLE)))
                fields->two_hands = 1;
        }
    }

    text_fill_field_with_terminators(g_unit_panel_billboard_unit_name);
    text_fill_field_with_terminators(g_unit_panel_billboard_job_name);
    text_fill_field_with_terminators(g_unit_panel_billboard_skillset_name);
    text_fill_field_with_terminators(g_unit_panel_billboard_secondary_skillset_name);
    /* Each panel uses 16-byte windows at each text destination, including
     * the two eight-byte skillset names; preserve those target copy lengths. */
    mem_copy_bytes_nonempty(g_unit_panel_billboard_unit_name, unit->name, 0x10);
    mem_copy_bytes_nonempty(g_unit_panel_billboard_job_name, unit->job_name, 0x10);
    mem_copy_bytes_nonempty(g_unit_panel_billboard_skillset_name, unit->primary_skillset_name, 0x10);
    mem_copy_bytes_nonempty(g_unit_panel_billboard_secondary_skillset_name, unit->secondary_skillset_name, 0x10);
}
