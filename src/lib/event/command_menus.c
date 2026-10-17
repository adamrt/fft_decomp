/* Command menu confirmation, action restrictions, system functions and scenario conditions. */

#include "fft/ability.h"
#include "fft/ability_list.h"
#include "fft/action.h"
#include "fft/ai.h"
#include "fft/battle_map.h"
#include "fft/battle_map_load.h"
#include "fft/camera.h"
#include "fft/command.h"
#include "fft/crystal.h"
#include "fft/cursor.h"
#include "fft/effect_script.h"
#include "fft/encounter.h"
#include "fft/equipment.h"
#include "fft/fade.h"
#include "fft/gfx.h"
#include "fft/gfx_primitive.h"
#include "fft/input.h"
#include "fft/item.h"
#include "fft/job.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/menu_gfx.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/menu_selection.h"
#include "fft/menu_window.h"
#include "fft/message.h"
#include "fft/music.h"
#include "fft/option.h"
#include "fft/overlay.h"
#include "fft/result.h"
#include "fft/save.h"
#include "fft/screen.h"
#include "fft/script.h"
#include "fft/script_instruction.h"
#include "fft/script_variable.h"
#include "fft/skillset.h"
#include "fft/sound.h"
#include "fft/sprite_sheet.h"
#include "fft/state.h"
#include "fft/status.h"
#include "fft/status_screen.h"
#include "fft/system.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/tutorial.h"
#include "fft/unit.h"
#include "fft/unit_event.h"
#include "fft/unit_identity.h"
#include "fft/unit_list.h"
#include "fft/unit_panel.h"
#include "fft/unit_runtime.h"
#include "fft/unit_view.h"
#include "fft/weather.h"
#include "fft/world_map.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/pad.h"
#include "psx/types.h"

/* One row of g_menu_system_function_table (menu_run_system_function_thread,
 * menu_dispatch_system_function). Provisional. */
typedef struct menu_system_function {
    s16 menu_id;                /* 0x00; copied to g_command_current_id */
    u16 text_id;                /* 0x02; menu entry text id */
    s16 menu_entry_index;       /* 0x04; g_menu_thread_menu_data index */
    u16 row_action;             /* 0x06; copied to g_menu_system_function_row_actions */
    u16 thread_count;           /* 0x08; menu entry thread_count */
    u8 _padding_0a[2];          /* aligns thread_entry */
    void (*thread_entry)(void); /* 0x0c; started as thread 8 */
    s16 alternate_id;           /* 0x10; replaces the id when navigation messages are not On */
    u8 _padding_12[2];          /* tail padding to 4-byte alignment */
} menu_system_function_t;
typedef char menu_system_function_size_must_be_20[(sizeof(menu_system_function_t) == 20) ? 1 : -1];

