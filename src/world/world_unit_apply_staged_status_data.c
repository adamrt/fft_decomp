#include "fft/battle_status.h"
#include "fft/battle_unit.h"
#include "fft/thread.h"
#include "fft/world.h"

/*
 * Restore the saved unit state and replay status changes made during an event.
 *
 * The status masks track additions and removals relative to the snapshot.
 * Each changed status is applied through the main-thread dispatcher before
 * the unit graphics are refreshed. State 2 prevents applying the snapshot twice.
 */
void world_unit_apply_staged_status_data(s32 unit_id, s32 runtime_id) {
    unit_stats_t* unit;
    s32 i;
    s32 status_id;
    s32 byte_index;
    s32 mask;
    u8 inflict_status_mode;
    if (!(script_get_current_scenario_finish_operation() & 0x8000)
        && g_world_unit_status_staging_data->state[unit_id] == UNIT_STATUS_STAGING_SAVED) {
        unit = battle_unit_get_stats_from_battle_id(unit_id);
        for (i = 0; i < STATUS_BYTE_COUNT; i++) {
            g_world_unit_status_staging_data->added[unit_id][i]
                = unit->status_sets.current[i] ^ g_world_unit_status_staging_data->current[unit_id][i];
            g_world_unit_status_staging_data->removed[unit_id][i] |= g_world_unit_status_staging_data->added[unit_id][i]
                & g_world_unit_status_staging_data->current[unit_id][i];
            g_world_unit_status_staging_data->added[unit_id][i] &= unit->status_sets.current[i];
        }
        for (i = 0; i < STATUS_BYTE_COUNT; i++) {
            unit->status_sets.innate[i] = g_world_unit_status_staging_data->innate[unit_id][i];
            unit->inflicted_status[i] = g_world_unit_status_staging_data->inflicted[unit_id][i];
            unit->status_sets.current[i] = g_world_unit_status_staging_data->current[unit_id][i];
        }
        for (i = 0; i < BATTLE_TIMED_STATUS_COUNT; i++) {
            unit->status_ct[i] = g_world_unit_status_staging_data->status_ct[unit_id][i];
        }
        unit->hp = g_world_unit_status_staging_data->hp[unit_id];
        unit->team_flags = g_world_unit_status_staging_data->team_flags[unit_id];
        unit->death_counter = g_world_unit_status_staging_data->death_counter[unit_id];
        g_thread_call_target = (void (*)(void))status_queue_runtime_graphics_flag_change;
        i = 0;
        status_id = STATUS_HANDLER_INDEX(STATUS_ID_UNNAMED_00);
        for (; i < STATUS_COUNT; i++) {
            byte_index = i / 8;
            mask = 0x80 >> (i % 8);
            if (g_world_unit_status_staging_data->added[unit_id][byte_index] & mask) {
                thread_call_on_main_stack(status_id, 0, unit_id);
            }
            if (g_world_unit_status_staging_data->removed[unit_id][byte_index] & mask) {
                thread_call_on_main_stack(status_id, 1, unit_id);
            }
            status_id++;
        }
        if (g_world_unit_status_staging_data->removed[unit_id][STATUS_BYTE_INDEX(STATUS_ID_JUMP)]
            & STATUS_BYTE_MASK(STATUS_ID_JUMP)) {
            g_world_unit_status_staging_data->jump_float_flags |= UNIT_STATUS_STAGING_JUMP_REMOVED;
        }
        if (g_world_unit_status_staging_data->removed[unit_id][STATUS_BYTE_INDEX(STATUS_ID_FLOAT)]
            & STATUS_BYTE_MASK(STATUS_ID_FLOAT)) {
            g_world_unit_status_staging_data->jump_float_flags |= UNIT_STATUS_STAGING_FLOAT_REMOVED;
        }
        inflict_status_mode = g_world_unit_status_staging_data->inflict_status_mode[unit_id];
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
            g_thread_call_target = resolve_unit_status_changes;
            thread_call_on_main_stack(unit_id, 1);
        }
        if (!(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CHARGING)]
                & (STATUS_BYTE_MASK(STATUS_ID_CHARGING) | STATUS_BYTE_MASK(STATUS_ID_JUMP)
                    | STATUS_BYTE_MASK(STATUS_ID_PERFORMING)))) {
            unit->charged_ability_ct = 0xff;
        } else {
            unit->charged_ability_ct = g_world_unit_status_staging_data->charged_ability_ct[unit_id];
        }
        g_world_unit_status_staging_data->state[unit_id] = UNIT_STATUS_STAGING_RESTORED;
        g_thread_call_target = (void (*)(void))battle_unit_update_graphics_by_runtime_id;
        thread_call_on_main_stack(runtime_id);
    }
}
