#include "fft/battle.h"
#include "fft/data.h"
#include "psx/types.h"

s32 battle_status_remove_charging_ability_ct(battle_stats_t* unit, s32 do_disable) {
    u8 mount_info;

    for (;;) {
        if ((unit->status_sets.current[0] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_CHARGING))
            && unit->charged_ability_ct != 0xFF && unit->last_skillset_id == SKILLSET_ID_CHARGE) {
            if (do_disable != 0) {
                battle_status_disable_acting(unit);
            }
            return 1;
        }
        mount_info = unit->mount_info;
        if (!(mount_info & BATTLE_MOUNT_INFO_FLAG_MOUNT)) {
            break;
        }
        unit = &g_battle_unit_stats[mount_info & BATTLE_MOUNT_INFO_PARTNER_ID_MASK];
    }
    return 0;
}
