#include "fft/battle_map.h"
#include "fft/battle_unit.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "psx/types.h"

/* Request a runtime animation, selecting mount/status animations when battle rules require them. */
void unit_set_animation(s32 runtime_id, u32 animation) {
    s32 battle_id;
    unit_stats_t* stats;
    s32 depth;

    if ((script_get_variable(EVENT_SCRIPT_VAR_BATTLE_NOT_STARTED) == 0)
        && (script_get_variable(EVENT_SCRIPT_VAR_SHOP_ITEM_AVAILABILITY) != 0)) {
        battle_id = battle_unit_get_battle_id_by_runtime_id(runtime_id);
        if ((battle_id >= 0) && (animation < BATTLE_SEQUENCE_EVTCHR_SLOT_0)) {
            stats = battle_unit_get_stats_from_battle_id(battle_id);
            depth = map_get_tile_data_value(
                MAP_TILE_DATA_DEPTH, stats->x, stats->position.y, stats->position.higher_elevation);
            if ((stats->mount_info & BATTLE_MOUNT_INFO_ROLE_MASK) != 0) {
                g_thread_call_target = (void (*)(void))battle_unit_set_status_animation_by_runtime_id;
            } else if ((depth & 0xFF) >= 2) {
                g_thread_call_target = (void (*)(void))battle_unit_set_mounted_animation_by_runtime_id;
            } else {
                battle_unit_set_specific_animation_by_runtime_id(runtime_id, animation);
                return;
            }
            thread_call_on_main_stack(runtime_id);
            return;
        }
    }
    battle_unit_set_specific_animation_by_runtime_id(runtime_id, animation);
}
