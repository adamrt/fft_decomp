#include "fft/sound.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

/*
 * Menu thread for the "Move" command row.
 *
 * A unit with Don't Move stores row action -1, plays the invalid sound and
 * falls back to the linked entry. The menus of a unit that has already moved
 * also refuse with the invalid sound. Otherwise the entry is confirmed.
 */
void world_menu_confirm_move_command_thread(void) {
    menu_entry_t* entry = (menu_entry_t*)world_thread_get_current_parameter_1();
    s32 menu_id;
    s16* result = &g_world_menu_confirm_stored_unit_row_actions;

    *result = -2;
    if (world_unit_get_active_stats()->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DONT_MOVE)]
        & STATUS_BYTE_MASK(STATUS_ID_DONT_MOVE)) {
        *result = -1;
        sound_queue_sfx_invalid();
        world_menu_icon_linked_entry_thread();
        thread_exit_current();
    }
    menu_id = g_menu_current_id;
    if (menu_id == BATTLE_MENU_ID_TURN_MOVED || menu_id == BATTLE_MENU_ID_TURN_MOVED_ACTED
        || menu_id == BATTLE_MENU_ID_TURN_NO_ACT_MOVED || menu_id == BATTLE_MENU_ID_RIDER_TURN_MOVED
        || menu_id == BATTLE_MENU_ID_RIDER_TURN_MOVED_ACTED || menu_id == BATTLE_MENU_ID_RIDER_TURN_NO_ACT_MOVED) {
        sound_queue_sfx_invalid();
        thread_exit_current();
    }
    sound_queue_sfx_confirm_unless_restoring();
    menu_confirm_action_silently(entry);
    thread_exit_current();
}
