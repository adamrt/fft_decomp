#include "fft/battle.h"
#include "fft/event.h"
#include "fft/status.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

/* Inflict Status event command thread.
 *
 * Parameters: ENTD unit id halfword, mode byte, then a wait-frame halfword.
 * Mode 0 revives a dead unit at 1 HP with Critical (or refreshes its mounted
 * animation when it is neither dead nor critical). Mode 1 clears Dead/Critical
 * and inflicts Crystal. Mode 2 inflicts Poison with animation 0x16. Each mode
 * records the matching staged exit mode for the later status-application pass. */
void world_script_inflict_status_thread(void) {
    u8* parameters;
    s32 unit_id;
    s32 mode;
    s32 wait_frames;
    s32 battle_unit_index;
    s32 misc_id;
    battle_stats_t* unit;
    s32 state;

    world_thread_set_current_task_id(NATIVE_THREAD_TASK_INFLICT_STATUS);
    parameters = world_thread_get_current_parameter_1();
    unit_id = world_script_load_halfword(parameters);
    mode = parameters[2];
    wait_frames = world_script_load_halfword(parameters + 3);
    battle_unit_index = world_unit_get_battle_index_by_entd_id(unit_id);
    misc_id = world_get_misc_id(unit_id);
    if (misc_id == EVENT_MISC_ID_NONE) {
        world_thread_exit_current();
    }
    world_unit_update_staged_status_data(unit_id, 0);
    unit = battle_unit_get_stats_from_battle_id(battle_unit_index);
    if (unit->status_sets.current[0] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_DEAD)) {
        state = 0;
    } else {
        state = 2;
        if (unit->status_sets.current[2] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_CRITICAL)) {
            state = 1;
        }
    }
    if (mode == 2) {
        world_unit_inflict_status_by_entd_id(unit_id, BATTLE_STATUS_LSB_INDEX(BATTLE_STATUS_ID_POISON), 1, 1);
        g_world_unit_status_staging_data->exit_mode[battle_unit_index] = 3;
        g_world_thread_call_target = (void (*)(void))battle_unit_update_graphics_by_misc_id_wrapper;
        world_thread_call_on_main_stack(misc_id);
        world_unit_set_animation(misc_id, 0x16);
    } else if (mode == 1) {
        world_unit_inflict_status_by_entd_id(unit_id, BATTLE_STATUS_LSB_INDEX(BATTLE_STATUS_ID_DEAD), 0, 1);
        world_unit_inflict_status_by_entd_id(unit_id, BATTLE_STATUS_LSB_INDEX(BATTLE_STATUS_ID_CRITICAL), 0, 1);
        world_unit_inflict_status_by_entd_id(unit_id, BATTLE_STATUS_LSB_INDEX(BATTLE_STATUS_ID_CRYSTAL), 1, 1);
        g_world_unit_status_staging_data->exit_mode[battle_unit_index] = 2;
        g_world_thread_call_target = (void (*)(void))battle_unit_update_graphics_by_misc_id_wrapper;
        world_thread_call_on_main_stack(misc_id);
    } else if (mode == 0) {
        if (state == 0) {
            g_world_unit_status_staging_data->exit_mode[battle_unit_index] = 1;
            world_unit_inflict_status_by_entd_id(unit_id, BATTLE_STATUS_LSB_INDEX(BATTLE_STATUS_ID_DEAD), 0, 1);
            unit->hp = 1;
            world_unit_inflict_status_by_entd_id(unit_id, BATTLE_STATUS_LSB_INDEX(BATTLE_STATUS_ID_CRITICAL), 1, 1);
            g_world_thread_call_target = (void (*)(void))battle_unit_update_graphics_by_misc_id_wrapper;
            world_thread_call_on_main_stack(misc_id);
            g_world_sound_effect_id_to_play = 0x41;
        } else if (state == 2) {
            g_world_thread_call_target = (void (*)(void))battle_unit_set_mounted_animation_by_misc_id;
            world_thread_call_on_main_stack(misc_id);
        }
    }
    if (wait_frames != 0) {
        world_thread_wait_frames(wait_frames);
    }
    world_thread_exit_current();
}
