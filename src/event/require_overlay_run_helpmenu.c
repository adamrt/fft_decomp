#include "fft/battle.h"
#include "fft/helpmenu.h"
#include "fft/main_heap.h"
#include "fft/main_sound.h"
#include "fft/require.h"
#include "fft/thread.h"
#include "psx/types.h"

void require_overlay_run_helpmenu(void) {
    s32 thread_id;

    battle_thread_set_current_task_id(NATIVE_THREAD_TASK_HELP_MENU);
    g_sound_effect_id_to_play = MAIN_SFX_CONFIRM;
    g_require_panel_status_animation[0] = 1;
    g_require_panel_status_animation[1] = 1;
    while (
        main_heap_reserve_at(g_main_heap_high_overlay_load_address, 0x10000) != g_main_heap_high_overlay_load_address) {
        battle_thread_yield();
    }
    battle_menu_request_open_companion_executable(3);
    thread_id = battle_thread_resolve_id(0x10);
    battle_thread_start(thread_id, helpmenu_menu_run_require_help);
    battle_thread_wait_until_inactive(thread_id);
    g_require_panel_status_animation[0] = 0;
    g_require_panel_status_animation[1] = 0;
    battle_thread_exit_current();
}
