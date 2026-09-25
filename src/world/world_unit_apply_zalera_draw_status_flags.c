#include "fft/battle.h"
#include "fft/world.h"

void world_unit_apply_zalera_draw_status_flags(s32 battle_unit_index, s32 enabled) {
    battle_stats_t* unit;
    s32 status_index;
    s32 byte_index;
    s32 bit_index;

    unit = battle_unit_get_stats_from_battle_id(battle_unit_index);
    g_world_thread_inner_subroutine_callback = (void (*)(void))battle_status_queue_misc_graphics_flag_change;
    status_index = 0;
    do {
        byte_index = status_index / 8;
        bit_index = status_index - byte_index * 8;
        if ((unit->status_sets.current[byte_index] & (0x80 >> bit_index)) != 0) {
            world_thread_call_on_main_stack(BATTLE_STATUS_HANDLER_INDEX(status_index), enabled, battle_unit_index);
        }
        status_index++;
    } while (status_index < BATTLE_STATUS_COUNT);
}