/* System-menu requests: callbacks and their initial message/selection parameters. */
static menu_system_function_t g_menu_system_function_table[51] = {
    { MENU_COMMAND_ID_MESSAGE_YES_NO, 0x8004, MENU_COMMAND_ID_MESSAGE, MENU_COMMAND_ID_MESSAGE_YES_NO, 1, { 0, 0 },
        menu_icon_linked_entry_thread, 0, { 0, 0 } }, /* 0 */
    { MENU_COMMAND_ID_MESSAGE, 0x800f, MENU_COMMAND_ID_MESSAGE, 0xffff, 1, { 0, 0 }, menu_icon_linked_entry_thread,
        MENU_SYSTEM_FUNCTION_CLOSE, { 0, 0 } }, /* 1 */
    { MENU_COMMAND_ID_TARGET_KIND, 0x5056, MENU_COMMAND_ID_TARGET_KIND, 0xffff, 1, { 0, 0 },
        menu_run_icon_selection_loop, 2, { 0, 0 } }, /* 2 */
    { MENU_COMMAND_ID_PANEL_EXECUTE, 0x5058, MENU_COMMAND_ID_PANEL_EXECUTE, 0xffff, 1, { 0, 0 },
        menu_run_icon_selection_loop, 3, { 0, 0 } }, /* 3 */
    { MENU_COMMAND_ID_NONE, 0x505a, MENU_COMMAND_ID_MESSAGE, 0xffff, 0, { 0, 0 }, mini_menu_cancel_delay_thread,
        MENU_SYSTEM_FUNCTION_CLOSE, { 0, 0 } }, /* 4 */
    { MENU_COMMAND_ID_NONE, 0x505a, MENU_COMMAND_ID_MESSAGE, 0xffff, 0, { 0, 0 }, mini_menu_cancel_delay_thread,
        MENU_SYSTEM_FUNCTION_CLOSE, { 0, 0 } }, /* 5 */
    { MENU_COMMAND_ID_MESSAGE, 0x800b, MENU_COMMAND_ID_MESSAGE, 0xffff, 1, { 0, 0 }, menu_icon_linked_entry_thread,
        MENU_SYSTEM_FUNCTION_CLOSE, { 0, 0 } }, /* 6 */
    { MENU_COMMAND_ID_MESSAGE, 0x8003, MENU_COMMAND_ID_MESSAGE, 0xffff, 1, { 0, 0 }, menu_icon_linked_entry_thread,
        MENU_SYSTEM_FUNCTION_CLOSE, { 0, 0 } }, /* 7 */
    { MENU_COMMAND_ID_NONE, 0x505a, MENU_COMMAND_ID_MESSAGE, 0xffff, 0, { 0, 0 }, mini_menu_cancel_delay_thread,
        MENU_SYSTEM_FUNCTION_CLOSE, { 0, 0 } }, /* 8 */
    { MENU_COMMAND_ID_EXECUTE, 0x5061, MENU_COMMAND_ID_EXECUTE, 0xffff, 1, { 0, 0 }, menu_run_icon_selection_loop, 9,
        { 0, 0 } }, /* 9 */
    { MENU_COMMAND_ID_MESSAGE, 0x5062, MENU_COMMAND_ID_ANNOUNCEMENT, 0xffff, 1, { 0, 0 }, menu_icon_timed_thread,
        MENU_SYSTEM_FUNCTION_ABILITY_ANNOUNCEMENT, { 0, 0 } }, /* 10 */
    { 100, 0x8010, MENU_COMMAND_ID_MESSAGE, MENU_COMMAND_ID_MESSAGE_TARGET_KIND, 1, { 0, 0 },
        menu_icon_linked_entry_thread, 2, { 0, 0 } }, /* 11 */
    { 101, 0x8011, MENU_COMMAND_ID_MESSAGE, MENU_COMMAND_ID_MESSAGE_PANEL_EXECUTE, 1, { 0, 0 },
        menu_icon_linked_entry_thread, 3, { 0, 0 } }, /* 12 */
    { 102, 0x8011, MENU_COMMAND_ID_MESSAGE, MENU_COMMAND_ID_MESSAGE_EXECUTE, 1, { 0, 0 }, menu_icon_linked_entry_thread,
        9, { 0, 0 } }, /* 13 */
    { MENU_COMMAND_ID_NONE, 0x505a, MENU_COMMAND_ID_MESSAGE, 0xffff, 0, { 0, 0 }, mini_menu_cancel_delay_thread,
        MENU_SYSTEM_FUNCTION_CLOSE, { 0, 0 } },                                                                /* 14 */
    { 99, 0x1004, MENU_COMMAND_ID_MESSAGE, 0xffff, 1, { 0, 0 }, menu_icon_linked_entry_thread, 15, { 0, 0 } }, /* 15 */
    { 99, 0x8006, MENU_COMMAND_ID_MESSAGE, 0xffff, 1, { 0, 0 }, menu_icon_linked_entry_thread,
        MENU_SYSTEM_FUNCTION_CLOSE, { 0, 0 } }, /* 16 */
    { 99, 0x8007, MENU_COMMAND_ID_MESSAGE, 0xffff, 1, { 0, 0 }, menu_icon_linked_entry_thread,
        MENU_SYSTEM_FUNCTION_CLOSE, { 0, 0 } }, /* 17 */
    { 99, 0x8008, MENU_COMMAND_ID_MESSAGE, 0xffff, 1, { 0, 0 }, menu_icon_linked_entry_thread,
        MENU_SYSTEM_FUNCTION_CLOSE, { 0, 0 } }, /* 18 */
    { 99, 0x8009, MENU_COMMAND_ID_MESSAGE, 0xffff, 1, { 0, 0 }, menu_icon_linked_entry_thread,
        MENU_SYSTEM_FUNCTION_CLOSE, { 0, 0 } }, /* 19 */
    { MENU_COMMAND_ID_MESSAGE, 0x1012, MENU_COMMAND_ID_MESSAGE, 0xffff, 1, { 0, 0 }, menu_icon_linked_entry_thread, 20,
        { 0, 0 } }, /* 20 */
    { MENU_COMMAND_ID_MESSAGE, 0x1015, MENU_COMMAND_ID_MESSAGE, 0xffff, 1, { 0, 0 }, menu_icon_linked_entry_thread, 21,
        { 0, 0 } },                                                                                            /* 21 */
    { 99, 0x1014, MENU_COMMAND_ID_MESSAGE, 0xffff, 1, { 0, 0 }, menu_icon_linked_entry_thread, 22, { 0, 0 } }, /* 22 */
    { 99, 0x1016, MENU_COMMAND_ID_MESSAGE, 0xffff, 1, { 0, 0 }, menu_icon_linked_entry_thread, 23, { 0, 0 } }, /* 23 */
    { MENU_COMMAND_ID_AUTO_BATTLE_UNIT, 0x506e, MENU_COMMAND_ID_AUTO_BATTLE_UNIT, 0xffff, 1, { 0, 0 },
        menu_run_icon_selection_loop, MENU_SYSTEM_FUNCTION_CLOSE, { 0, 0 } }, /* 24 */
    { MENU_COMMAND_ID_AUTO_BATTLE_UNIT, 0x506f, MENU_COMMAND_ID_AUTO_BATTLE_UNIT, 0xffff, 1, { 0, 0 },
        menu_run_icon_selection_loop, MENU_SYSTEM_FUNCTION_CLOSE, { 0, 0 } }, /* 25 */
    { 103, 0x800d, MENU_COMMAND_ID_MESSAGE, MENU_COMMAND_ID_MESSAGE_ATTACK_UNIT, 1, { 0, 0 },
        menu_icon_linked_entry_thread, 24, { 0, 0 } }, /* 26 */
    { 104, 0x800e, MENU_COMMAND_ID_MESSAGE, MENU_COMMAND_ID_MESSAGE_DEFEND_UNIT, 1, { 0, 0 },
        menu_icon_linked_entry_thread, 25, { 0, 0 } }, /* 27 */
    { MENU_COMMAND_ID_MESSAGE_YES_NO, 0x802e, MENU_COMMAND_ID_MESSAGE, MENU_COMMAND_ID_MESSAGE_YES_NO, 1, { 0, 0 },
        menu_icon_linked_entry_thread, MENU_SYSTEM_FUNCTION_CLOSE, { 0, 0 } }, /* 28 */
    { MENU_COMMAND_ID_MESSAGE, 0x0, MENU_COMMAND_ID_EFFECT_MESSAGE, 0xffff, 1, { 0, 0 }, menu_icon_timed_thread,
        MENU_SYSTEM_FUNCTION_EFFECT_MESSAGE, { 0, 0 } }, /* 29 */
    { MENU_COMMAND_ID_MESSAGE_YES_NO, 0x1017, MENU_COMMAND_ID_MESSAGE, MENU_COMMAND_ID_MESSAGE_YES_NO, 1, { 0, 0 },
        menu_icon_linked_entry_thread, MENU_SYSTEM_FUNCTION_CLOSE, { 0, 0 } }, /* 30 */
    { MENU_COMMAND_ID_CRYSTAL_PICKUP, 0x505a, MENU_COMMAND_ID_MESSAGE, 0xffff, 0, { 0, 0 },
        mini_menu_cancel_delay_thread, MENU_SYSTEM_FUNCTION_CRYSTAL_OR_TREASURE, { 0, 0 } },                   /* 31 */
    { 99, 0x1013, MENU_COMMAND_ID_MESSAGE, 0xffff, 1, { 0, 0 }, menu_icon_linked_entry_thread, 32, { 0, 0 } }, /* 32 */
    { MENU_COMMAND_ID_MESSAGE_YES_NO, 0x8006, MENU_COMMAND_ID_MESSAGE, MENU_COMMAND_ID_MESSAGE_YES_NO, 1, { 0, 0 },
        menu_icon_linked_entry_thread, MENU_SYSTEM_FUNCTION_CLOSE, { 0, 0 } }, /* 33 */
    { MENU_COMMAND_ID_MESSAGE, 0x1830, MENU_COMMAND_ID_MESSAGE, 0xffff, 1, { 0, 0 }, menu_icon_linked_entry_thread,
        MENU_SYSTEM_FUNCTION_TRAP_RESULT_FIRST, { 0, 0 } }, /* 34 */
    { MENU_COMMAND_ID_MESSAGE, 0x1831, MENU_COMMAND_ID_MESSAGE, 0xffff, 1, { 0, 0 }, menu_icon_linked_entry_thread, 35,
        { 0, 0 } }, /* 35 */
    { MENU_COMMAND_ID_MESSAGE, 0x1832, MENU_COMMAND_ID_MESSAGE, 0xffff, 1, { 0, 0 }, menu_icon_linked_entry_thread, 36,
        { 0, 0 } }, /* 36 */
    { MENU_COMMAND_ID_MESSAGE, 0x1833, MENU_COMMAND_ID_MESSAGE, 0xffff, 1, { 0, 0 }, menu_icon_linked_entry_thread, 37,
        { 0, 0 } }, /* 37 */
    { MENU_COMMAND_ID_MESSAGE, 0x101d, MENU_COMMAND_ID_MESSAGE, 0xffff, 1, { 0, 0 }, menu_icon_linked_entry_thread,
        MENU_SYSTEM_FUNCTION_MOVE_FIND_ITEM_SOLD, { 0, 0 } }, /* 38 */
    { MENU_COMMAND_ID_MESSAGE, 0x101c, MENU_COMMAND_ID_MESSAGE, 0xffff, 1, { 0, 0 }, menu_icon_linked_entry_thread,
        MENU_SYSTEM_FUNCTION_MOVE_FIND_ITEM, { 0, 0 } }, /* 39 */
    { MENU_COMMAND_ID_MESSAGE, 0x0, MENU_COMMAND_ID_MESSAGE, 0xffff, 1, { 0, 0 }, menu_input_thread,
        MENU_SYSTEM_FUNCTION_MOVE_CONFIRM_SILENT, { 0, 0 } },                                                  /* 40 */
    { 99, 0x100a, MENU_COMMAND_ID_MESSAGE, 0xffff, 1, { 0, 0 }, menu_icon_linked_entry_thread, 41, { 0, 0 } }, /* 41 */
    { 106, 0x800c, MENU_COMMAND_ID_MESSAGE, MENU_COMMAND_ID_MESSAGE_EXECUTE, 1, { 0, 0 }, menu_icon_linked_entry_thread,
        43, { 0, 0 } },                                                                                       /* 42 */
    { 98, 0x5061, MENU_COMMAND_ID_EXECUTE, 0xffff, 1, { 0, 0 }, menu_run_icon_selection_loop, 43, { 0, 0 } }, /* 43 */
    { MENU_COMMAND_ID_MESSAGE, 0x5089, MENU_COMMAND_ID_ANNOUNCEMENT, 0xffff, 1, { 0, 0 }, menu_icon_timed_thread,
        MENU_SYSTEM_FUNCTION_ARITHMETICKS_ANNOUNCEMENT, { 0, 0 } }, /* 44 */
    { 101, 0x8012, MENU_COMMAND_ID_MESSAGE, MENU_COMMAND_ID_MESSAGE_PANEL_EXECUTE, 1, { 0, 0 },
        menu_icon_linked_entry_thread, 3, { 0, 0 } }, /* 45 */
    { 102, 0x8012, MENU_COMMAND_ID_MESSAGE, MENU_COMMAND_ID_MESSAGE_EXECUTE, 1, { 0, 0 }, menu_icon_linked_entry_thread,
        9, { 0, 0 } }, /* 46 */
    { 99, 0x100f, MENU_COMMAND_ID_MESSAGE, 0xffff, 1, { 0, 0 }, menu_icon_linked_entry_thread,
        MENU_SYSTEM_FUNCTION_CLOSE, { 0, 0 } }, /* 47 */
    { MENU_COMMAND_ID_MESSAGE_YES_NO, 0x8005, MENU_COMMAND_ID_MESSAGE, MENU_COMMAND_ID_MESSAGE_YES_NO, 1, { 0, 0 },
        menu_icon_linked_entry_thread, MENU_SYSTEM_FUNCTION_CLOSE, { 0, 0 } },                                 /* 48 */
    { 99, 0x1025, MENU_COMMAND_ID_MESSAGE, 0xffff, 1, { 0, 0 }, menu_icon_linked_entry_thread, 49, { 0, 0 } }, /* 49 */
    { MENU_COMMAND_ID_MESSAGE, 0x508c, MENU_COMMAND_ID_ANNOUNCEMENT, 0xffff, 1, { 0, 0 }, menu_icon_timed_thread,
        MENU_SYSTEM_FUNCTION_POST_ACTION_RESULT, { 0, 0 } }, /* 50 */
};

/* Operand-halfword counts through condition 37. The compiler aligns the
 * following weather table; the original declared extent is unknown. */
static u8 g_scenario_condition_param_lengths[38] = { 0, 2, 2, 2, 1, 2, 2, 2, 2, 2, 2, 1, 1, 1, 2, 2, 3, 3, 2, 2, 2, 2,
    0, 3, 4, 2, 8, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4 };

static s32 menu_dispatch_system_function(
    s32 command, s32 option, s32 unit_id, s32 parameter, s32 enabled, action_command_t* action);
static event_scenario_condition_result_e scenario_check_condition(
    event_scenario_condition_e type, s32 id, s32 value, s32 tile_y, s32 elevation);

/* Queue the confirm sound unless a menu restore is replaying selections. */
static void sound_queue_sfx_confirm_unless_restoring(void) {
    if (g_menu_restore_pending == 0) {
        g_sound_pending_sfx_id = SOUND_SFX_CONFIRM;
    }
}

/* Runs the menu's action handler on a simulated, silent Circle press with
 * option 0 selected, then clears the injected input. */
