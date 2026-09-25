#include "fft/data.h"
#include "fft/main_runtime.h"
#include "fft/main_unit.h"
#include "fft/require.h"
#include "psx/types.h"

/* Load a party roster unit into the REQUIRE status/editor display.
 *
 * Party counterpart of require_editor_load_selected_unit: the unit is
 * rebuilt from the roster into a scratch battle_stats_t, shown at full HP/MP
 * and CT, and without the monster-skill or name-text refresh. */
void require_load_party_unit_into_editor(s32 unused, s32 party_index) {
    battle_stats_t* unit;
    s16* editor;
    u16* innate_items;
    s16* ability_slot;
    item_data_t* item;
    s32 i;
    s32 item_id;
    u8 item_flags;
    u8 field_value;
    u16 maximum_hp;
    u16 maximum_mp;

    main_unit_init_job_data(&g_require_editor_party_unit_stats, party_index, 0);
    editor = g_require_editor_unit_fields.halfwords;
    unit = &g_require_editor_party_unit_stats;

    /* The target stores this halfword standalone; the buffer is the u8 view. */
    *(s16*)g_require_active_unit_data = party_index;
    g_require_editor_selected_unit_id = party_index;
    g_require_editor_team_state = 0;
    g_require_editor_unit_type = unit->job_id;
    g_require_editor_brave = unit->brave;
    g_require_editor_faith = unit->faith;
    g_require_editor_zodiac = *(u16*)&unit->birthday >> 12;
    if (unit->initial_team_flags & BATTLE_TEAM_MASK) {
        g_require_editor_team_state = 1;
    }
    if (!(unit->initial_team_flags & BATTLE_TEAM_OR_PLAYER_CONTROL_MASK)) {
        g_require_editor_team_state = 2;
    }
    g_require_panel_selected_billboard._04 = 0;
    g_require_panel_selected_billboard._06 = 0;
    g_require_panel_selected_billboard.unit_index = party_index + 0x100;
    g_require_panel_selected_billboard.level = unit->level;
    g_require_panel_selected_billboard.experience = unit->experience;
    maximum_hp = unit->max_hp;
    g_require_panel_selected_billboard.max_hp = maximum_hp;
    if (maximum_hp == 0) {
        g_require_panel_selected_billboard.max_hp = maximum_hp + 1;
    }
    g_require_panel_selected_billboard.hp = g_require_panel_selected_billboard.max_hp;
    maximum_mp = unit->max_mp;
    g_require_panel_selected_billboard.max_mp = maximum_mp;
    if (maximum_mp == 0) {
        g_require_panel_selected_billboard.max_mp = maximum_mp + 1;
    }
    g_require_panel_selected_billboard.mp = g_require_panel_selected_billboard.max_mp;
    g_require_panel_selected_billboard.max_ct = 100;
    g_require_panel_selected_billboard.ct = 100;

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
        if (editor[18 + i] == 0 || editor[18 + i] == 0xff)
            editor[18 + i] = -1;
    }

    editor[7] = 0;
    if ((u8)(unit->primary_skillset + 0x50) < 0x30)
        editor[7] = 1;

    if (editor[7] != 0) {
        innate_items = main_ability_store_skillset_abilities(unit->primary_skillset, 1);
        i = 0;
        ability_slot = editor;
        do {
            ability_slot[23] = *innate_items++;
            i++;
            ability_slot++;
        } while (i < 5);
        editor[26] = 0;
    } else {
        field_value = unit->primary_skillset;
        editor[23] = field_value;
        if (field_value == 0)
            g_require_editor_third_ability_bytes[0] = 0xfe;
        field_value = unit->secondary_skillset;
        editor[24] = field_value;
        if (field_value == 0)
            g_require_editor_fourth_ability_bytes[1] = 0xfe;
        editor[25] = unit->reaction_ability;
        editor[26] = unit->support_ability;
        editor[27] = unit->movement_ability;
    }

    item_id = 0;
    editor[8] = 0;
    if (editor[18] != -1) {
        if (editor[19] == -1)
            item_id = editor[18];
    } else if (editor[19] != -1) {
        item_id = editor[19];
    }

    if (item_id != 0) {
        item = main_item_get_data_pointer(item_id);
        if (item->type_flags & ITEM_TYPE_FLAG_WEAPON) {
            item_flags = g_main_item_weapon_data[item_id].flags;
            if ((item_flags & WEAPON_FLAG_FORCED_TWO_HANDS)
                || ((unit->support_abilities[2] & BATTLE_SUPPORT_SET_3_TWO_HANDS)
                    && (item_flags & WEAPON_FLAG_TWO_HANDS_COMPATIBLE)))
                editor[8] = 1;
        }
    }
}
