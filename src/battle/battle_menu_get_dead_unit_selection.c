#include "fft/battle.h"
#include "fft/option.h"
#include "fft/thread.h"
#include "psx/types.h"

/* Menu result record produced on the main thread stack by battle_unit_generate_crystal_or_treasure.
 * Only the status word and the selected entry are established here. */
typedef struct battle_menu_result {
    s32 status; /* 0x00: -1 cancelled, 0 idle, 4 selection available */
    u8 unknown_04[0x4e];
    u8 selection; /* 0x52 */
} battle_menu_result_t;

typedef struct crystal_pickup_result crystal_pickup_result_t;
extern crystal_pickup_result_t* battle_unit_generate_crystal_or_treasure(battle_stats_t* mover);

s32 battle_menu_get_dead_unit_selection(s32 battle_id) {
    battle_stats_t* stats;
    battle_menu_result_t* result;

    stats = battle_unit_get_stats_from_battle_id(battle_id);
    g_battle_thread_call_target = (void (*)(void))battle_unit_generate_crystal_or_treasure;
    result = (battle_menu_result_t*)battle_thread_call_on_main_stack(stats);
    g_dead_unit_context = (struct dead_unit_context*)result;
    if ((u32)(result->status + 1) < 2) {
        main_noop_800449f8(0x11, 7);
    }
    if (((battle_menu_result_t*)g_dead_unit_context)->status != 4) {
        return -1;
    }
    return ((battle_menu_result_t*)g_dead_unit_context)->selection;
}
