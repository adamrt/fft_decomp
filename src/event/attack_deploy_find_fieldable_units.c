#include "fft/attack.h"
#include "fft/main_unit.h"
#include "fft/script_variables.h"

#define ATTACK_NAME_BYTES       16
#define ATTACK_NAME_END         0xfe
#define PARTY_FLAG_UNDEPLOYABLE 0x04

/* Volatile views of g_battle_text_section_pointers[2], [6] and [8]: this
 * function matches only with volatile stores. */
extern u8* volatile g_battle_text_pointer_2;
extern u8* volatile g_battle_text_pointer_6;
extern u8* volatile g_battle_text_pointer_8;

void attack_deploy_find_fieldable_units(void) {
    s32 roster_index;
    s32 name_index;
    s32 name_end;
    s32 final_name_index;
    u8* name_cursor;
    u8* fieldable;
    party_data_t* party_unit;
    u8 name_byte;
    s32 map_id;
    s32 script_variable;
    s32 next_squad;
    s32 first_name_offset;
    s32 second_name_offset;
    /* Pin: the target holds this base in $a0 and `help_text` in $v1; unpinned
     * GCC swaps the two regardless of declaration or assignment order. */
    register u8* unit_names asm("$4");
    u8* help_text;
    attack_deployment_squad_data_t* squad;

    roster_index = 0;
    name_end = ATTACK_NAME_END;
    final_name_index = ATTACK_NAME_BYTES - 1;
    name_cursor = g_battle_text_pointer_11;
    first_name_offset = g_attack_text_section_offset_1;
    unit_names = g_attack_text_data;
    help_text = g_battle_text_pointer_22;
    g_battle_text_pointer_8 = unit_names + first_name_offset;
    second_name_offset = g_attack_text_section_offset_2;
    g_attack_deploy_fieldable_unit_count = 0;
    g_battle_text_pointer_2 = help_text;
    g_battle_text_pointer_6 = unit_names + second_name_offset;
    fieldable = g_attack_deploy_unit_fieldable;

    do {
        party_unit = main_party_get_data_pointer(roster_index);
        if (party_unit->party_id == PARTY_ID_NONE || roster_index >= PARTY_GUEST_SLOT_FIRST
            || party_unit->proposition_status != 0 || (party_unit->gender_flags & PARTY_FLAG_UNDEPLOYABLE) != 0) {
            *fieldable = 0;
            *name_cursor = name_end;
            name_cursor++;
        } else {
            *fieldable = 1;
            g_attack_deploy_fieldable_unit_count++;
            for (name_index = 0; name_index < ATTACK_NAME_BYTES; name_index++) {
                name_byte = party_unit->name[name_index];
                *name_cursor = name_byte;
                if ((name_byte & ATTACK_NAME_END) == name_end) {
                    name_cursor++;
                    break;
                }
                name_cursor++;
                if (name_index == final_name_index) {
                    *name_cursor = name_end;
                    name_cursor++;
                }
            }
        }
        roster_index++;
        fieldable++;
    } while (roster_index < PARTY_ROSTER_SLOT_COUNT);

    if (g_battle_scenario_event_active != 0) {
        return;
    }

    if (g_attack_deploy_squad_data[g_attack_deploy_current_squad_id].valid_tile_bitmap == 0) {
        g_attack_deploy_current_squad_id = 0;
    }

    if (battle_script_get_variable(EVENT_SCRIPT_VAR_WORLD_DEBUG_BATTLE_STYLE) != 0) {
        map_id = battle_script_get_variable(EVENT_SCRIPT_VAR_CURRENT_MAP);
        squad = g_attack_deploy_squad_data;
        roster_index = 0;
        g_attack_deploy_current_squad_id = 0;
        /* The target places the match handler out of line between the two
           secondary-squad arms, so the search exits and rejoins by label. */
        do {
            if (squad->map_id == map_id) {
                goto found_squad;
            }
            roster_index++;
            squad++;
        } while (roster_index < 0x300);

    squad_selected:
        if (g_attack_deploy_squad_data[g_attack_deploy_current_squad_id].valid_tile_bitmap == 0) {
            g_attack_deploy_current_squad_id = 0;
        }
        battle_script_set_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_SQUAD_COUNT, 2);
        battle_script_set_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_PRIMARY_SQUAD_ID, g_attack_deploy_current_squad_id);
        next_squad = g_attack_deploy_current_squad_id;
        if (next_squad != 0) {
            next_squad++;
            goto set_nonzero_next_squad;
        }
        next_squad++;
        script_variable = EVENT_SCRIPT_VAR_DEPLOYMENT_SECONDARY_SQUAD_ID;
        next_squad = 0;
        goto set_next_squad;
    found_squad:
        g_attack_deploy_current_squad_id = roster_index;
        goto squad_selected;
    set_nonzero_next_squad:
        script_variable = EVENT_SCRIPT_VAR_DEPLOYMENT_SECONDARY_SQUAD_ID;
    set_next_squad:
        battle_script_set_variable(script_variable, next_squad);
        battle_script_set_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_UNIT_LIMIT,
            g_attack_deploy_squad_data[g_attack_deploy_current_squad_id].unit_limit);
        g_attack_deploy_squad_data[g_attack_deploy_current_squad_id].unit_limit = 4;
    } else {
        g_battle_entd_selection_mode = 0;
        g_attack_deploy_current_squad_id = battle_script_get_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_PRIMARY_SQUAD_ID);
        battle_script_set_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_UNIT_LIMIT,
            g_attack_deploy_squad_data[g_attack_deploy_current_squad_id].unit_limit);
    }
}
