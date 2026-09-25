#include "fft/event_require.h"
#include "psx/types.h"

void require_condition_show_ready_bugged(void) {
    s32 thread_id = battle_thread_resolve_id(0x10);

    battle_thread_start(thread_id, battle_menu_refresh_hovered_unit_stats_display);
    battle_thread_set_parameters(thread_id, 0, 0, 0);
    require_input_wait_frames_or_skip(0x3c);
    battle_thread_set_parameters(thread_id, 0, 0, 1);
    battle_thread_wait_until_inactive(thread_id);
    battle_thread_exit_current();
}
