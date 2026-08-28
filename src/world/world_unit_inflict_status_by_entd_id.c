#include "fft/battle.h"
#include "fft/event.h"
#include "fft/world.h"

void world_unit_inflict_status_by_entd_id(
    s32 entd_unit_id, s32 status_index, s32 is_infliction, s32 less_strict_removal) {
    s32 battle_unit_index;
    battle_stats_t* unit;
    s32 clear_index;
    s32 status_byte_index;
    s32 status_bit_index;
    u8 status_mask;

    battle_unit_index = world_unit_get_battle_index_by_entd_id(entd_unit_id);
    if (battle_unit_index != 2000) {
        unit = battle_unit_get_stats_from_battle_id(battle_unit_index);
        clear_index = 0;
        do {
            unit->action.status_infliction[clear_index] = 0;
            unit->action.status_removal[clear_index] = 0;
            clear_index++;
        } while (clear_index < BATTLE_STATUS_BYTE_COUNT);

        /* Action infliction/removal bytes use the event VM's LSB-first order,
         * unlike the stored status-set masks. */
        status_byte_index = status_index / 8;
        status_bit_index = status_index - status_byte_index * 8;
        status_mask = 1 << status_bit_index;
        if (is_infliction != 0) {
            unit->action.status_infliction[status_byte_index] = status_mask;
        } else {
            unit->action.status_removal[status_byte_index] = status_mask;
        }
        g_world_thread_inner_subroutine_callback = resolve_unit_status_changes;
        world_thread_call_on_main_stack(battle_unit_index, less_strict_removal);
    }
}
