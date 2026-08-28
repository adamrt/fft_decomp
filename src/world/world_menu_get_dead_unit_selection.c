#include "fft/battle.h"
#include "fft/main_runtime.h"
#include "fft/world.h"

/* Declared as in the BATTLE callers; WORLD reads the result through
 * world_crystal_pickup_result_t. */
typedef struct crystal_pickup_result crystal_pickup_result_t;
extern crystal_pickup_result_t* battle_unit_generate_crystal_or_treasure(battle_stats_t* mover);

/* Run battle_unit_generate_crystal_or_treasure for the unit on the main thread
 * and return the treasure item when the result is 4, else -1. */
s32 world_menu_get_dead_unit_selection(s32 battle_unit_index) {
    battle_stats_t* unit = battle_unit_get_stats_from_battle_id(battle_unit_index);

    g_world_thread_inner_subroutine_callback = (void (*)(void))battle_unit_generate_crystal_or_treasure;
    g_world_thread_inner_subroutine_result = (world_crystal_pickup_result_t*)world_thread_call_on_main_stack(unit);
    if ((u32)(g_world_thread_inner_subroutine_result->result + 1) < 2) {
        main_noop_800449f8(0x11, 7);
    }
    if (g_world_thread_inner_subroutine_result->result == 4) {
        return g_world_thread_inner_subroutine_result->treasure_item;
    }
    return -1;
}
