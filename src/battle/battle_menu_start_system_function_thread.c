#include "fft/battle.h"

enum {
    BATTLE_SYSTEM_FUNCTION_THREAD_ID = 4,
};

/* Start the fixed worker used by the system-function dispatcher.
 *
 * The dispatcher at 0x80141B0C calls this helper with command-specific
 * values. Thread 4 receives that value as its first parameter after the
 * shared idle-action menu flags are cleared. */
void battle_menu_start_system_function_thread(s32 system_function) {
    battle_menu_clear_idle_action_menu_entry_flags();
    battle_thread_start(BATTLE_SYSTEM_FUNCTION_THREAD_ID, battle_menu_run_system_function_thread);
    battle_thread_set_parameters(BATTLE_SYSTEM_FUNCTION_THREAD_ID, system_function, 0, 0);
}
