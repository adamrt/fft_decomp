#include "fft/battle.h"
#include "fft/script_variables.h"
#include "fft/thread.h"
#include "psx/types.h"

void battle_unit_set_specific_animation_value_on_battle_init(s32 misc_id, u32 value) {
    s32 idx;
    battle_stats_t* stats;
    s32 depth;

    if ((battle_script_get_variable(EVENT_SCRIPT_VAR_PENDING_STAGED_STATUS) == 0)
        && (battle_script_get_variable(EVENT_SCRIPT_VAR_SHOP_ITEM_AVAILABILITY) != 0)) {
        idx = battle_unit_get_battle_index_by_misc_id(misc_id);
        if ((idx >= 0) && (value < 0x1F4)) {
            stats = battle_unit_get_stats_from_battle_id(idx);
            depth = battle_map_get_tile_data_value(
                MAP_TILE_DATA_DEPTH, stats->x, stats->position.bits.y, stats->position.raw >> 15);
            if ((stats->mount_info & BATTLE_MOUNT_INFO_ROLE_MASK) != 0) {
                g_battle_thread_call_target = (void (*)(void))battle_unit_set_status_animation_by_misc_id;
            } else if ((depth & 0xFF) >= 2) {
                g_battle_thread_call_target = (void (*)(void))battle_unit_set_mounted_animation_by_misc_id;
            } else {
                battle_unit_set_specific_animation_by_misc_id(misc_id, value);
                return;
            }
            battle_thread_call_on_main_stack(misc_id);
            return;
        }
    }
    battle_unit_set_specific_animation_by_misc_id(misc_id, value);
}
