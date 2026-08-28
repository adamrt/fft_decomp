#include "fft/battle.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

void world_menu_confirm_stored_unit_thread(void) {
    s32 entry = world_thread_get_current_parameter_1();
    s32 menu_id;
    s16* result = &g_world_menu_confirm_stored_unit_row_actions;

    *result = -2;
    if (world_unit_get_battle_stats_for_stored()->status_sets.current[4]
        & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_DONT_MOVE)) {
        *result = -1;
        world_sound_set_effect_to_invalid();
        world_menu_icon_linked_entry_thread();
        world_thread_exit_current();
    }
    menu_id = g_world_menu_current_id;
    if (menu_id == 0xE || menu_id == 0x14 || menu_id == 0x21 || menu_id == 0x2F || menu_id == 0x31 || menu_id == 0x33) {
        world_sound_set_effect_to_invalid();
        world_thread_exit_current();
    }
    world_sound_set_effect_to_confirm_checked();
    world_menu_confirm_action_silently(entry);
    world_thread_exit_current();
}
