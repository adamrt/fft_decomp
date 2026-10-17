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
#include "fft/unit_editor.h"
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
            unit_id = unit_get_battle_id_by_runtime_id(i);
            unit = unit_get_stats(unit_id);
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
 * DEBUGCHR uses its local billboard and name buffers; the other programs use the event-library records.
 * Empty secondary skillsets mark byte 1 of the name buffer before the names are copied. */
void unit_panel_load_selected(void) {
    unit_stats_t* unit = unit_get_stats(g_unit_panel_comparison_unit_data.battle_id);
    menu_status_panel_slot_state_t* editor = &g_unit_panel_selected_unit_fields.state;
    u16* innate_items;
    item_data_t* item;
    s32 i;
    s32 output_index;
    s32 item_id;
    u8 item_flags;
    u8 field_value;
    s16 list_value;

    if (unit != 0) {
#ifdef FFT_EVENT_DEBUGCHR
        unit_panel_fill_billboard(unit, &g_unit_panel_comparison_billboard);
#else
        unit_panel_fill_billboard(unit, &g_unit_panel_comparison_unit_status);
#endif
    }

#ifdef FFT_EVENT_DEBUGCHR
    unit = unit_get_stats(g_unit_panel_selected_unit_data.battle_id);
#else
    unit = unit_get_stats(g_unit_panel_selected_unit.battle_id);
#endif
    if (unit == 0)
        return;
#ifdef FFT_EVENT_DEBUGCHR
    unit_panel_fill_billboard(unit, &g_unit_panel_selected_billboard);
#else
    unit_panel_fill_billboard(unit, &g_unit_panel_selected_unit_status);
#endif

#ifdef FFT_EVENT_DEBUGCHR
    g_unit_panel_selected_unit_data.job_id = unit->job_id;
    g_unit_panel_selected_unit_data.brave = unit->brave;
    g_unit_panel_selected_unit_data.faith = unit->faith;
#else
    g_unit_panel_selected_unit.job_id = unit->job_id;
    g_unit_panel_selected_unit.brave = unit->brave;
    g_unit_panel_selected_unit.faith = unit->faith;
#endif
    /* The zodiac sign is the top nibble of the 16-bit birthday value. */
#ifdef FFT_EVENT_DEBUGCHR
    g_unit_panel_selected_unit_data.zodiac = unit->birthday.zodiac;
#else
    g_unit_panel_selected_unit.zodiac = unit->birthday.zodiac;
#endif

    editor->move = unit->move;
    editor->speed = unit->attributes[UNIT_ATTRIBUTE_SPEED];
    editor->jump = unit->jump;
    editor->right_weapon_evade = unit->equipment_stats[UNIT_EQUIPMENT_STAT_RIGHT_WEAPON_EVADE];
    editor->left_weapon_evade = unit->equipment_stats[UNIT_EQUIPMENT_STAT_LEFT_WEAPON_EVADE];
    editor->right_weapon_power = unit->equipment_stats[UNIT_EQUIPMENT_STAT_RIGHT_WEAPON_POWER];
    editor->left_weapon_power = unit->equipment_stats[UNIT_EQUIPMENT_STAT_LEFT_WEAPON_POWER];

    editor->physical_attack = unit->attributes[UNIT_ATTRIBUTE_PHYSICAL_ATTACK];
    editor->physical_class_evade = unit->equipment_stats[UNIT_EQUIPMENT_STAT_CLASS_PHYSICAL_EVADE];
    editor->physical_shield_evade = unit->equipment_stats[UNIT_EQUIPMENT_STAT_RIGHT_SHIELD_PHYSICAL_EVADE];
    if (unit->equipment_stats[UNIT_EQUIPMENT_STAT_RIGHT_SHIELD_PHYSICAL_EVADE]
        <= unit->equipment_stats[UNIT_EQUIPMENT_STAT_LEFT_SHIELD_PHYSICAL_EVADE])
        editor->physical_shield_evade = unit->equipment_stats[UNIT_EQUIPMENT_STAT_LEFT_SHIELD_PHYSICAL_EVADE];
    editor->physical_accessory_evade = unit->equipment_stats[UNIT_EQUIPMENT_STAT_ACCESSORY_PHYSICAL_EVADE];

    editor->magical_attack = unit->attributes[UNIT_ATTRIBUTE_MAGIC_ATTACK];
    editor->magical_class_evade = 0;
    editor->magical_shield_evade = unit->equipment_stats[UNIT_EQUIPMENT_STAT_RIGHT_SHIELD_MAGIC_EVADE];
    if (unit->equipment_stats[UNIT_EQUIPMENT_STAT_RIGHT_SHIELD_MAGIC_EVADE]
        <= unit->equipment_stats[UNIT_EQUIPMENT_STAT_LEFT_SHIELD_MAGIC_EVADE])
        editor->magical_shield_evade = unit->equipment_stats[UNIT_EQUIPMENT_STAT_LEFT_SHIELD_MAGIC_EVADE];
    editor->magical_accessory_evade = unit->equipment_stats[UNIT_EQUIPMENT_STAT_ACCESSORY_MAGIC_EVADE];

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
        innate_items = ability_store_skillset_abilities(unit->primary_skillset, SKILLSET_ABILITY_FILTER_ACTION);
        i = 0;
        do {
            editor->ability_ids[i] = *innate_items++;
            i++;
        } while (i < 5);

        /* The fourth ability (index 3) stays only when the monster skill check passes; the
         * check runs on the main stack. */
        g_thread_call_target = skillset_monster_count_supporters;
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
        /* Both name marks are overwritten when the names are refilled and copied at the end. */
        field_value = unit->primary_skillset;
        editor->ability_ids[0] = field_value;
        if (field_value == 0)
#ifdef FFT_EVENT_DEBUGCHR
            g_unit_panel_editor_primary_skillset_name[0] = 0xfe;
#else
            g_unit_panel_billboard_skillset_name[0] = 0xfe;
#endif
        field_value = unit->secondary_skillset;
        editor->ability_ids[1] = field_value;
        if (field_value == 0)
#ifdef FFT_EVENT_DEBUGCHR
            g_unit_panel_editor_secondary_skillset_name[1] = 0xfe;
#else
            g_unit_panel_billboard_secondary_skillset_name[1] = 0xfe;
#endif
        editor->ability_ids[2] = unit->abilities.reaction;
        editor->ability_ids[3] = unit->abilities.support;
        editor->ability_ids[4] = unit->abilities.movement;
    }

    /* Empty ability rows become -1, as empty equipment slots do. */
    for (i = 0; i < 5; i++) {
        list_value = editor->ability_ids[i];
        if (list_value == 0)
            editor->ability_ids[i] = list_value - 1;
    }

    /* Two hands applies only when exactly one hand holds an item. */
    item_id = ITEM_ID_NOTHING;
    editor->two_hands = 0;
    if (editor->equipment[0] != -1) {
        if (editor->equipment[1] == -1)
            item_id = editor->equipment[0];
    } else if (editor->equipment[1] != -1) {
        item_id = editor->equipment[1];
    }

    if (item_id != ITEM_ID_NOTHING) {
        item = item_get_data(item_id);
        if (item->type_flags & ITEM_TYPE_FLAG_WEAPON) {
            item_flags = g_item_weapon_data[item_id].flags;
            if ((item_flags & WEAPON_FLAG_FORCED_TWO_HANDS)
                || ((unit->ability_flags.support[2] & UNIT_SUPPORT_SET_3_TWO_HANDS)
                    && (item_flags & WEAPON_FLAG_TWO_HANDS_COMPATIBLE)))
                editor->two_hands = 1;
        }
    }

