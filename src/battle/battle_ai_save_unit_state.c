#include "fft/battle_ai.h"
#include "fft/unit_slots.h"

/*
 * Save baseline unit data, or suspend simulated status and restore baseline status.
 *
 * Mode 0 saves selected fields, action records, and team Golem values. Any
 * nonzero mode saves only the compact status record before restoring the
 * baseline identity, death counter, current status, and Death Sentence CT.
 */
void battle_ai_save_unit_state(s32 mode) {
    battle_ai_data_t* ai = &g_battle_ai_data_base;
    battle_ai_status_snapshot_t* status;
    battle_ai_extended_snapshot_t* snapshot;
    battle_stats_t* unit;
    s32 i;
    s32 id;

    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        id = ai->unit_battle_ids[i];
        if (id == 0xff) {
            continue;
        }
        unit = &g_battle_unit_stats[i];
        if (mode != 0) {
            status = (battle_ai_status_snapshot_t*)ai->unit_status_records[id];
        } else {
            status = &g_battle_ai_unit_snapshots[id].status;
        }
        status->entd_slot = unit->entd_slot;
        status->death_counter = unit->death_counter;
        battle_ai_transfer_byte_values(status->current_status, unit->status_sets.current, 5);
        if (mode != 0) {
            status->death_sentence_ct = unit->status_ct[BATTLE_STATUS_CT_INDEX(BATTLE_STATUS_ID_DEATH_SENTENCE)];
            snapshot = &g_battle_ai_unit_snapshots[id];
            unit->entd_slot = snapshot->status.entd_slot;
            unit->death_counter = snapshot->status.death_counter;
            unit->status_ct[BATTLE_STATUS_CT_INDEX(BATTLE_STATUS_ID_DEATH_SENTENCE)]
                = snapshot->status_ct[BATTLE_STATUS_CT_INDEX(BATTLE_STATUS_ID_DEATH_SENTENCE)];
            battle_ai_transfer_byte_values(unit->status_sets.current, snapshot->status.current_status, 5);
        } else {
            snapshot = (battle_ai_extended_snapshot_t*)status;
            snapshot->team_flags = unit->team_flags;
            battle_ai_transfer_byte_values(snapshot->status_ct, unit->status_ct, BATTLE_TIMED_STATUS_COUNT);
            battle_ai_transfer_byte_values(snapshot->equipment, unit->equipment, 7);
            snapshot->brave = unit->brave;
            battle_ai_transfer_byte_values(&snapshot->faith, &unit->faith, 4);
            snapshot->mp = unit->mp;
            battle_ai_transfer_byte_values(snapshot->base_attributes, unit->base_attributes, 3);
            battle_ai_transfer_byte_values(snapshot->attributes, unit->attributes, 4);
            battle_ai_transfer_byte_values(&snapshot->x, &unit->x, 3);
            snapshot->charged_ability_ct = unit->charged_ability_ct;
            battle_ai_transfer_byte_values(&snapshot->has_turn, &unit->has_turn, 3);
            snapshot->mount_info = unit->mount_info;
            battle_ai_transfer_byte_values(&snapshot->auto_battle_setting, &unit->auto_battle_setting, 8);
            battle_ai_transfer_halfword_values((u16*)ai->unit_action_records[id], (u16*)&unit->action_actor_id, 20);
        }
    }
    if (mode == 0) {
        battle_ai_transfer_byte_values(ai->team_golem.bytes, (u8*)g_battle_team_golem, 8);
    }
}
