#include "fft/battle.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "fft/unit.h"

/*
 * Restore the saved unit state and replay status changes made during an event.
 *
 * The status masks track additions and removals relative to the snapshot.
 * Each changed status is applied through the main-thread dispatcher before
 * the unit graphics are refreshed. State 2 prevents applying the snapshot twice.
 */
void unit_apply_staged_status_data(s32 battle_id, s32 runtime_id) {
    unit_stats_t* unit;
    s32 i;
    s32 byte_index;
    s32 mask;
    u8 inflict_status_mode;
    if (!(script_get_current_scenario_finish_operation() & 0x8000)
        && g_unit_status_staging_data->state[battle_id] == UNIT_STATUS_STAGING_SAVED) {
        unit = battle_unit_get_stats_from_battle_id(battle_id);
        for (i = 0; i < STATUS_BYTE_COUNT; i++) {
            g_unit_status_staging_data->added[battle_id][i]
                = unit->status_sets.current[i] ^ g_unit_status_staging_data->current[battle_id][i];
            g_unit_status_staging_data->removed[battle_id][i]
                |= g_unit_status_staging_data->added[battle_id][i] & g_unit_status_staging_data->current[battle_id][i];
            g_unit_status_staging_data->added[battle_id][i] &= unit->status_sets.current[i];
        }
        for (i = 0; i < STATUS_BYTE_COUNT; i++) {
            unit->status_sets.innate[i] = g_unit_status_staging_data->innate[battle_id][i];
            unit->inflicted_status[i] = g_unit_status_staging_data->inflicted[battle_id][i];
            unit->status_sets.current[i] = g_unit_status_staging_data->current[battle_id][i];
        }
        for (i = 0; i < BATTLE_TIMED_STATUS_COUNT; i++) {
            unit->status_ct[i] = g_unit_status_staging_data->status_ct[battle_id][i];
        }
        unit->hp = g_unit_status_staging_data->hp[battle_id];
        unit->team_flags = g_unit_status_staging_data->team_flags[battle_id];
        unit->death_counter = g_unit_status_staging_data->death_counter[battle_id];
        /* WORLD's copy targets this BATTLE address too (see QUIRKS.md). */
        g_thread_call_target = (void (*)(void))status_queue_runtime_graphics_flag_change;
        for (i = 0; i < STATUS_COUNT; i++) {
            byte_index = i / 8;
            mask = 0x80 >> (i % 8);
            if (g_unit_status_staging_data->added[battle_id][byte_index] & mask) {
                thread_call_on_main_stack(STATUS_HANDLER_INDEX(i), 0, battle_id);
            }
            if (g_unit_status_staging_data->removed[battle_id][byte_index] & mask) {
                thread_call_on_main_stack(STATUS_HANDLER_INDEX(i), 1, battle_id);
            }
        }
        if (g_unit_status_staging_data->removed[battle_id][STATUS_BYTE_INDEX(STATUS_ID_JUMP)]
            & STATUS_BYTE_MASK(STATUS_ID_JUMP)) {
            g_unit_status_staging_data->jump_float_flags |= UNIT_STATUS_STAGING_JUMP_REMOVED;
        }
        if (g_unit_status_staging_data->removed[battle_id][STATUS_BYTE_INDEX(STATUS_ID_FLOAT)]
            & STATUS_BYTE_MASK(STATUS_ID_FLOAT)) {
            g_unit_status_staging_data->jump_float_flags |= UNIT_STATUS_STAGING_FLOAT_REMOVED;
        }
        inflict_status_mode = g_unit_status_staging_data->inflict_status_mode[battle_id];
        if (inflict_status_mode != UNIT_INFLICT_STATUS_MODE_NONE) {
            if (inflict_status_mode == UNIT_INFLICT_STATUS_MODE_POISON) {
                unit->action.status_changes.infliction[STATUS_BYTE_INDEX(STATUS_ID_POISON)]
                    |= STATUS_BYTE_MASK(STATUS_ID_POISON);
            } else if (inflict_status_mode == UNIT_INFLICT_STATUS_MODE_CRYSTAL) {
                unit->action.status_changes.infliction[STATUS_BYTE_INDEX(STATUS_ID_CRYSTAL)]
                    |= STATUS_BYTE_MASK(STATUS_ID_CRYSTAL);
            } else if (inflict_status_mode == UNIT_INFLICT_STATUS_MODE_REVIVE) {
                unit->hp = 1;
                unit->action.status_changes.infliction[STATUS_BYTE_INDEX(STATUS_ID_CRITICAL)]
                    |= STATUS_BYTE_MASK(STATUS_ID_CRITICAL);
                unit->action.status_changes.removal[STATUS_BYTE_INDEX(STATUS_ID_DEAD)]
                    |= STATUS_BYTE_MASK(STATUS_ID_DEAD);
            }
            g_thread_call_target = (void (*)(void))status_resolve_unit_changes;
            thread_call_on_main_stack(battle_id, 1);
        }
        if (!(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CHARGING)]
                & (STATUS_BYTE_MASK(STATUS_ID_CHARGING) | STATUS_BYTE_MASK(STATUS_ID_JUMP)
                    | STATUS_BYTE_MASK(STATUS_ID_PERFORMING)))) {
            unit->charged_ability_ct = 0xff;
        } else {
            unit->charged_ability_ct = g_unit_status_staging_data->charged_ability_ct[battle_id];
        }
        g_unit_status_staging_data->state[battle_id] = UNIT_STATUS_STAGING_RESTORED;
        g_thread_call_target = (void (*)(void))battle_unit_update_graphics_by_runtime_id;
        thread_call_on_main_stack(runtime_id);
    }
}
