#include "fft/wldcore.h"
#include "psx/types.h"

/* g_wldcore_thread8_offset_y aliases g_wldcore_thread8_params.y. Referencing it through
 * the block symbol makes GCC base that block's address register on +4; the
 * target bases it on +0 and spells this store absolutely, so the word keeps
 * its own symbol here. */
void wldcore_start_core_background_threads(s32 party_index) {
    wldcore_unit_build_status_panel_data(party_index);
    g_wldcore_thread9_params.y = 0;
    g_wldcore_threadc_params.y = 0;
    g_wldcore_thread8_offset_y = 0;
    g_wldcore_thread8_params.x = 0;
    world_thread_start(8, world_menu_unit_status_banner_thread);
    world_thread_set_parameters(8, (s32)&g_wldcore_thread8_params, 0, 0);
    world_thread_start(0xC, world_menu_run_numeric_editor_thread);
    world_thread_set_parameters(0xC, (s32)&g_wldcore_threadc_params, 0, 0);
    world_thread_start(9, world_menu_equipment_and_ability_panel_thread);
    world_thread_set_parameters(9, (s32)&g_wldcore_thread9_params, 0, 0);
    g_main_system_flags |= 0x20000;
}
