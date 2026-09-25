#include "fft/battle.h"

/*
 * Restore the saved unit state and replay status changes made during an event.
 *
 * The status masks track additions and removals relative to the snapshot.
 * Each changed status is applied through the main-thread dispatcher before
 * the unit graphics are refreshed. State 2 prevents applying the snapshot twice.
 */
void battle_unit_apply_staged_status_data(s32 unit_id, s32 misc_id) {
    /* Pin: unpinned, GCC swaps unit ($s4) and unit_id ($s5) throughout. */
    register battle_stats_t* unit __asm__("$20");
    s32 i;
    s32 status_id;
    s32 byte_index;
    s32 mask;
    u8 exit_mode;
    if (!(battle_script_get_current_scenario_finish_operation() & 0x8000)
        && g_battle_unit_status_staging_data->state[unit_id] == 1) {
        unit = battle_unit_get_stats_from_battle_id(unit_id);
        for (i = 0; i < BATTLE_STATUS_BYTE_COUNT; i++) {
            g_battle_unit_status_staging_data->added[unit_id][i]
                = unit->status_sets.current[i] ^ g_battle_unit_status_staging_data->current[unit_id][i];
            g_battle_unit_status_staging_data->removed[unit_id][i]
                |= g_battle_unit_status_staging_data->added[unit_id][i]
                & g_battle_unit_status_staging_data->current[unit_id][i];
            g_battle_unit_status_staging_data->added[unit_id][i] &= unit->status_sets.current[i];
        }
        for (i = 0; i < BATTLE_STATUS_BYTE_COUNT; i++) {
            unit->status_sets.innate[i] = g_battle_unit_status_staging_data->innate[unit_id][i];
            unit->inflicted_status[i] = g_battle_unit_status_staging_data->inflicted[unit_id][i];
            unit->status_sets.current[i] = g_battle_unit_status_staging_data->current[unit_id][i];
        }
        for (i = 0; i < BATTLE_TIMED_STATUS_COUNT; i++) {
            unit->status_ct[i] = g_battle_unit_status_staging_data->status_ct[unit_id][i];
        }
        unit->hp = g_battle_unit_status_staging_data->hp[unit_id];
        unit->team_flags = g_battle_unit_status_staging_data->team_flags[unit_id];
        unit->death_counter = g_battle_unit_status_staging_data->death_counter[unit_id];
        g_battle_thread_call_target = (void (*)(void))battle_status_queue_misc_graphics_flag_change;
        i = 0;
        status_id = BATTLE_STATUS_HANDLER_INDEX(BATTLE_STATUS_ID_UNNAMED_00);
        for (; i < BATTLE_STATUS_COUNT; i++) {
            byte_index = i / 8;
            mask = 0x80 >> (i % 8);
            if (g_battle_unit_status_staging_data->added[unit_id][byte_index] & mask) {
                battle_thread_call_on_main_stack(status_id, 0, unit_id);
            }
            if (g_battle_unit_status_staging_data->removed[unit_id][byte_index] & mask) {
                battle_thread_call_on_main_stack(status_id, 1, unit_id);
            }
            status_id++;
        }
        if (g_battle_unit_status_staging_data->removed[unit_id][BATTLE_STATUS_BYTE_INDEX(BATTLE_STATUS_ID_JUMP)]
            & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_JUMP)) {
            g_battle_unit_status_staging_data->flags |= 1;
        }
        if (g_battle_unit_status_staging_data->removed[unit_id][BATTLE_STATUS_BYTE_INDEX(BATTLE_STATUS_ID_FLOAT)]
            & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_FLOAT)) {
            g_battle_unit_status_staging_data->flags |= 2;
        }
        exit_mode = g_battle_unit_status_staging_data->exit_mode[unit_id];
        if (exit_mode != 0) {
            if (exit_mode == 3) {
                unit->action.status_infliction[BATTLE_STATUS_BYTE_INDEX(BATTLE_STATUS_ID_POISON)]
                    |= BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_POISON);
            } else if (exit_mode == 2) {
                unit->action.status_infliction[BATTLE_STATUS_BYTE_INDEX(BATTLE_STATUS_ID_CRYSTAL)]
                    |= BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_CRYSTAL);
            } else if (exit_mode == 1) {
                unit->hp = 1;
                unit->action.status_infliction[BATTLE_STATUS_BYTE_INDEX(BATTLE_STATUS_ID_CRITICAL)]
                    |= BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_CRITICAL);
                unit->action.status_removal[BATTLE_STATUS_BYTE_INDEX(BATTLE_STATUS_ID_DEAD)]
                    |= BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_DEAD);
            }
            g_battle_thread_call_target = (void (*)(void))battle_status_resolve_unit_changes;
            battle_thread_call_on_main_stack(unit_id, 1);
        }
        {
            u8 charged_ct = unit->status_sets.current[0]
                    & (BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_CHARGING)
                        | BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_JUMP)
                        | BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_PERFORMING))
                ? g_battle_unit_status_staging_data->charged_ability_ct[unit_id]
                : 0xff;
            /* Keeps one merged CT store; without it reorg duplicates the store into a jump. */
            __asm__("" : : "r"(charged_ct));
            unit->charged_ability_ct = charged_ct;
        }
        g_battle_unit_status_staging_data->state[unit_id] = 2;
        g_battle_thread_call_target = (void (*)(void))battle_unit_update_graphics_by_misc_id_wrapper;
        battle_thread_call_on_main_stack(misc_id);
    }
}
