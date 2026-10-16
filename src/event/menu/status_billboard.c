/* Field filling with terminators, unit data copies to the status billboard and selected unit loading. */

#include "fft/battle_action.h"
#include "fft/battle_menu.h"
#include "fft/battle_unit.h"
#include "fft/battle_unit_encounter.h"
#include "fft/data_item.h"
#include "fft/data_skillset.h"
#include "fft/main_item.h"
#include "fft/main_unit.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/menu_panel.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "psx/types.h"
#ifdef FFT_EVENT_DEBUGCHR
#    include "fft/event_debugchr.h"
#endif

static void menu_copy_unit_data_to_status_billboard(unit_stats_t* unit, menu_unit_status_record_t* output);

/* Fill a 16-byte text field with terminators. */
static void text_fill_field_with_terminators(u8* text) {
    u8 value = TEXT_END_WAIT_FOR_CONFIRM;
    s32 index;

    for (index = 15; index >= 0; index--) {
        *text++ = value;
    }
}

#ifdef FFT_WORLD
/* Fills a status billboard (level, team kind, HP/MP/CT and the live-unit
 * count) from a battle unit. This is WORLD's shorter body.
 *
 * Team kind is 0 for the player's side, 1 for the enemy team, 2 for a unit on
 * neither the enemy team nor under player control, and 3 for a unit on auto
 * battle. A zero maximum HP or MP is stored as 1. list_index is always 1
 * here; unit_count counts the runtime ids whose battle unit is the first holder
 * of a nonzero ENTD unit id. */
static void menu_copy_unit_data_to_status_billboard(unit_stats_t* unit, menu_unit_status_record_t* output) {
    s32 resolved_id;
    s32 count;
    s32 i;
    s32 unit_id;
    u16 value;
    u16 mp;
    u8 ct;

    output->level = unit->level;
    output->team_kind = 0;
    if (unit->initial_team_flags & BATTLE_TEAM_MASK)
        output->team_kind = 1;
    if (!(unit->initial_team_flags & BATTLE_TEAM_OR_PLAYER_CONTROL_MASK))
        output->team_kind = 2;
    if (unit->auto_battle_setting != AI_AUTOBATTLE_MANUAL)
        output->team_kind = 3;
    output->experience = unit->experience;
    value = unit->max_hp;
    output->gauges[MENU_UNIT_STATUS_GAUGE_HP].max = value;
    if (value == 0) {
        value++;
        output->gauges[MENU_UNIT_STATUS_GAUGE_HP].max = value;
    }
    output->gauges[MENU_UNIT_STATUS_GAUGE_HP].value = unit->hp;
    output->gauges[MENU_UNIT_STATUS_GAUGE_HP].bonus = 0;
    value = unit->max_mp;
    output->gauges[MENU_UNIT_STATUS_GAUGE_MP].max = value;
    if (value == 0) {
        value++;
        output->gauges[MENU_UNIT_STATUS_GAUGE_MP].max = value;
    }
    mp = unit->mp;
    output->gauges[MENU_UNIT_STATUS_GAUGE_MP].bonus = 0;
    output->gauges[MENU_UNIT_STATUS_GAUGE_CT].max = UNIT_TURN_CT_THRESHOLD;
    output->gauges[MENU_UNIT_STATUS_GAUGE_MP].value = mp;
    ct = unit->ct;
    output->list_index = 0;
    output->unit_count = 0;
    output->gauges[MENU_UNIT_STATUS_GAUGE_CT].value = ct;
    if (output->list_index >= 0)
        output->list_index++;
    output->unit_count = BATTLE_UNIT_SLOT_COUNT;
    count = 0;
    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        if (battle_unit_has_runtime_id(i) != 0) {
            unit_id = battle_unit_get_battle_id_by_runtime_id(i);
            unit = battle_unit_find_stats_by_entd_unit_id(
                battle_unit_get_stats_from_battle_id(unit_id)->unit_id, &resolved_id);
            if (resolved_id >= 0 && unit_id == resolved_id && unit->unit_id != 0) {
                count++;
            }
        }
    }
    output->unit_count = count;
}
#endif

