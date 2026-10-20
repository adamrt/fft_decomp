/* REQUIRE dispatches the requested battle-condition display or party change.
 * Recruitment also uses this unit's HELPMENU bridge and battle-unit name text. */

#include "fft/after_battle.h"
#include "fft/help.h"
#include "fft/input.h"
#include "fft/memory.h"
#include "fft/overlay.h"
#include "fft/script_instruction.h"
#include "fft/sound.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "fft/unit_panel.h"
#include "fft/unit_runtime.h"
#include "psx/types.h"

/*
 * Thread body of the REQUIRE overlay: runs one display condition.
 *
 * Parameter 1 is the condition id and parameter 2 its argument. The general
 * conditions draw their banner inline (the argument is the hold time). The
 * others each run their routine in the first idle thread slot and wait for
 * it: the special-case banner (held for the argument's frames or until
 * skipped), "ready", the battle congratulations, the gil reward, the war
 * trophies, the permanent Brave/Faith changes and the low-Brave/high-Faith
 * desertions. The post-battle unit changes run inline.
 */
void require_conditions_entrypoint(void) {
    s32 condition;
    s32 parameter;
    s32 thread;

    thread_current_task_set(THREAD_TASK_CONDITION_DISPATCH);
    condition = thread_current_parameter_1_get();
    parameter = thread_current_parameter_2_get();
    if (condition >= EVENT_DISPLAY_CONDITION_GENERAL_FIRST)
        conditions_show_general(parameter);
    thread = thread_slot_resolve(THREAD_SLOT_FIRST_IDLE);

    if (condition == EVENT_DISPLAY_CONDITION_SPECIAL_CASES) {
        thread_start(thread, conditions_special_thread);
        thread_parameters_set(thread, CONDITIONS_BANNER_READY, 0, 0);
        input_wait_frames_or_skip(parameter);
        thread_parameters_set(thread, 0, 0, 2);
        thread_inactive_wait(thread);
    } else if (condition == EVENT_DISPLAY_CONDITION_SHOW_READY_BUGGED) {
        thread_start(thread, conditions_show_ready_bugged);
        thread_parameters_set(thread, 0, 0, 0);
        thread_inactive_wait(thread);
    } else if (condition == EVENT_DISPLAY_CONDITION_BATTLE_CONGRATULATIONS) {
        thread_start(thread, victory_congratulations_thread);
        thread_parameters_set(thread, 0, 0, 0);
        thread_inactive_wait(thread);
    } else if (condition == EVENT_DISPLAY_CONDITION_GIL_REWARD) {
        thread_start(thread, war_trophy_run_gil_ticker);
        thread_parameters_set(thread, 0, 0, 0);
        thread_inactive_wait(thread);
    } else if (condition == EVENT_DISPLAY_CONDITION_WAR_TROPHIES) {
        thread_start(thread, war_trophy_list_thread);
        thread_parameters_set(thread, 0, 0, 0);
        thread_inactive_wait(thread);
    } else if (condition == EVENT_DISPLAY_CONDITION_APPLY_PERMANENT_BRAVE_FAITH_CHANGES) {
        thread_start(thread, party_brave_faith_report_thread);
        thread_parameters_set(thread, 0, 0, 0);
        thread_inactive_wait(thread);
    } else if (condition == EVENT_DISPLAY_CONDITION_REMOVE_LOW_BRAVE_HIGH_FAITH_UNITS) {
        thread_start(thread, party_departures_thread);
        thread_parameters_set(thread, 0, 0, 0);
        thread_inactive_wait(thread);
    } else if (condition == EVENT_DISPLAY_CONDITION_APPLY_POST_BATTLE_UNIT_CHANGES) {
        party_apply_post_battle_changes();
    }
    thread_current_exit();
}

/* Thread body that runs the HELPMENU overlay from the REQUIRE screen: waits
 * until 0x10000 bytes can be reserved at the high overlay address, loads the
 * overlay and runs helpmenu_after_battle_entrypoint in the first idle slot,
 * with the panel's two animation flags set while it runs. */
void help_open_after_battle(void) {
    s32 thread_id;

    thread_current_task_set(THREAD_TASK_HELP_MENU);
    g_sound_pending_sfx_id = SOUND_SFX_CONFIRM;
    g_unit_panel_status_animation[0] = 1;
    g_unit_panel_status_animation[1] = 1;
    while (heap_reserve_at(g_heap_high_overlay_load_address, 0x10000) != g_heap_high_overlay_load_address) {
        thread_yield();
    }
    overlay_companion_request(COMPANION_OVERLAY_HELPMENU);
    thread_id = thread_slot_resolve(THREAD_SLOT_FIRST_IDLE);
    thread_start(thread_id, helpmenu_after_battle_entrypoint);
    thread_inactive_wait(thread_id);
    g_unit_panel_status_animation[0] = 0;
    g_unit_panel_status_animation[1] = 0;
    thread_current_exit();
}

/* Rebuild the roster name text section from the battle units: one 0xFE
 * terminated name per battle unit slot, with a terminator appended to a name
 * that fills all 16 bytes. */
void text_build_battle_unit_names(void) {
    u8* output = g_text_section_pointers[TEXT_SECTION_ROSTER_UNIT_NAMES];
    s32 unit_id;

    for (unit_id = 0; unit_id < UNIT_SLOT_COUNT; unit_id++) {
        unit_stats_t* unit = unit_stats_get(unit_id);
        s32 length = 0;

        do {
            u8 value = unit->name[length];
            *output = value;
            if ((value & 0xfe) != 0xfe) {
                output++;
                if (length == UNIT_NAME_SIZE - 1) {
                    *output++ = 0xfe;
                }
            } else {
                output++;
                break;
            }
            length++;
        } while (length < UNIT_NAME_SIZE);
    }
}
