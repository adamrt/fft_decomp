#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/types.h"

/* Build the status panel data of party member `party_index` and start the
 * three unit panel threads on it (status banner, numeric editor, equipment
 * and ability panel), raising MAIN_SYSTEM_FLAG_UNIT_PANEL_THREADS. */
void wldcore_start_core_background_threads(s32 party_index) {
    menu_status_thread_t* thread8;

    wldcore_unit_build_status_panel_data(party_index);
    thread8 = &g_wldcore_thread8_params;
    g_wldcore_thread9_params.y = 0;
    g_wldcore_threadc_params.y = 0;
    g_wldcore_thread8_params.y = 0;
    thread8->x = 0;
    thread_start(THREAD_SCREEN_SLOT_UNIT_STATUS, menu_run_unit_status_thread_variant);
    thread_set_parameters(THREAD_SCREEN_SLOT_UNIT_STATUS, (s32)thread8, 0, 0);
    thread_start(THREAD_SCREEN_SLOT_NUMERIC_EDITOR, menu_run_numeric_editor_frame_thread_variant);
    thread_set_parameters(THREAD_SCREEN_SLOT_NUMERIC_EDITOR, (s32)&g_wldcore_threadc_params, 0, 0);
    thread_start(THREAD_SCREEN_SLOT_CHARACTER_STATUS, menu_run_equipment_ability_panel_thread);
    thread_set_parameters(THREAD_SCREEN_SLOT_CHARACTER_STATUS, (s32)&g_wldcore_thread9_params, 0, 0);
    g_main_system_flags |= MAIN_SYSTEM_FLAG_UNIT_PANEL_THREADS;
}
