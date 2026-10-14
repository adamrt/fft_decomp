#include "fft/battle.h"
#include "fft/menu.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "psx/types.h"

/* Run battle_unit_generate_crystal_or_treasure for the unit on the main stack,
 * keep its record in g_battle_dead_unit_context and return the treasure item when
 * the result is 4, otherwise -1. */
s32 menu_get_dead_unit_selection(s32 battle_id) {
    unit_stats_t* stats;
    crystal_pickup_result_t* result;

    stats = battle_unit_get_stats_from_battle_id(battle_id);
    g_thread_call_target = (void (*)(void))battle_unit_generate_crystal_or_treasure;
    result = (crystal_pickup_result_t*)thread_call_on_main_stack(stats);
    g_menu_dead_unit_context = result;
    if (result->result == -1 || result->result == 0) {
        main_system_report_stub(0x11, 7);
    }
    /* Reload the published context after the call; reusing result changes the frame and registers. */
    if (g_menu_dead_unit_context->result != 4) {
        return -1;
    }
    return g_menu_dead_unit_context->treasure_item;
}