static void menu_confirm_action_silently(menu_entry_t* menu) {
    g_sound_suppressed = 1;
    g_script_event_input = PSX_PAD_CIRCLE;
    menu_handle_action(menu, 0);
    g_sound_suppressed = 0;
    g_script_event_input = 0;
}

/* Menu-opening thread 6: the Wait menu. */
void command_wait_thread(void) {
    menu_entry_t* menu = (menu_entry_t*)thread_get_current_parameter_1();

    g_command_wait_row_actions[0] = -2;
    g_command_wait_row_actions[1] = -2;
    menu_confirm_action_silently(menu);
    thread_exit_current();
}

/* Thread entry of menu entry 0x22, which confirms the Move command.
 *
 * A unit with Don't Move gets the invalid sound and the linked-entry icon thread
 * (stored row action -1). In a menu whose move is already used (ids 0x0e, 0x14,
 * 0x21, 0x2f, 0x31 and 0x33) only the invalid sound plays. Otherwise the command
 * is confirmed at once, as a silent Circle press. */
void command_move_thread(void) {
    s16* stored_actions;
    menu_entry_t* entry = (menu_entry_t*)thread_get_current_parameter_1();
    s32 menu;

    stored_actions = &g_command_confirm_stored_unit_row_actions;
    *stored_actions = -2;
    if (menu_get_selected_unit_stats()->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DONT_MOVE)]
        & STATUS_BYTE_MASK(STATUS_ID_DONT_MOVE)) {
        *stored_actions = -1;
        sound_queue_sfx_invalid();
        menu_icon_linked_entry_thread();
        thread_exit_current();
    }
    menu = g_command_current_id;
    if (menu == MENU_COMMAND_ID_TURN_MOVED || menu == MENU_COMMAND_ID_TURN_MOVED_ACTED
        || menu == MENU_COMMAND_ID_TURN_NO_ACT_MOVED || menu == MENU_COMMAND_ID_RIDER_TURN_MOVED
        || menu == MENU_COMMAND_ID_RIDER_TURN_MOVED_ACTED || menu == MENU_COMMAND_ID_RIDER_TURN_NO_ACT_MOVED) {
        sound_queue_sfx_invalid();
        thread_exit_current();
    }
    sound_queue_sfx_confirm_unless_restoring();
    menu_confirm_action_silently(entry);
    thread_exit_current();
}

/*
 * Check action-menu restrictions and show the applicable warning.
 *
 * Menu groups 0x13/0x14 and 0x30/0x31 use the battle eligibility check.
 * Other menus inspect the unit's current status and last action. The packed
 * options can suppress status warnings, but messages 0x1002/0x1003 always
 * stop the current thread after any warning thread finishes.
 */
void command_check_restrictions(void) {
    s32 message = 0;
    unit_stats_t* unit;
    s32 menu;
    s32 result;
    s32 mode;

    unit = menu_get_selected_unit_stats();
    menu = g_command_current_id;
    if (menu == MENU_COMMAND_ID_TURN_ACTED || menu == MENU_COMMAND_ID_TURN_MOVED_ACTED
        || menu == MENU_COMMAND_ID_RIDER_TURN_ACTED || menu == MENU_COMMAND_ID_RIDER_TURN_MOVED_ACTED) {
        result = unit_get_action_restriction(unit);
        if (result == UNIT_ACTION_RESTRICTION_NONE) {
            sound_queue_sfx_invalid();
            thread_exit_current();
        }
        if (result == UNIT_ACTION_RESTRICTION_CANNOT_ACT) {
            message = 0x1002;
        }
        if (result == UNIT_ACTION_RESTRICTION_DEEP_WATER) {
            message = 0x1003;
        }
    } else {
        if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CHARGING)] & STATUS_BYTE_MASK(STATUS_ID_CHARGING)) {
            if (g_command_menu_types_by_skillset[unit->action_command.skillset_id] == ACTION_MENU_TYPE_CHARGE) {
                g_text_substitution_values[0] = unit->action_command.skillset_id | TEXT_ID_SKILLSET_NAME_SOURCE_BASE;
            } else {
                g_text_substitution_values[0] = unit->action_command.ability_id + TEXT_ID_ABILITY_NAME_BASE;
            }
            message = 0x8001;
        } else if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_PERFORMING)]
            & STATUS_BYTE_MASK(STATUS_ID_PERFORMING)) {
            message = 0x8002;
        }
    }
    /* Messages 0x10xx always show; 0x80xx are navigation messages that the option turns off.
     * The test compares the message's high byte with the option bits themselves, which works
     * because GAME_OPTIONS_NAVIGATION_MESSAGES_OFF_BITS is also 0x8000. */
    if (message != 0) {
        mode = g_game_options.value & GAME_OPTIONS_NAVIGATION_MESSAGES_MASK;
        if (mode == GAME_OPTIONS_NAVIGATION_MESSAGES_OFF_BITS && (message & 0xff00) == mode) {
            sound_queue_sfx_confirm_unless_restoring();
        } else if (g_menu_restore_pending == 0) {
            sound_queue_sfx_invalid();
            g_menu_thread_menu_data[MENU_COMMAND_ID_ACTION_RESTRICTION_MESSAGE].text_id = message;
            thread_start(g_thread_current_id - 1, menu_icon_linked_entry_thread);
            thread_set_parameters(g_thread_current_id - 1,
                (s32)&g_menu_thread_menu_data[MENU_COMMAND_ID_ACTION_RESTRICTION_MESSAGE], 0, 0);
            thread_wait_until_inactive(g_thread_current_id - 1);
        }
        if (message == 0x1002 || message == 0x1003) {
            thread_exit_current();
        }
    } else {
        sound_queue_sfx_confirm_unless_restoring();
    }
}

/* Check a chosen action-menu row before it opens. With check_unit, deep water, Frog or the menu's
 * own restriction shows its message, plays the invalid sound and ends the thread; an Arithmeticks
 * row shows navigation message 0x8009 - value. A row with no message and a negative action mode is
 * confirmed at once without sound. */
void command_check_row_restrictions(s32 menu, s32 value, s32 check_unit) {
    s32 mode;
    s32 message;
    menu_entry_t* parameter;
    unit_stats_t* unit;

    g_menu_active_menu = menu;
    parameter = (menu_entry_t*)thread_get_current_parameter_1();
    mode = 0;
    message = 0;
    unit = menu_get_selected_unit_stats();
    if (check_unit != 0) {
        if (unit_get_action_restriction(unit) == UNIT_ACTION_RESTRICTION_DEEP_WATER) {
            message = 0x1003;
            mode = 2;
        } else {
            if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FROG)] & STATUS_BYTE_MASK(STATUS_ID_FROG)) {
                message = 0x1007;
                mode = 2;
            } else {
                mode = 2;
                message = g_command_restriction_message_ids[menu] + 0x1000;
            }
        }
    }
    if (message == 0 && menu == ACTION_MENU_TYPE_ARITHMETICKS) {
        message = 0x8009 - value;
        mode = 1;
    }
    if (mode != 0) {
        if (g_game_options.fields.navigation_messages != GAME_OPTION_OFF || (message & 0xff00) != 0x8000) {
            g_menu_thread_menu_data[MENU_COMMAND_ID_ACTION_RESTRICTION_MESSAGE].text_id = message;
            thread_start(g_thread_current_id - 1, menu_icon_linked_entry_thread);
            thread_set_parameters(g_thread_current_id - 1,
                (s32)&g_menu_thread_menu_data[MENU_COMMAND_ID_ACTION_RESTRICTION_MESSAGE], 0, 0);
        }
        if (mode == 2) {
            sound_queue_sfx_invalid();
            if (g_game_options.fields.navigation_messages == GAME_OPTION_ON) {
                thread_wait_until_inactive(g_thread_current_id - 1);
            }
            thread_exit_current();
        }
    } else {
        mode = g_command_action_type_modes
            [g_menu_skillset_types[g_menu_thread_menu_data[MENU_COMMAND_ID_SKILLSET_LIST].selected_index]][0];
        if (mode < 0) {
            g_command_action_slot_row_actions[0] = mode;
            menu_confirm_action_silently(parameter);
            thread_exit_current();
        }
    }
    g_menu_restore_depth = 2;
}

/* Thread body: validates the skill chosen in menu entry 3
 * for the current menu (MP cost, blocking status, per-menu restrictions) and, when it is not usable, shows the matching
 * message before ending the thread. */
