/* REQUIRE dispatches the requested battle-condition display or party change.
 * Recruitment also uses this unit's HELPMENU bridge and battle-unit name text. */

#include "fft/battle/after_battle.h"
#include "fft/battle/conditions.h"
#include "fft/battle/war_trophy.h"
#include "fft/game/battle_unit.h"
#include "fft/menu/help.h"
#include "fft/party/unit_panel.h"
#include "fft/script/script_instruction.h"
#include "fft/sys/input.h"
#include "fft/sys/memory.h"
#include "fft/sys/overlay.h"
#include "fft/sys/sound.h"
#include "fft/sys/thread.h"
#include "fft/text/text.h"
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

    thread_current_set_task(THREAD_TASK_CONDITION_DISPATCH);
    condition = thread_current_get_parameter_1();
    parameter = thread_current_get_parameter_2();
    if (condition >= EVENT_DISPLAY_CONDITION_GENERAL_FIRST) {
        conditions_show_general_banner(parameter);
    }
    thread = thread_resolve_slot(THREAD_SLOT_FIRST_IDLE);

    if (condition == EVENT_DISPLAY_CONDITION_SPECIAL_CASES) {
        thread_start(thread, conditions_special_thread);
        thread_set_parameters(thread, CONDITIONS_BANNER_READY, 0, 0);
        input_wait_frames_or_skip(parameter);
        thread_set_parameters(thread, 0, 0, 2);
        thread_wait_inactive(thread);
    } else if (condition == EVENT_DISPLAY_CONDITION_SHOW_READY_BUGGED) {
        thread_start(thread, conditions_hovered_stats_thread);
        thread_set_parameters(thread, 0, 0, 0);
        thread_wait_inactive(thread);
    } else if (condition == EVENT_DISPLAY_CONDITION_BATTLE_CONGRATULATIONS) {
        thread_start(thread, conditions_congratulations_thread);
        thread_set_parameters(thread, 0, 0, 0);
        thread_wait_inactive(thread);
    } else if (condition == EVENT_DISPLAY_CONDITION_GIL_REWARD) {
        thread_start(thread, war_trophy_gil_ticker_thread);
        thread_set_parameters(thread, 0, 0, 0);
        thread_wait_inactive(thread);
    } else if (condition == EVENT_DISPLAY_CONDITION_WAR_TROPHIES) {
        thread_start(thread, war_trophy_list_thread);
        thread_set_parameters(thread, 0, 0, 0);
        thread_wait_inactive(thread);
    } else if (condition == EVENT_DISPLAY_CONDITION_APPLY_PERMANENT_BRAVE_FAITH_CHANGES) {
        thread_start(thread, after_battle_brave_faith_report_thread);
        thread_set_parameters(thread, 0, 0, 0);
        thread_wait_inactive(thread);
    } else if (condition == EVENT_DISPLAY_CONDITION_REMOVE_LOW_BRAVE_HIGH_FAITH_UNITS) {
        thread_start(thread, after_battle_departures_thread);
        thread_set_parameters(thread, 0, 0, 0);
        thread_wait_inactive(thread);
    } else if (condition == EVENT_DISPLAY_CONDITION_APPLY_POST_BATTLE_UNIT_CHANGES) {
        after_battle_apply_unit_changes();
    }
    thread_current_exit();
}

/* Thread body that runs the HELPMENU overlay from the REQUIRE screen: waits
 * until 0x10000 bytes can be reserved at the high overlay address, loads the
 * overlay and runs helpmenu_after_battle_entrypoint in the first idle slot,
 * with the panel's two animation flags set while it runs. */
void help_menu_after_battle_thread(void) {
    s32 thread_id;

    thread_current_set_task(THREAD_TASK_HELP_MENU);
    g_sound_pending_sfx_id = SOUND_SFX_CONFIRM;
    g_unit_panel_button_prompt_animation[0] = 1;
    g_unit_panel_button_prompt_animation[1] = 1;
    while (heap_reserve_at(g_heap_high_overlay_load_address, 0x10000) != g_heap_high_overlay_load_address) {
        thread_yield();
    }
    overlay_companion_load(COMPANION_OVERLAY_HELPMENU);
    thread_id = thread_resolve_slot(THREAD_SLOT_FIRST_IDLE);
    thread_start(thread_id, helpmenu_after_battle_entrypoint);
    thread_wait_inactive(thread_id);
    g_unit_panel_button_prompt_animation[0] = 0;
    g_unit_panel_button_prompt_animation[1] = 0;
    thread_current_exit();
}

/* Rebuild the roster name text section from the battle units: one 0xFE
 * terminated name per battle unit slot, with a terminator appended to a name
 * that fills all 16 bytes. */
void text_build_battle_unit_names(void) {
    u8* output = g_text_section_pointers[TEXT_SECTION_ROSTER_UNIT_NAMES];
    s32 unit_id;

    for (unit_id = 0; unit_id < BATTLE_UNIT_SLOT_COUNT; unit_id++) {
        battle_unit_t* unit = battle_unit_get(unit_id);
        s32 length;

        for (length = 0; length < UNIT_NAME_SIZE; length++) {
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
        }
    }
}
