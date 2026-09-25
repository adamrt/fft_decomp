#include "fft/battle.h"
#include "fft/script_variables.h"
#include "fft/world.h"
#include "psx/types.h"

/* Face a unit, and its mount when one is linked, in the given direction.
   Script variable 0x1FD suppresses the mount update. */
void world_unit_set_facing(s32 misc_id, s32 facing) {
    s32 battle_id;
    battle_stats_t* stats;

    if (world_script_get_variable(EVENT_SCRIPT_VAR_PENDING_STAGED_STATUS) == 0) {
        battle_id = battle_unit_get_battle_index_by_misc_id(misc_id);
        if (battle_id >= 0) {
            stats = battle_unit_get_stats_from_battle_id(battle_id);
            if (stats->mount_info != 0) {
                /* The target narrows facing to u16 for the definition's s32 parameter. */
                ((void (*)(s32, u16))battle_unit_set_animation_from_facing_nibble)(
                    battle_unit_get_misc_id_by_battle_id(stats->mount_info & 0x1F), facing);
            }
        }
    }
    /* The target narrows facing to u16 for the definition's s32 parameter. */
    ((void (*)(s32, u16))battle_unit_set_animation_from_facing_nibble)(misc_id, facing);
}