void command_validate_ability_thread(void) {
    s32 skill = g_menu_thread_menu_data[MENU_COMMAND_ID_ABILITY_LIST].selected_index;
    menu_entry_t* parameter = (menu_entry_t*)thread_get_current_parameter_1();
    s32 ct;
    s32 entry;
    s32 mp_cost;
    s32 message;
    s32 blocked;
    unit_stats_t* unit;
    menu_ability_list_t* skill_tables = &g_ai_workspace_ptr->ability_list;

    ct = skill_tables->skill_ct[skill];
    entry = g_ability_list_ability_layout.columns.text_colors[0][skill];
    mp_cost = skill_tables->mp_costs[skill];
    if (ct == 0xFF) {
        ct = 0;
    }
    message = 0;
    blocked = skill_tables->flags[skill] & MENU_ABILITY_FLAG_AFFECTED_BY_SILENCE;
    unit = menu_get_selected_unit_stats();
    if (g_menu_active_menu == ACTION_MENU_TYPE_DEFAULT && unit->mp - mp_cost < 0) {
        message = 0x800A;
        if (ct == 0) {
            message = 0x1008;
        }
    }
    g_command_selected_skill_ct = ct;
    if ((g_menu_active_menu == ACTION_MENU_TYPE_MONSTER || g_menu_active_menu == ACTION_MENU_TYPE_DEFAULT)
        && blocked != 0
        && (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_SILENCE)] & STATUS_BYTE_MASK(STATUS_ID_SILENCE))) {
        message = 0x8013;
        if (ct == 0) {
            message = 0x1009;
        }
    }
    if (g_menu_active_menu == ACTION_MENU_TYPE_ITEM_INVENTORY) {
        if (entry != 0) {
            message = 0x100B;
        }
    }
    if (g_menu_active_menu == ACTION_MENU_TYPE_WEAPON_INVENTORY && entry != 0) {
        message = 0x100C;
    }
    if (g_menu_active_menu == ACTION_MENU_TYPE_KATANA_INVENTORY) {
        if (entry != 0) {
            message = 0x100D;
        }
    }
    g_menu_result = -4;
    if (message != 0) {
        if (g_game_options.fields.navigation_messages == GAME_OPTION_OFF) {
            sound_queue_sfx_confirm_unless_restoring();
        } else {
            g_menu_modal_open = 1;
            g_menu_selected_option = 0;
            g_menu_thread_menu_data[MENU_COMMAND_ID_ACTION_RESTRICTION_MESSAGE].text_id = message;
            thread_start(g_thread_current_id - 1, menu_icon_linked_entry_thread);
            thread_set_parameters(g_thread_current_id - 1,
                (s32)&g_menu_thread_menu_data[MENU_COMMAND_ID_ACTION_RESTRICTION_MESSAGE], 0, 0);
            sound_queue_sfx_invalid();
            thread_wait_until_inactive(g_thread_current_id - 1);
            g_menu_modal_open = 0;
        }
        if ((message != 0x800A && message != 0x8013) || g_menu_selected_option == -1) {
            g_menu_result = -1;
        }
    } else {
        sound_queue_sfx_confirm_unless_restoring();
    }
    menu_confirm_action_silently(parameter);
    thread_exit_current();
}

/* 1 when the skillset is in g_announcement_spell_quote_exception_skillsets (ended by SKILLSET_ID_NONE).
 * announcement_select shows no spell quote and no ability name for such skillsets,
 * except Item and Throw, whose names still show. */
static s32 announcement_is_spell_quote_exception(s32 skillset_id) {
    s32 i;
    s32 found = 0;

    for (i = 0; g_announcement_spell_quote_exception_skillsets[i] != SKILLSET_ID_NONE; i++) {
        if (g_announcement_spell_quote_exception_skillsets[i] == skillset_id) {
            found = 1;
            break;
        }
    }
    return found;
}

/*
 * Resolve the pending action-menu command, option, and item selection.
 *
 * Remaps menu-specific values through the current menu's command map, publishes the
 * resolved selection, and returns command | option << 8 | item << 16.
 */
s32 command_resolve_selection(void) {
    s32 command = (u8)g_command_pending_selection[0];
    s32 option = (u8)g_command_pending_selection[1];
    u32 item = (u16)g_command_pending_selection[2];
    s32 index;
    s32 i;
    s32 menu;

    g_command_selected_command.kind = command;
    index = option;
    if (command != 0x12 && (command & 0xFE) != 0xFE) {
        menu = g_command_current_id;
        for (i = 0; i < 31; i++) {
            if (menu == g_command_maps[i].menu_id) {
                if (command != 0xFF) {
                    if (menu == MENU_COMMAND_ID_CRYSTAL_PICKUP) {
                        command = g_crystal_pickup_action;
                        g_command_selected_command.data.action.item_id = g_crystal_pickup_item;
                        option = MENU_SELECTION_NONE;
                    } else if (menu == MENU_COMMAND_ID_MESSAGE_YES_NO || menu >= 0x64) {
                        command = *(g_command_maps[i].map + option + 1);
                        option = MENU_SELECTION_NONE;
                    } else {
                        command = *(g_command_maps[i].map + command + 1);
                    }
                } else {
                    command = g_command_maps[i].map[0];
                }
                break;
            }
        }
        if (i == 31) {
            command = 7;
        }
        if (i < 16) {
            g_command_resolved_command = command;
        } else if (command == 5 || command == 6) {
            g_command_selected_command.data.action.targeting_type = command;
        }
        if (command == 1) {
            g_menu_restore_pending = 1;
        }
        if (command == 12) {
            if (option < 3) {
                s32 option_offset = (option - 1) * 2;
                s32 adjusted_item = item + 12;

                command = option_offset + adjusted_item;
            } else {
                command = option + 13;
            }
            option = MENU_SELECTION_NONE;
            item = MENU_SELECTION_NONE;
            g_command_resolved_command = command;
        }
        g_command_selected_command.kind = command;
        if ((option & 0xFE) != 0xFE) {
            option = g_command_skillsets[index];
            if (option == SKILLSET_ID_ATTACK || option == SKILLSET_ID_DEFEND || option == SKILLSET_ID_EQUIP_CHANGE
                || option == SKILLSET_ID_BLANK_04 || option == SKILLSET_ID_ELEMENTAL || option == SKILLSET_ID_JUMP) {
                g_command_selected_command.data.action.ability_id = 0;
                item = MENU_SELECTION_NONE;
            } else if (option == ACTION_MENU_PSEUDO_SKILLSET_ANYTHING && g_command_skillset_build_result == 0) {
                item = g_ability_list_anything_ability_id;
            } else {
                item = g_ai_workspace_ptr->ability_list.ids[item] & 0x1FF;
            }
            g_command_selected_command.data.action.skillset_id = option;
            g_command_used_skillset_id = option;
            if (item != MENU_SELECTION_NONE) {
                if (option == SKILLSET_ID_ITEM || option == SKILLSET_ID_DRAW_OUT || option == SKILLSET_ID_THROW) {
                    g_command_selected_command.data.action.item_id = item;
                } else {
                    g_command_selected_command.data.action.ability_id = item;
                }
                g_command_used_item_id = item;
            }
        }
    }
    return command | ((option & 0xFF) << 8) | (item << 16);
}

/* Wait for menu/event threads, then start the selected system-function entry. */
static void menu_run_system_function_thread(void) {
    s32 id;
    s32 menu_id;
    menu_system_function_t* entry;

    g_menu_system_function_thread_busy = 1;
    text_init_section_pointers();
    id = g_thread_contexts[g_thread_current_id].function_parameter_1;
    if (g_game_options.fields.navigation_messages != GAME_OPTION_ON) {
        id = g_menu_system_function_table[id].alternate_id;
    }
    thread_wait_until_inactive(THREAD_SLOT_MENU);
    if (g_menu_input_mode == MENU_INPUT_PLAYER) {
        while (thread_is_running_asm(THREAD_SLOT_EVENT) != 0) {
            if (g_thread_contexts[THREAD_SLOT_EVENT].task_id == THREAD_TASK_EXECUTE_EVENT) {
                thread_exit_current();
            }
            thread_yield();
        }
    }
    camera_wait_until_idle();
    entry = &g_menu_system_function_table[id];
    menu_id = entry->menu_id;
    g_menu_thread_menu_data[entry->menu_entry_index].text_id = entry->text_id;
    g_menu_system_function_row_actions[0] = entry->row_action;
    g_command_current_id = menu_id;
    g_menu_thread_menu_data[entry->menu_entry_index].thread_count = entry->thread_count;
    if (id == MENU_SYSTEM_FUNCTION_CRYSTAL_OR_TREASURE) {
        thread_start(THREAD_SLOT_MENU, crystal_open_pickup_panel);
        thread_set_parameters(THREAD_SLOT_MENU, 0, g_crystal_pickup_panel_parameter, 0);
        thread_exit_current();
    }
    if (id != MENU_SYSTEM_FUNCTION_CLOSE) {
        tutorial_pulse_wait_value(id);
    }
    thread_start(THREAD_SLOT_MENU, entry->thread_entry);
    thread_set_four_parameters(THREAD_SLOT_MENU, (s32)&g_menu_thread_menu_data[entry->menu_entry_index], 0, 0, 0);
    g_menu_system_function_thread_busy = 0;
    thread_exit_current();
}

/* Clear menu selections and start the system-function worker. */
static void menu_start_system_function_thread(s32 system_function) {
    menu_clear_entry_selections();
    thread_start(THREAD_SLOT_SYSTEM_FUNCTION, menu_run_system_function_thread);
    thread_set_parameters(THREAD_SLOT_SYSTEM_FUNCTION, system_function, 0, 0);
}