#if defined(FFT_BATTLE) || defined(FFT_EVENT_DEBUGCHR)
/* Fill the level, team, AT-list position, HP/MP/CT and live-unit count. */
static void menu_copy_unit_data_to_status_billboard(unit_stats_t* unit, menu_unit_status_record_t* output) {
    s32 resolved_id;
    s32 count;
    s32 i;
    s32 unit_id;
    u16 value;

    output->level = unit->level;
    output->team_kind = 0;
    if (unit->initial_team_flags & BATTLE_TEAM_MASK)
        output->team_kind = 1;
    if (!(unit->initial_team_flags & BATTLE_TEAM_OR_PLAYER_CONTROL_MASK))
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
    output->list_index = 0;
    output->unit_count = 0;
    g_thread_call_target = (void (*)(void))battle_action_find_unit_turn_index;
    output->list_index = thread_call_on_main_stack(unit);
    if (unit->has_turn != 0)
        output->gauges[MENU_UNIT_STATUS_GAUGE_CT].value = UNIT_TURN_CT_THRESHOLD;
    if (output->list_index >= 0)
        output->list_index++;
    output->unit_count = BATTLE_UNIT_SLOT_COUNT;
    count = 0;
    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        if (battle_unit_has_runtime_id(i) != 0) {
            unit_id = battle_unit_get_battle_id_by_runtime_id(i);
            unit = battle_unit_get_stats_from_battle_id(unit_id);
            unit = battle_unit_find_stats_by_entd_unit_id(unit->unit_id, &resolved_id);
            if (resolved_id >= 0 && unit_id == resolved_id && unit->unit_id != 0) {
                count++;
            }
        }
    }
    output->unit_count = count;
}
#endif

#ifdef FFT_EVENT_REQUIRE
/* Fills a status billboard (level, team kind, experience and the HP/MP/CT
 * gauges) from a battle unit, leaving the unit index, list index and unit
 * count 0.
 *
 * Team kind is 0 for the player's side, 1 for the enemy team, 2 for a unit on
 * neither the enemy team nor under player control, and 3 for a unit on auto
 * battle. A zero maximum HP or MP is stored as 1. */
static void menu_copy_unit_data_to_status_billboard(unit_stats_t* unit, menu_unit_status_record_t* output) {
    u16 maximum_hp;
    u16 maximum_mp;
    u8 ct;

    output->level = unit->level;
    output->team_kind = 0;
    if (unit->initial_team_flags & BATTLE_TEAM_MASK) {
        output->team_kind = 1;
    }
    if (!(unit->initial_team_flags & BATTLE_TEAM_OR_PLAYER_CONTROL_MASK)) {
        output->team_kind = 2;
    }
    if (unit->auto_battle_setting != AI_AUTOBATTLE_MANUAL) {
        output->team_kind = 3;
    }
    output->experience = unit->experience;
    maximum_hp = unit->max_hp;
    output->gauges[MENU_UNIT_STATUS_GAUGE_HP].max = maximum_hp;
    if (maximum_hp == 0) {
        output->gauges[MENU_UNIT_STATUS_GAUGE_HP].max = maximum_hp + 1;
    }
    output->gauges[MENU_UNIT_STATUS_GAUGE_HP].value = unit->hp;
    output->gauges[MENU_UNIT_STATUS_GAUGE_HP].bonus = 0;
    maximum_mp = unit->max_mp;
    output->gauges[MENU_UNIT_STATUS_GAUGE_MP].max = maximum_mp;
    if (maximum_mp == 0) {
        output->gauges[MENU_UNIT_STATUS_GAUGE_MP].max = maximum_mp + 1;
    }
    output->gauges[MENU_UNIT_STATUS_GAUGE_MP].value = unit->mp;
    output->gauges[MENU_UNIT_STATUS_GAUGE_MP].bonus = 0;
    output->gauges[MENU_UNIT_STATUS_GAUGE_CT].max = UNIT_TURN_CT_THRESHOLD;
    ct = unit->ct;
    output->unit_index = 0;
    output->list_index = 0;
    output->unit_count = 0;
    output->gauges[MENU_UNIT_STATUS_GAUGE_CT].value = ct;
}
#endif

