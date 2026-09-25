#include "fft/battle.h"
#include "fft/world.h"
#include "psx/types.h"

/* WORLD twin of battle_unit_set_specific_animation_value_on_battle_init. */
void world_unit_set_animation(s32 misc_id, u32 value) {
    s32 battle_id;
    battle_stats_t* stats;
    s32 depth;

    if ((world_script_get_variable(EVENT_SCRIPT_VAR_PENDING_STAGED_STATUS) == 0)
        && (world_script_get_variable(EVENT_SCRIPT_VAR_SHOP_ITEM_AVAILABILITY) != 0)) {
        battle_id = battle_unit_get_battle_index_by_misc_id(misc_id);
        if ((battle_id >= 0) && (value < 0x1F4)) {
            stats = battle_unit_get_stats_from_battle_id(battle_id);
            depth = battle_map_get_tile_data_value(
                MAP_TILE_DATA_DEPTH, stats->x, stats->position.bits.y, stats->position.raw >> 15);
            if ((stats->mount_info & BATTLE_MOUNT_INFO_ROLE_MASK) != 0) {
                g_world_thread_call_target = (void (*)(void))battle_unit_set_status_animation_by_misc_id;
                world_thread_call_on_main_stack(misc_id);
                return;
            }
            if ((depth & 0xFF) >= 2) {
                g_world_thread_call_target = (void (*)(void))battle_unit_set_mounted_animation_by_misc_id;
                world_thread_call_on_main_stack(misc_id);
                return;
            }
        }
    }
    battle_unit_set_specific_animation_by_misc_id(misc_id, value);
}