/* Confirm the current menu action, recording the auto-battle mode it selects. */
void auto_battle_confirm_thread(void) {
    unit_stats_t* attacker;
    s32 menu;
    menu_entry_t* parameter = (menu_entry_t*)thread_get_current_parameter_1();

    attacker = menu_get_selected_unit_stats();
    menu = g_menu_thread_menu_data[MENU_COMMAND_ID_AUTO_BATTLE].selected_index;
    if (menu == 3) {
        attacker->auto_battle_setting = AI_AUTOBATTLE_FADING_LIFE;
    } else if (menu == 4) {
        attacker->auto_battle_setting = AI_AUTOBATTLE_RETREAT;
    }
    g_menu_set_auto_battle_row_actions = -3;
    menu_confirm_action_silently(parameter);
    thread_exit_current();
}

/* Store a unit's auto-battle target together with the resolved menu command as
 * its auto-battle setting. */
void auto_battle_store_target(s32 target_id, s32 battle_id) {
    unit_stats_t* stats = unit_get_stats(battle_id);
    stats->auto_battle_target = target_id;
    stats->auto_battle_setting = g_command_resolved_command;
}

/* Choose how an ability is announced: with the caster's spell quote, or with its name.
 *
 * A spell quote (SPELL_QUOTE) needs a skillset outside the exception list and not one the
 * unit's spell_quote_skillsets flags suppress, an ability other than the last one quoted
 * (g_announcement_spell_quote_last_ability_id) that is no item and has the spell-quote flag, a unit
 * that is not a Calculator or Mime and neither Transparent nor a Frog, ability names on, and a
 * 4 in 16 roll of the event RNG. Otherwise ABILITY_ANNOUNCEMENT runs: enabled when the
 * ability's type flags ask for its name (names on) or it is an Item or Throw, else with enabled
 * cleared. */
void announcement_select(action_command_t* action, s32 unit_id, s32 enabled) {
    const ability_data_t* ability;
    ability_secondary_pointer_t secondary;
    unit_stats_t* unit;
    s32 known;

    ability_get_data_and_type(action->ability_id & ABILITY_ID_MASK, &ability, &secondary);
    unit = unit_get_stats(unit_id);
    if (action->skillset_id != 0) {
        known = announcement_is_spell_quote_exception(action->skillset_id);
    } else {
        known = 1;
    }
    if (action->skillset_id == unit->primary_skillset
        && (unit->position.spell_quote_skillsets & UNIT_SPELL_QUOTE_SUPPRESS_PRIMARY)) {
        known = 1;
    }
    if (action->skillset_id == unit->secondary_skillset
        && (unit->position.spell_quote_skillsets & UNIT_SPELL_QUOTE_SUPPRESS_SECONDARY)) {
        known = 1;
    }
    if (known == 0 && g_announcement_spell_quote_last_ability_id != (action->ability_id & ABILITY_ID_MASK)
        && unit->job_id != JOB_ID_CALCULATOR && unit->job_id != JOB_ID_MIME
        && action->ability_id < ABILITY_ID_ITEM_FIRST
        && (secondary.range->flags_3 & ABILITY_SECONDARY_FLAG_3_SPELL_QUOTE)
        && !(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_TRANSPARENT)]
            & (STATUS_BYTE_MASK(STATUS_ID_TRANSPARENT) | STATUS_BYTE_MASK(STATUS_ID_FROG)))
        && g_game_options.fields.ability_names == GAME_OPTION_ON
        && (script_get_variable(EVENT_SCRIPT_VAR_RANDOM_VALUE) & 0xF) < 4) {
        g_announcement_spell_quote_last_ability_id = action->ability_id & ABILITY_ID_MASK;
        menu_dispatch_system_function(MENU_SYSTEM_COMMAND_SPELL_QUOTE, 0, unit_id, 0, enabled, action);
        return;
    }
    if (action->skillset_id != 0) {
        known = announcement_is_spell_quote_exception(action->skillset_id);
    } else {
        known = 0;
    }
    if ((known == 0 && (ability->type_flags & ABILITY_TYPE_FLAG_SHOW_NAME)
            && g_game_options.fields.ability_names == GAME_OPTION_ON)
        || action->skillset_id == SKILLSET_ID_ITEM || action->skillset_id == SKILLSET_ID_THROW) {
        menu_dispatch_system_function(MENU_SYSTEM_COMMAND_ABILITY_ANNOUNCEMENT, 0, unit_id, 0, enabled, action);
    } else {
        menu_dispatch_system_function(MENU_SYSTEM_COMMAND_ABILITY_ANNOUNCEMENT, 0, unit_id, 0, 0, action);
    }
}

/* Run a system function (menu_system_command_e) for unit_id, looking up
 * the unit's command action first except for SCENARIO_CHECK and CLOSE.
 *
 * SPELL_QUOTE goes to announcement_select, a void routine whose
 * leftover v0 is returned. ABILITY_ANNOUNCEMENT drops option, parameter and
 * enabled. Everything else returns menu_dispatch_system_function's result;
 * for SCENARIO_CHECK and CLOSE the action pointer passed on is unset. */
s32 menu_init_system_function(s32 type, s32 option, s32 unit_id, s32 parameter, s32 enabled) {
    action_command_t* action;

    if (type != MENU_SYSTEM_COMMAND_SCENARIO_CHECK && type != MENU_SYSTEM_COMMAND_CLOSE) {
        action = unit_get_runtime_action_command_by_battle_id(unit_id);
        if (type == MENU_SYSTEM_COMMAND_SPELL_QUOTE) {
            /* The target returns the void callee's leftover $v0. */
            return ((s32 (*)(action_command_t*, s32, s32))announcement_select)(action, unit_id, enabled);
        }
        if (type == MENU_SYSTEM_COMMAND_ABILITY_ANNOUNCEMENT) {
            return menu_dispatch_system_function(MENU_SYSTEM_COMMAND_ABILITY_ANNOUNCEMENT, 0, unit_id, 0, 0, action);
        }
    }
    return menu_dispatch_system_function(type, option, unit_id, parameter, enabled, action);
}

/* Run crystal_build_pickup_result for the unit on the main stack,
 * keep its record in g_crystal_pickup_context and return the treasure item for
 * CRYSTAL_PICKUP_TREASURE, otherwise -1. */
s32 crystal_resolve_pickup(s32 battle_id) {
    unit_stats_t* stats = unit_get_stats(battle_id);
    crystal_pickup_result_t* result;

    g_thread_call_target = (void (*)(void))crystal_build_pickup_result;
    result = (crystal_pickup_result_t*)thread_call_on_main_stack(stats);
    g_crystal_pickup_context = result;
    if (result->result == CRYSTAL_PICKUP_NONE || result->result == CRYSTAL_PICKUP_NOTHING) {
        system_report_stub(0x11, 7);
    }
    /* Reload the published context after the call; reusing result changes the frame and registers. */
    if (g_crystal_pickup_context->result != CRYSTAL_PICKUP_TREASURE) {
        return -1;
    }
    return g_crystal_pickup_context->treasure_item;
}

/* Dispatch a battle menu system command (menu_system_command_e) for unit_id. The scenario
 * check may start an event (game over, a scenario condition's event, battle start, battle won);
 * with MENU_SCENARIO_CHECK_TEST it only records the event to run next (g_script_next_event_id). Each other
 * command starts one system function thread, a message or menu, after setting its text values.
 * Returns a MENU_SYSTEM_DISPATCH_* result and remembers the command and option. */
