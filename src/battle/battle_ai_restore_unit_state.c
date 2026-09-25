#include "fft/battle.h"

/*
 * Restore baseline unit data or suspended simulation status.
 *
 * Mode 0 restores selected baseline fields, action records, and team Golem
 * values. Any nonzero mode restores only the compact status record.
 */
void battle_ai_restore_unit_state(s32 mode) {
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
        unit->entd_slot = status->entd_slot;
        unit->death_counter = status->death_counter;
        battle_ai_transfer_byte_values(unit->status_sets.current, status->current_status, 5);
        if (mode != 0) {
            unit->status_ct[BATTLE_STATUS_CT_INDEX(BATTLE_STATUS_ID_DEATH_SENTENCE)] = status->death_sentence_ct;
        } else {
            snapshot = (battle_ai_extended_snapshot_t*)status;
            unit->team_flags = snapshot->team_flags;
            battle_ai_transfer_byte_values(unit->status_ct, snapshot->status_ct, BATTLE_TIMED_STATUS_COUNT);
            battle_ai_transfer_byte_values(unit->equipment, snapshot->equipment, 7);
            unit->brave = snapshot->brave;
            battle_ai_transfer_byte_values(&unit->faith, &snapshot->faith, 4);
            unit->mp = snapshot->mp;
            battle_ai_transfer_byte_values(unit->base_attributes, snapshot->base_attributes, 3);
            battle_ai_transfer_byte_values(unit->attributes, snapshot->attributes, 4);
            battle_ai_transfer_byte_values(&unit->x, &snapshot->x, 3);
            unit->charged_ability_ct = snapshot->charged_ability_ct;
            battle_ai_transfer_byte_values(&unit->has_turn, &snapshot->has_turn, 3);
            unit->mount_info = snapshot->mount_info;
            battle_ai_transfer_byte_values(&unit->auto_battle_setting, &snapshot->auto_battle_setting, 8);
            battle_ai_transfer_halfword_values((u16*)&unit->action_actor_id, (u16*)ai->unit_action_records[id], 20);
        }
    }
    if (mode == 0) {
        battle_ai_transfer_byte_values((u8*)g_battle_team_golem, ai->team_golem.bytes, 8);
    }
}
