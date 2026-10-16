/* A no-op and the search for fieldable units. */

#include "fft/battle_unit.h"
#include "fft/event_attack.h"
#include "fft/main_unit.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/text.h"
#include "fft/unit.h"
#include "psx/types.h"

void attack_noop_801c3070(void) {
}

#define ATTACK_NAME_BYTES 16
#define ATTACK_NAME_END   0xfe

/*
 * Prepare the deployment screen's roster: bind its text sections, flag each
 * of the roster slots that may be fielded and write the party name list.
 *
 * A slot is fieldable when it is occupied, is not a guest slot, is not away
 * on a proposition and is not an egg; every other slot gets an empty name.
 * A name with no terminator within its 16 bytes gets one appended.
 *
 * Outside scenario events the squad is then chosen: normally from the
 * primary-squad script variable, whose unit limit is published. With the
 * world debug battle style set, the first squad record for the current map
 * is used instead (squad 0 when it has no valid tiles), two squads are
 * announced and the record's unit limit is overwritten with 4 after being
 * published.
 */
void attack_deploy_find_fieldable_units(void) {
    s32 index;
    s32 name_index;
    u8* name_cursor;
    party_unit_t* party_unit;

    name_cursor = g_text_section_pointers[TEXT_SECTION_PARTY_UNIT_NAMES];
    g_text_section_pointers[TEXT_SECTION_ROSTER_UNIT_NAMES] = g_text_data + g_attack_text_roster_unit_names_offset;
    g_text_section_pointers[TEXT_SECTION_JOB_NAMES] = g_text_data + g_attack_text_job_names_offset;
    g_text_section_pointers[TEXT_SECTION_SKILLSET_NAMES_COPY] = g_text_section_pointers[TEXT_SECTION_SKILLSET_NAMES];
    g_attack_deploy_fieldable_unit_count = 0;

    for (index = 0; index < PARTY_ROSTER_SLOT_COUNT; index++) {
        party_unit = main_party_get_unit(index);
        if (party_unit->party_id == PARTY_ID_NONE || index >= PARTY_GUEST_SLOT_FIRST
            || party_unit->proposition_status != 0 || (party_unit->gender_flags & UNIT_FLAG_EGG) != 0) {
            g_attack_deploy_unit_fieldable[index] = 0;
            *name_cursor++ = ATTACK_NAME_END;
        } else {
            g_attack_deploy_unit_fieldable[index] = 1;
            g_attack_deploy_fieldable_unit_count++;
            for (name_index = 0; name_index < ATTACK_NAME_BYTES; name_index++) {
                *name_cursor = party_unit->name[name_index];
                if ((*name_cursor & ATTACK_NAME_END) == ATTACK_NAME_END) {
                    name_cursor++;
                    break;
                }
                name_cursor++;
                if (name_index == ATTACK_NAME_BYTES - 1) {
                    *name_cursor++ = ATTACK_NAME_END;
                }
            }
        }
    }

    if (g_script_scenario_event_active != 0) {
        return;
    }

    if (g_attack_deploy_squad_data[g_attack_deploy_current_squad_id].valid_tile_bitmap == 0) {
        g_attack_deploy_current_squad_id = 0;
    }

    if (script_get_variable(EVENT_SCRIPT_VAR_WORLD_DEBUG_BATTLE_STYLE) != 0) {
        s32 map_id = script_get_variable(EVENT_SCRIPT_VAR_CURRENT_MAP);

        g_attack_deploy_current_squad_id = 0;
        for (index = 0; index < 0x300; index++) {
            if (g_attack_deploy_squad_data[index].map_id == map_id) {
                g_attack_deploy_current_squad_id = index;
                break;
            }
        }
        if (g_attack_deploy_squad_data[g_attack_deploy_current_squad_id].valid_tile_bitmap == 0) {
            g_attack_deploy_current_squad_id = 0;
        }
        script_set_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_SQUAD_COUNT, 2);
        script_set_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_PRIMARY_SQUAD_ID, g_attack_deploy_current_squad_id);
        if (g_attack_deploy_current_squad_id == 0) {
            script_set_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_SECONDARY_SQUAD_ID, 0);
        } else {
            script_set_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_SECONDARY_SQUAD_ID, g_attack_deploy_current_squad_id + 1);
        }
        script_set_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_UNIT_LIMIT,
            g_attack_deploy_squad_data[g_attack_deploy_current_squad_id].unit_limit);
        g_attack_deploy_squad_data[g_attack_deploy_current_squad_id].unit_limit = 4;
    } else {
        g_battle_entd_selection_mode = BATTLE_ENTD_SELECTION_SCRIPT_VARIABLE;
        g_attack_deploy_current_squad_id = script_get_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_PRIMARY_SQUAD_ID);
        script_set_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_UNIT_LIMIT,
            g_attack_deploy_squad_data[g_attack_deploy_current_squad_id].unit_limit);
    }
}