static s32 menu_dispatch_system_function(
    s32 command, s32 option, s32 unit_id, s32 parameter, s32 enabled, action_command_t* action) {
    unit_stats_t* unit;
    s32 value;
    s32 count;
    s32 i;
    const item_data_t* item;
    menu_system_function_t* entry;
    s32 flags;
    u8 item_id;

    g_menu_thread_menu_data = g_command_idle_action_entries;
    g_command_current_id = MENU_COMMAND_ID_NONE;
    g_menu_restore_pending = 0;
    unit = unit_get_stats(unit_id);
    if (command == MENU_SYSTEM_COMMAND_SCENARIO_CHECK) {
        value = script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT);
        if (value >= EVENT_ID_TUTORIAL_FIRST && value <= EVENT_ID_TUTORIAL_LAST) {
            if (option != MENU_SCENARIO_CHECK_TEST) {
                menu_start_system_function_thread(MENU_SYSTEM_FUNCTION_CLOSE);
            }
            return MENU_SYSTEM_DISPATCH_DONE;
        } else if (g_menu_input_mode != MENU_INPUT_PLAYER) {
            if (option != MENU_SCENARIO_CHECK_TEST) {
                menu_start_system_function_thread(MENU_SYSTEM_FUNCTION_CLOSE);
            }
            return MENU_SYSTEM_DISPATCH_DONE;
        } else if (option != MENU_SCENARIO_CHECK_SKIP) {
            if (thread_is_running_asm(THREAD_SLOT_EVENT) != 0) {
                system_report_stub(7, 3);
                if (option != MENU_SCENARIO_CHECK_TEST) {
                    menu_start_system_function_thread(MENU_SYSTEM_FUNCTION_CLOSE);
                }
                return MENU_SYSTEM_DISPATCH_DONE;
            } else {
                count = 0;
                g_scenario_condition_unit_battle_id = unit_id;
                for (i = 0; i < 3; i++) {
                    if (unit_find_slot_by_identity(i + 1) == UNIT_CHARACTER_SLOT_UNAVAILABLE) {
                        count++;
                    }
                }
                if ((event_check_battle_outcome() < 0 || count != 0)
                    && script_get_variable(EVENT_SCRIPT_VAR_BATTLE_NOT_STARTED) == 0) {
                    if (option != MENU_SCENARIO_CHECK_TEST) {
                        script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, EVENT_ID_GAME_OVER);
                        unit_view_start(UNIT_VIEW_MODE_NONE, 0xFF, 0xFF);
                        g_menu_overlay_state = 1;
                        g_scenario_event_active = 1;
                        script_start_current_event_thread();
                    } else {
                        g_script_next_event_id = EVENT_ID_GAME_OVER;
                    }
                    return MENU_SYSTEM_DISPATCH_EVENT;
                }
                if (g_scenario_event_active != 0) {
                    if (scenario_run_conditions() != 0) {
                        if (option != MENU_SCENARIO_CHECK_TEST) {
                            unit_view_start(UNIT_VIEW_MODE_NONE, 0xFF, 0xFF);
                            g_menu_overlay_state = 1;
                            script_start_current_event_thread();
                        } else {
                            g_script_next_event_id = script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT);
                            script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, value);
                        }
                        return MENU_SYSTEM_DISPATCH_EVENT;
                    }
                    if (option != MENU_SCENARIO_CHECK_TEST) {
                        menu_start_system_function_thread(MENU_SYSTEM_FUNCTION_CLOSE);
                    }
                    return MENU_SYSTEM_DISPATCH_DONE;
                } else if (script_get_variable(EVENT_SCRIPT_VAR_BATTLE_NOT_STARTED) != 0) {
                    if (option != MENU_SCENARIO_CHECK_TEST) {
                        unit_view_start(UNIT_VIEW_MODE_NONE, 0xFF, 0xFF);
                        g_menu_overlay_state = 1;
                        if (script_get_variable(EVENT_SCRIPT_VAR_WORLD_DEBUG_BATTLE_STYLE) != 0) {
                            script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, EVENT_ID_DEBUG_BATTLE_START);
                        } else {
                            script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, EVENT_ID_BATTLE_START);
                        }
                        script_start_current_event_thread();
                    } else {
                        g_script_next_event_id = EVENT_ID_BATTLE_START;
                    }
                    return MENU_SYSTEM_DISPATCH_EVENT;
                } else if (event_check_battle_outcome() == 0) {
                    if (option != MENU_SCENARIO_CHECK_TEST) {
                        unit_view_start(UNIT_VIEW_MODE_NONE, 0xFF, 0xFF);
                        g_menu_overlay_state = 1;
                        script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, EVENT_ID_BATTLE_WON);
                        g_scenario_event_active = 1;
                        script_start_current_event_thread();
                    } else {
                        g_script_next_event_id = EVENT_ID_BATTLE_WON;
                    }
                    return MENU_SYSTEM_DISPATCH_EVENT;
                } else {
                    if (option != MENU_SCENARIO_CHECK_TEST) {
                        menu_start_system_function_thread(MENU_SYSTEM_FUNCTION_CLOSE);
                    }
                    return MENU_SYSTEM_DISPATCH_DONE;
                }
            }
        }
        menu_start_system_function_thread(MENU_SYSTEM_FUNCTION_CLOSE);
        return MENU_SYSTEM_DISPATCH_DONE;
    }
    if (enabled == 0) {
        menu_start_system_function_thread(MENU_SYSTEM_FUNCTION_CLOSE);
        return MENU_SYSTEM_DISPATCH_DONE;
    }
    if (command == MENU_SYSTEM_COMMAND_TARGETING_PROMPT) {
        value = 8;
        if (g_command_resolved_command == 0 || g_command_resolved_command == 0x19) {
            value = 7;
            if (option == MENU_TARGETING_PROMPT_NO_MOVE_RANGE) {
                value = 0x31;
            }
        } else if (g_command_resolved_command == 1) {
            g_menu_restore_pending = 1;
            value = 6;
            if (option == MENU_TARGETING_PROMPT_NO_TARGET) {
                value = 0x29;
            }
        } else if (g_command_resolved_command == 0xC) {
            value = 0x14;
        } else if (g_command_resolved_command == 0xE) {
            value = 0x15;
        } else if (g_command_used_skillset_id == SKILLSET_ID_DEFEND) {
            value = 0x1C;
        }
        menu_start_system_function_thread(value);
    } else if (command == MENU_SYSTEM_COMMAND_MOVE_FEEDBACK) {
        sound_queue_sfx_confirm();
        if (option == MENU_MOVE_FEEDBACK_CLOSE) {
            menu_start_system_function_thread(MENU_SYSTEM_FUNCTION_CLOSE);
        } else if (option == MENU_MOVE_FEEDBACK_MOUNT) {
            menu_start_system_function_thread(0);
        } else if (option == MENU_MOVE_FEEDBACK_CRYSTAL) {
            menu_start_system_function_thread(0x30);
        } else {
            sound_queue_sfx_invalid();
            menu_start_system_function_thread(option + 0xD);
        }
    } else if (command == MENU_SYSTEM_COMMAND_WAIT_MENU) {
        if (g_menu_system_function_command == MENU_SYSTEM_COMMAND_MOVE_FEEDBACK
            && g_menu_system_function_option == MENU_MOVE_FEEDBACK_MOUNT) {
            menu_start_system_function_thread(MENU_SYSTEM_FUNCTION_CLOSE);
        } else {
            menu_start_system_function_thread(MENU_SYSTEM_FUNCTION_WAIT_MENU);
        }
    } else if (command == MENU_SYSTEM_COMMAND_ACTION_CONFIRM) {
        value = 0x2F;
        if (option != 5) {
            value = 0xB;
            if (option != 1) {
                if (option == 2 || option == 4) {
                    if (g_command_used_item_id == MENU_SELECTION_NONE || g_command_selected_skill_ct == 0) {
                        value = 0x2D;
                    } else {
                        value = 0xC;
                    }
                } else {
                    if (g_command_used_item_id != MENU_SELECTION_NONE || g_command_selected_skill_ct == 0) {
                        value = 0x2E;
                    } else {
                        value = 0xD;
                    }
                }
            }
        }
        g_menu_restore_pending = 1;
        menu_start_system_function_thread(value);
    } else if (command == MENU_SYSTEM_COMMAND_SPELL_QUOTE) {
        g_announcement_spell_quote_last_ability_id = action->ability_id & ABILITY_ID_MASK;
        thread_start(THREAD_SLOT_SYSTEM_FUNCTION, announcement_spell_quote_thread);
        thread_set_parameters(THREAD_SLOT_SYSTEM_FUNCTION, unit_id, g_announcement_spell_quote_last_ability_id, 0);
    } else if (command == MENU_SYSTEM_COMMAND_ABILITY_ANNOUNCEMENT) {
        message_set_duration(60);
        if (g_command_menu_types_by_skillset[unit->action_command.skillset_id] == ACTION_MENU_TYPE_ARITHMETICKS) {
            g_text_substitution_values[1] = unit->action_command.calculator_multiplier + TEXT_ID_ABILITY_NAME_BASE;
            g_text_substitution_values[0] = unit->action_command.calculator_type + TEXT_ID_ABILITY_NAME_BASE;
            g_text_substitution_values[2] = unit->action_command.ability_id + TEXT_ID_ABILITY_NAME_BASE;
            menu_start_system_function_thread(MENU_SYSTEM_FUNCTION_ARITHMETICKS_ANNOUNCEMENT);
        } else {
            if (action->skillset_id == SKILLSET_ID_ITEM || action->skillset_id == SKILLSET_ID_THROW) {
                item_id = action->item_id;
                g_text_substitution_values[0] = item_id + TEXT_ID_ITEM_NAME_BASE;
            } else {
                value = action->ability_id & ABILITY_ID_MASK;
                if (value >= ABILITY_ID_SUMMON_MAGIC_MOOGLE && value <= ABILITY_ID_DRAW_OUT_CHIRIJIRADEN) {
                    g_text_substitution_values[0] = value + 0xB7C4;
                } else {
                    g_text_substitution_values[0] = value + TEXT_ID_ABILITY_NAME_BASE;
                }
            }
            menu_start_system_function_thread(MENU_SYSTEM_FUNCTION_ABILITY_ANNOUNCEMENT);
        }
    } else if (command == MENU_SYSTEM_COMMAND_CLOSE) {
        menu_start_system_function_thread(MENU_SYSTEM_FUNCTION_CLOSE);
    } else if (command == MENU_SYSTEM_COMMAND_TARGET_CONFIRM) {
        value = 8;
        if (option == 1) {
            value = 0x16;
        } else if (option == 2) {
            value = 0x17;
        } else if (option == 3) {
            value = 0x20;
        } else if (g_command_resolved_command == 0xC) {
            value = 0x1A;
        } else if (g_command_resolved_command == 0xE) {
            value = 0x1B;
        }
        menu_start_system_function_thread(value);
    } else if (command == MENU_SYSTEM_COMMAND_EFFECT_MESSAGE) {
        if (g_game_options.fields.effect_messages != GAME_OPTION_ON) {
            menu_start_system_function_thread(MENU_SYSTEM_FUNCTION_CLOSE);
        } else {
            g_menu_system_function_table[MENU_SYSTEM_FUNCTION_EFFECT_MESSAGE].text_id = option;
            g_text_substitution_values[0] = parameter & 0xFF;
            menu_start_system_function_thread(MENU_SYSTEM_FUNCTION_EFFECT_MESSAGE);
        }
    } else if (command == MENU_SYSTEM_COMMAND_ABILITY_LEARNED) {
        g_text_substitution_values[0] = option + TEXT_ID_ABILITY_NAME_BASE;
        menu_start_system_function_thread(MENU_SYSTEM_FUNCTION_ABILITY_LEARNED);
    } else if (command == MENU_SYSTEM_COMMAND_CRYSTAL_OR_TREASURE) {
        menu_start_system_function_thread(MENU_SYSTEM_FUNCTION_CRYSTAL_OR_TREASURE);
    } else if (command == MENU_SYSTEM_COMMAND_MOVE_CONFIRM) {
        menu_start_system_function_thread(MENU_SYSTEM_FUNCTION_MOVE_CONFIRM);
    } else if (command == MENU_SYSTEM_COMMAND_TRAP_RESULT) {
        menu_start_system_function_thread(option + MENU_SYSTEM_FUNCTION_TRAP_RESULT_FIRST);
    } else if (command == MENU_SYSTEM_COMMAND_MOVE_FIND_ITEM) {
        /* With 99 already held, the found item is sold for a quarter of its price instead. The
         * discarded data pointer lookups are retail. */
        if (item_get_total_quantity(option, 1) == 0x63) {
            item_get_data(option);
            g_text_substitution_values[0] = option;
            g_text_substitution_values[1] = option;
            item = item_get_data(option);
            g_text_substitution_values[2] = item->price >> 2;
            script_set_variable(
                EVENT_SCRIPT_VAR_WAR_FUNDS, (item->price >> 2) + script_get_variable(EVENT_SCRIPT_VAR_WAR_FUNDS));
            menu_start_system_function_thread(MENU_SYSTEM_FUNCTION_MOVE_FIND_ITEM_SOLD);
        } else {
            item_get_data(option);
            g_text_substitution_values[0] = option;
            g_item_quantities[option]++;
            menu_start_system_function_thread(MENU_SYSTEM_FUNCTION_MOVE_FIND_ITEM);
        }
    } else if (command == MENU_SYSTEM_COMMAND_MOVE_CONFIRM_SILENT) {
        menu_start_system_function_thread(MENU_SYSTEM_FUNCTION_MOVE_CONFIRM_SILENT);
    } else if (command == MENU_SYSTEM_COMMAND_POST_ACTION_RESULT) {
        entry = &g_menu_system_function_table[MENU_SYSTEM_FUNCTION_POST_ACTION_RESULT];
        flags = parameter;
        if (option >= ACTION_DISPLAY_IMAGE_CT && option <= ACTION_DISPLAY_IMAGE_LEVEL) {
            g_text_substitution_values[0] = unit_id;
            g_text_substitution_values[1] = option + 0x506B;
            if (option == ACTION_DISPLAY_IMAGE_LEVEL) {
                flags = 1;
                if (parameter & ACTION_STAT_CHANGE_INCREASE) {
                    flags = 1 | ACTION_STAT_CHANGE_INCREASE;
                }
            }
            g_text_substitution_values[2] = flags;
            if (flags & ACTION_STAT_CHANGE_INCREASE) {
                g_text_substitution_values[2] = flags & ACTION_STAT_CHANGE_VALUE_MASK;
                entry->text_id = 0x508C;
            } else {
                entry->text_id = 0x508D;
            }
            if (option == ACTION_DISPLAY_IMAGE_CT) {
                if (flags == 0xFF) {
                    entry->text_id = 0x509A;
                } else if (flags == 0x7F) {
                    entry->text_id = 0x5099;
                }
            }
            menu_start_system_function_thread(MENU_SYSTEM_FUNCTION_POST_ACTION_RESULT);
        } else if (option == ACTION_DISPLAY_IMAGE_GIL) {
            g_text_substitution_values[0] = parameter < 0 ? -parameter : parameter;
            g_menu_system_function_table[MENU_SYSTEM_FUNCTION_POST_ACTION_RESULT].text_id = 0x508F;
            menu_start_system_function_thread(MENU_SYSTEM_FUNCTION_POST_ACTION_RESULT);
        } else if (option == ACTION_DISPLAY_IMAGE_EXP) {
            g_text_substitution_values[0] = parameter & 0x7F;
            g_menu_system_function_table[MENU_SYSTEM_FUNCTION_POST_ACTION_RESULT].text_id = 0x508E;
            menu_start_system_function_thread(MENU_SYSTEM_FUNCTION_POST_ACTION_RESULT);
        } else if (option == ACTION_DISPLAY_IMAGE_STOLEN || option == ACTION_DISPLAY_IMAGE_BROKEN) {
            if (parameter & 0x8000) {
                g_text_substitution_values[0] = (parameter & 0xFF) + TEXT_ID_ITEM_NAME_BASE;
                g_menu_system_function_table[MENU_SYSTEM_FUNCTION_POST_ACTION_RESULT].text_id = 0x509B;
            } else {
                g_text_substitution_values[0] = parameter + TEXT_ID_ITEM_NAME_BASE;
                g_menu_system_function_table[MENU_SYSTEM_FUNCTION_POST_ACTION_RESULT].text_id = option + 0x5074;
            }
            menu_start_system_function_thread(MENU_SYSTEM_FUNCTION_POST_ACTION_RESULT);
        } else {
            menu_start_system_function_thread(MENU_SYSTEM_FUNCTION_CLOSE);
        }
    } else {
        system_report_stub(7, 5);
        menu_start_system_function_thread(MENU_SYSTEM_FUNCTION_CLOSE);
        g_menu_system_function_command = command;
        g_menu_system_function_option = option;
        return MENU_SYSTEM_DISPATCH_UNKNOWN_COMMAND;
    }
    g_menu_system_function_command = command;
    g_menu_system_function_option = option;
    return MENU_SYSTEM_DISPATCH_DONE;
}