#ifdef FFT_EVENT_DEBUGCHR
    text_fill_field_with_terminators(g_unit_editor_unit_name);
    text_fill_field_with_terminators(g_unit_editor_job_name);
    text_fill_field_with_terminators(g_unit_panel_editor_primary_skillset_name);
    text_fill_field_with_terminators(g_unit_panel_editor_secondary_skillset_name);
#else
    text_fill_field_with_terminators(g_unit_panel_billboard_unit_name);
    text_fill_field_with_terminators(g_unit_panel_billboard_job_name);
    text_fill_field_with_terminators(g_unit_panel_billboard_skillset_name);
    text_fill_field_with_terminators(g_unit_panel_billboard_secondary_skillset_name);
#endif
    /* The editor uses 16-byte windows at each text destination, including
     * the two eight-byte skillset names; preserve those target copy lengths. */
#ifdef FFT_EVENT_DEBUGCHR
    mem_copy_bytes_nonempty(g_unit_editor_unit_name, unit->name, 0x10);
    mem_copy_bytes_nonempty(g_unit_editor_job_name, unit->job_name, 0x10);
    mem_copy_bytes_nonempty(g_unit_panel_editor_primary_skillset_name, unit->primary_skillset_name, 0x10);
    mem_copy_bytes_nonempty(g_unit_panel_editor_secondary_skillset_name, unit->secondary_skillset_name, 0x10);
#else
    mem_copy_bytes_nonempty(g_unit_panel_billboard_unit_name, unit->name, 0x10);
    mem_copy_bytes_nonempty(g_unit_panel_billboard_job_name, unit->job_name, 0x10);
    mem_copy_bytes_nonempty(g_unit_panel_billboard_skillset_name, unit->primary_skillset_name, 0x10);
    mem_copy_bytes_nonempty(g_unit_panel_billboard_secondary_skillset_name, unit->secondary_skillset_name, 0x10);
#endif
}
