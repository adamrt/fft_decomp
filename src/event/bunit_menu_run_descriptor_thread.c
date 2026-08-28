#include "fft/bunit.h"
#include "psx/types.h"

/* Returns s32: the value is the 0/1 flag g_bunit_menu_thread_running, and
 * both call sites declare and use an s32 result. */
s32 bunit_menu_run_descriptor_thread(s32 id, world_menu_entry_t* desc) {
    if (g_bunit_menu_thread_running == 0) {
        if (battle_thread_is_running(id) != 0) {
            return 1;
        }
        g_battle_menu_thread_menu_data = desc;
        battle_thread_start(id, desc->thread_entry);
        battle_thread_set_parameters(id, (s32)g_battle_menu_thread_menu_data, 0, 0);
        g_bunit_menu_thread_running = 1;
    }
    g_bunit_menu_thread_running = battle_thread_is_running(id);
    return g_bunit_menu_thread_running;
}