/* Resolves an event unit id to battle stats for the scenario conditions; returns
 * -1 for none.
 *
 * An id found among the ENTD unit ids gives that unit when it has a unit record.
 * Otherwise an id in the special-character identity range is looked up by
 * character identity and accepted only for a deployed party member (unit id
 * 0x78..0x7c). */
static unit_stats_t* unit_get_stats_by_unit_id(s32 unit_id) {
    s32 index;
    unit_stats_t* stats;

    unit_entd_find_stats(unit_id, &index);
    if (index < 0) {
        if (unit_id < CHARACTER_IDENTITY_RAMZA_CHAPTER_1 || unit_id >= CHARACTER_IDENTITY_GENERIC_FIRST) {
            return (unit_stats_t*)-1;
        }
        index = unit_find_slot_by_identity(unit_id);
        if (index >= 0) {
            stats = unit_get_stats(index);
            if (stats->unit_id >= EVENT_UNIT_ID_DEPLOYED_FIRST && stats->unit_id < EVENT_UNIT_ID_DEPLOYED_END) {
                return stats;
            }
        }
        return (unit_stats_t*)-1;
    }
    if (unit_get_runtime_id_by_battle_id(index) >= 0) {
        return unit_get_stats(index);
    }
    return (unit_stats_t*)-1;
}

/* Walk the scenario's condition chains and report whether one selects an event.
 *
 * The runner inspects the first ten of sixteen byte offsets into
 * g_scenario_condition_data, one per chain; a zero offset after the first
 * ends the list. A chain is a run of records, each a 16-bit type followed by
 * g_scenario_condition_param_lengths[type] 16-bit operands. Records are
 * evaluated while they pass and the first failure drops the chain; the RUN_EVENT
 * record that closes a chain stores its event id in the current-event variable,
 * and the function returns 1. It returns 0 when no chain gets that far.
 *
 * The checker always receives the five 16-bit values at the record, whatever the
 * type's length. Called at EventEnd and by the scenario-check command of
 * the scene's menu system-function dispatcher. */
s32 scenario_run_conditions(void) {
    s16* condition;
    s32 i;
    s32 shifted_offset;
    s32 offset;
    s32 result;

    for (i = 0; i < 10; i++) {
        /* The entry is a 16-bit byte offset into the script area; the
         * target sign-extends it with a live `<< 16` value, taking the
         * division's sign bit from that value rather than from the
         * sign-extended one. Spelled as `offset = (s16)g_scenario_condition_offsets[i]` with
         * `offset / 2`, combine folds the shift pair into a single `lh` and the
         * function comes out three instructions short, so the shift pair
         * and the by-two division are spelled out here. */
        shifted_offset = g_scenario_condition_offsets[i] << 16;
        offset = shifted_offset >> 16;
        if (i != 0 && offset == 0) {
            return 0;
        }
        condition = &g_scenario_condition_data[(offset + (s32)((u32)shifted_offset >> 31)) >> 1];
        do {
            result = scenario_check_condition(condition[0], condition[1], condition[2], condition[3], condition[4]);
            condition += g_scenario_condition_param_lengths[condition[0]] + 1;
        } while (result == EVENT_SCENARIO_CONDITION_RESULT_PASS);
        if (result == EVENT_SCENARIO_CONDITION_RESULT_RUN_EVENT) {
            return 1;
        }
    }
    return 0;
}

