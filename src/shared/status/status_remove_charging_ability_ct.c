#include "fft/battle_status.h"
#include "psx/types.h"

/* 1 when the unit, or its rider when it is a mount, is charging an attack of
 * the Charge skillset; with do_disable, status_disable_acting is called
 * on that unit. */
s32 status_remove_charging_ability_ct(unit_stats_t* unit, s32 do_disable) {
    u8 mount_info;

    for (;;) {
        if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CHARGING)] & STATUS_BYTE_MASK(STATUS_ID_CHARGING))
            && unit->charged_ability_ct != 0xFF && unit->action_command.skillset_id == SKILLSET_ID_CHARGE) {
            if (do_disable != 0) {
                status_disable_acting(unit);
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
