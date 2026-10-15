#include "fft/battle_unit.h"
#include "psx/types.h"

/* Copy the ENTD control-byte into the unit's runtime data, but strip the
   0x08 (player-controllable) bit when the unit's stats mark it as
   AI-only. Also propagates to the linked mount unit, if any. */
void battle_unit_store_entd_flags_without_control_flag(battle_unit_t* unit) {
    u8 entd_flags;
    u8 mount_entd_flags;
    battle_unit_t* mount;

    entd_flags = unit->stats->team_flags;
    unit->team_flags = entd_flags;
    if (unit->stats->auto_battle_setting != AI_AUTOBATTLE_MANUAL) {
        unit->team_flags = entd_flags & ~BATTLE_TEAM_FLAG_PLAYER_CONTROLLED;
    }
    if (unit->mount_state != BATTLE_UNIT_MOUNT_STATE_NONE) {
        mount = battle_unit_get_by_runtime_id(unit->mount_partner_runtime_id);
        mount_entd_flags = mount->stats->team_flags;
        mount->team_flags = mount_entd_flags;
        if (mount->stats->auto_battle_setting != AI_AUTOBATTLE_MANUAL) {
            mount->team_flags = mount_entd_flags & ~BATTLE_TEAM_FLAG_PLAYER_CONTROLLED;
        }
    }
}