#if defined(FFT_WORLD) || defined(FFT_BATTLE) || defined(FFT_EVENT_REQUIRE)
/* Fill selected and comparison billboards, stats, equipment and ability names. */
void menu_load_selected_unit(void) {
    unit_stats_t* unit = battle_unit_get_stats_from_battle_id(g_menu_comparison_unit_data.battle_id);
    menu_status_panel_slot_state_t* editor = &g_menu_selected_unit_fields.state;
    u16* innate_items;
    item_data_t* item;
    s32 i;
    s32 output_index;
    s32 item_id;
    u8 item_flags;
    u8 field_value;
    s16 list_value;

    if (unit != 0) {
        menu_copy_unit_data_to_status_billboard(unit, &g_menu_comparison_unit_status);
    }

    unit = battle_unit_get_stats_from_battle_id(g_menu_selected_unit.battle_id);
    if (unit == 0)
        return;
    menu_copy_unit_data_to_status_billboard(unit, &g_menu_selected_unit_status);

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

        /* The fourth ability (index 3) stays only when the monster skill check passes; the
         * check runs on the main stack. */
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
        /* Both name marks are overwritten when the names are refilled and copied at the end. */
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
#endif

#ifdef FFT_EVENT_DEBUGCHR
/*
 * Fills the comparison and selected-unit billboards and the status panel's
 * fields for the selected battle unit. DEBUGCHR's body.
 *
 * The 16-bit fields are: 0 move, 1 speed, 2 jump, 3/4 right and left weapon
 * power, 5/6 right and left weapon evade, 7 set for a monster skillset, 8 set
 * when a lone weapon is held in two hands, 9 physical attack, 10 class evade,
 * 11/12 shield (the larger hand) and accessory physical evade, 14 magic
 * attack, 16/17 shield and accessory magic evade, 18..22 the equipped items
 * (-1 for none) and 23..27 the ability rows (-1 for none): both skillsets,
 * reaction, support and movement, or a monster's skillset abilities packed to
 * the front, the fourth kept only while battle_menu_init_monster_skill_check
 * allows it.
 *
 * An empty secondary skillset marks byte 1 of its name buffer, not byte 0,
 * before the names are cleared and copied.
 */
void menu_load_selected_unit(void) {
    unit_stats_t* unit = battle_unit_get_stats_from_battle_id(g_debugchr_panel_comparison_unit_data.battle_id);
    menu_status_panel_slot_state_t* editor = &g_menu_selected_unit_fields.state;
    u16* innate_items;
    item_data_t* item;
    s32 i;
    s32 output_index;
    s32 item_id;
    u8 item_flags;
    u8 field_value;
    s16 list_value;

    if (unit != 0) {
        menu_copy_unit_data_to_status_billboard(unit, &g_panel_comparison_billboard);
    }

    unit = battle_unit_get_stats_from_battle_id(g_debugchr_panel_selected_unit_data.battle_id);
    if (unit == 0)
        return;
    menu_copy_unit_data_to_status_billboard(unit, &g_panel_selected_billboard);

    g_debugchr_panel_selected_unit_data.job_id = unit->job_id;
    g_debugchr_panel_selected_unit_data.brave = unit->brave;
    g_debugchr_panel_selected_unit_data.faith = unit->faith;
    /* The zodiac sign lives in the top nibble of the 16-bit value at 0x008. */
    /* Direct union-member access hoists the 16-bit load above the faith
     * store (1344 vs 1348 bytes); keep the typed field's aliased 16-bit view. */
    g_debugchr_panel_selected_unit_data.zodiac = unit->birthday.zodiac;

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

        /* The fourth ability (index 3) stays only when the monster skill check passes; the
         * check runs on the main stack. */
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
        /* Both name marks are overwritten when the names are refilled and copied at the end. */
        field_value = unit->primary_skillset;
        editor->ability_ids[0] = field_value;
        if (field_value == 0)
            g_editor_primary_skillset_name[0] = 0xfe;
        field_value = unit->secondary_skillset;
        editor->ability_ids[1] = field_value;
        if (field_value == 0)
            g_editor_secondary_skillset_name[1] = 0xfe;
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

    text_fill_field_with_terminators(g_debugchr_editor_unit_name);
    text_fill_field_with_terminators(g_debugchr_editor_job_name);
    text_fill_field_with_terminators(g_editor_primary_skillset_name);
    text_fill_field_with_terminators(g_editor_secondary_skillset_name);
    /* The editor uses 16-byte windows at each text destination, including
     * the two eight-byte skillset names; preserve those target copy lengths. */
    mem_copy_bytes_nonempty(g_debugchr_editor_unit_name, unit->name, 0x10);
    mem_copy_bytes_nonempty(g_debugchr_editor_job_name, unit->job_name, 0x10);
    mem_copy_bytes_nonempty(g_editor_primary_skillset_name, unit->primary_skillset_name, 0x10);
    mem_copy_bytes_nonempty(g_editor_secondary_skillset_name, unit->secondary_skillset_name, 0x10);
}
#endif
