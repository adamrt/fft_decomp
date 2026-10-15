#include "fft/battle.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "psx/types.h"

/* Thread entry of menu entry 0x22, which confirms the Move command.
 *
 * A unit with Don't Move gets the invalid sound and the linked-entry icon thread
 * (stored row action -1). In a menu whose move is already used (ids 0x0e, 0x14,
 * 0x21, 0x2f, 0x31 and 0x33) only the invalid sound plays. Otherwise the command
 * is confirmed at once, as a silent Circle press. */
void menu_confirm_move_command_thread(void) {
    s16* stored_actions;
    menu_entry_t* entry;
    s32 menu;

    entry = (menu_entry_t*)thread_get_current_parameter_1_variant();
    stored_actions = &g_menu_confirm_stored_unit_row_actions;
    *stored_actions = -2;
    if (menu_get_selected_unit_stats()->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DONT_MOVE)]
        & STATUS_BYTE_MASK(STATUS_ID_DONT_MOVE)) {
        *stored_actions = -1;
        sound_queue_sfx_invalid();
        menu_icon_linked_entry_thread();
        thread_exit_current();
    }
    menu = g_menu_current_id;
    if (menu == BATTLE_MENU_ID_TURN_MOVED || menu == BATTLE_MENU_ID_TURN_MOVED_ACTED
        || menu == BATTLE_MENU_ID_TURN_NO_ACT_MOVED || menu == BATTLE_MENU_ID_RIDER_TURN_MOVED
        || menu == BATTLE_MENU_ID_RIDER_TURN_MOVED_ACTED || menu == BATTLE_MENU_ID_RIDER_TURN_NO_ACT_MOVED) {
        sound_queue_sfx_invalid();
        thread_exit_current();
    }
    sound_queue_sfx_confirm_unless_restoring();
    menu_confirm_action_silently(entry);
    thread_exit_current();
}