/* Empty in BATTLE and WORLD; in BATTLE it sits between
 * scenario_run_conditions and scenario_check_condition. Nothing
 * on the disc calls it or points at it; the name is a neutral placeholder
 * until a reference turns up. */
void script_noop_9(void) {
}

/* Evaluate one scenario condition record for
 * scenario_run_conditions.
 *
 * The record's 16-bit values arrive as type (event_scenario_condition_e), id and
 * value; the two tile conditions also read a y and an elevation, with value as
 * the x. Returns EVENT_SCENARIO_CONDITION_RESULT_PASS when the condition holds,
 * RESULT_RUN_EVENT for type RUN_EVENT after storing id as the current event, and
 * RESULT_FAIL otherwise, including for every type that has no test here.
 *
 * ACTING_UNIT looks at the unit the check runs for
 * (g_scenario_condition_unit_battle_id): it passes when that unit's unit id
 * is id, or when it is a deployed unit whose character identity is id and id is a
 * named (non-generic) identity. While the battle id is 0xff the condition fails.
 * ITEM_EQUIPPED searches the seven equipment slots of the units with unit ids 1
 * to 3. The HP percentage is hp * 100 / (max_hp + 1). The casualty conditions
 * compare with id, not value.
 *
 * Keep the two unit-id range comparisons: retail reloads the byte. */
static event_scenario_condition_result_e scenario_check_condition(
    event_scenario_condition_e type, s32 id, s32 value, s32 tile_y, s32 elevation) {
    unit_stats_t* unit;
    s32 i;
    s32 j; /* equipment slot, then battle index: one variable (a0 in both loops) */

    if (type == EVENT_SCENARIO_CONDITION_VARIABLE_EQUAL && script_get_variable(id) == value)
        return EVENT_SCENARIO_CONDITION_RESULT_PASS;
    if (type == EVENT_SCENARIO_CONDITION_VARIABLE_AT_LEAST && script_get_variable(id) >= value)
        return EVENT_SCENARIO_CONDITION_RESULT_PASS;
    if (type == EVENT_SCENARIO_CONDITION_VARIABLE_AT_MOST && value >= script_get_variable(id))
        return EVENT_SCENARIO_CONDITION_RESULT_PASS;
    if (type >= EVENT_SCENARIO_CONDITION_UNIT_PRESENT && type <= EVENT_SCENARIO_CONDITION_UNIT_MP_AT_MOST) {
        unit = unit_get_stats_by_unit_id(id);
        if (unit != (unit_stats_t*)-1) {
            if (type == EVENT_SCENARIO_CONDITION_UNIT_PRESENT)
                return EVENT_SCENARIO_CONDITION_RESULT_PASS;
            if (type == EVENT_SCENARIO_CONDITION_UNIT_HP_AT_LEAST && unit->hp >= value)
                return EVENT_SCENARIO_CONDITION_RESULT_PASS;
            if (type == EVENT_SCENARIO_CONDITION_UNIT_HP_AT_MOST && value >= unit->hp)
                return EVENT_SCENARIO_CONDITION_RESULT_PASS;
            if (type == EVENT_SCENARIO_CONDITION_UNIT_HP_PERCENT_AT_LEAST
                && unit->hp * 100 / (unit->max_hp + 1) >= value)
                return EVENT_SCENARIO_CONDITION_RESULT_PASS;
            if (type == EVENT_SCENARIO_CONDITION_UNIT_HP_PERCENT_AT_MOST
                && value >= unit->hp * 100 / (unit->max_hp + 1))
                return EVENT_SCENARIO_CONDITION_RESULT_PASS;
            if (type == EVENT_SCENARIO_CONDITION_UNIT_MP_AT_LEAST && unit->mp >= value)
                return EVENT_SCENARIO_CONDITION_RESULT_PASS;
            if (type == EVENT_SCENARIO_CONDITION_UNIT_MP_AT_MOST && value >= unit->mp)
                return EVENT_SCENARIO_CONDITION_RESULT_PASS;
        }
    }
    if (type == EVENT_SCENARIO_CONDITION_ACTING_UNIT && g_scenario_condition_unit_battle_id != 0xff) {
        unit = unit_get_stats(g_scenario_condition_unit_battle_id);
        if (unit->unit_id == id
            || (unit->unit_id >= EVENT_UNIT_ID_DEPLOYED_FIRST && unit->unit_id < EVENT_UNIT_ID_DEPLOYED_END
                && unit->character_identity == id && id >= 0 && id < CHARACTER_IDENTITY_GENERIC_FIRST
                && id != CHARACTER_IDENTITY_ENTD_NONE))
            return EVENT_SCENARIO_CONDITION_RESULT_PASS;
        return EVENT_SCENARIO_CONDITION_RESULT_FAIL;
    }
    if (type == EVENT_SCENARIO_CONDITION_ITEM_EQUIPPED) {
        for (i = 1; i < 4; i++) {
            unit = unit_get_stats_by_unit_id(i);
            if (unit != (unit_stats_t*)-1) {
                for (j = 0; j < 7; j++) {
                    if (unit->equipment[j] == id)
                        return EVENT_SCENARIO_CONDITION_RESULT_PASS;
                }
            }
        }
    }
    if (type == EVENT_SCENARIO_CONDITION_WAR_FUNDS_AT_LEAST && script_get_variable(EVENT_SCRIPT_VAR_WAR_FUNDS) >= value)
        return EVENT_SCENARIO_CONDITION_RESULT_PASS;
    if (type == EVENT_SCENARIO_CONDITION_WAR_FUNDS_AT_MOST && value >= script_get_variable(EVENT_SCRIPT_VAR_WAR_FUNDS))
        return EVENT_SCENARIO_CONDITION_RESULT_PASS;
    if (type == EVENT_SCENARIO_CONDITION_DATE_ON_OR_BEFORE) {
        if (script_get_variable(EVENT_SCRIPT_VAR_MONTH) < id)
            return EVENT_SCENARIO_CONDITION_RESULT_PASS;
        if (script_get_variable(EVENT_SCRIPT_VAR_MONTH) == id && value >= script_get_variable(EVENT_SCRIPT_VAR_DAY))
            return EVENT_SCENARIO_CONDITION_RESULT_PASS;
    }
    if (type == EVENT_SCENARIO_CONDITION_DATE_ON_OR_AFTER) {
        if (id < script_get_variable(EVENT_SCRIPT_VAR_MONTH))
            return EVENT_SCENARIO_CONDITION_RESULT_PASS;
        if (script_get_variable(EVENT_SCRIPT_VAR_MONTH) == id && script_get_variable(EVENT_SCRIPT_VAR_DAY) >= value)
            return EVENT_SCENARIO_CONDITION_RESULT_PASS;
    }
    if (type == EVENT_SCENARIO_CONDITION_CASUALTIES_AT_LEAST && script_get_variable(EVENT_SCRIPT_VAR_CASUALTIES) >= id)
        return EVENT_SCENARIO_CONDITION_RESULT_PASS;
    if (type == EVENT_SCENARIO_CONDITION_CASUALTIES_AT_MOST && id >= script_get_variable(EVENT_SCRIPT_VAR_CASUALTIES))
        return EVENT_SCENARIO_CONDITION_RESULT_PASS;
    if (type == EVENT_SCENARIO_CONDITION_ENEMIES_DEFEATED && event_check_battle_outcome() == 0)
        return EVENT_SCENARIO_CONDITION_RESULT_PASS;
    if (type == EVENT_SCENARIO_CONDITION_UNIT_AT_TILE) {
        unit = unit_get_stats_by_unit_id(id);
        if (unit != (unit_stats_t*)-1 && unit->x == value && unit->position.y == tile_y
            && unit->position.higher_elevation == elevation)
            return EVENT_SCENARIO_CONDITION_RESULT_PASS;
    }
    if (type == EVENT_SCENARIO_CONDITION_PALETTE_UNIT_AT_TILE) {
        for (i = 0; i < UNIT_SLOT_COUNT; i++) {
            if (unit_has_runtime_id(i)) {
                j = unit_get_battle_id_by_runtime_id(i);
                if (j != -1) {
                    unit = unit_get_stats(j);
                    if (unit->x == value && unit->position.y == tile_y && unit->position.higher_elevation == elevation
                        && unit->sprite_palette == id)
                        return EVENT_SCENARIO_CONDITION_RESULT_PASS;
                }
            }
        }
    }
    if (type == EVENT_SCENARIO_CONDITION_RUN_EVENT) {
        script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, id);
        return EVENT_SCENARIO_CONDITION_RESULT_RUN_EVENT;
    }
    return EVENT_SCENARIO_CONDITION_RESULT_FAIL;
}

/* Return the current event's 16-bit finish-operation value, or 0 while no scenario event
 * is active. Bits 0-7 and 10-11 hold the next scenario id and the remaining bits
 * the after-event code. */
s32 scenario_get_finish_operation(void) {
    if (g_scenario_event_active == 0) {
        return 0;
    }
    return g_scenario_event_finish_operations[script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT)];
}

/* Return the after-event code of the pending event (g_script_next_event_id): bits
 * 8-9 and 12-15 of its 16-bit finish-operation value, shifted down by 8. Zero means the
 * event has none. */
u32 script_get_event_finish_operation(void) {
    return (g_scenario_event_finish_operations[g_script_next_event_id] & 0xF300) >> 8;
}
