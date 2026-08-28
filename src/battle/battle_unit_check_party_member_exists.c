#include "fft/battle.h"
#include "fft/unit_slots.h"
#include "psx/types.h"

u8* battle_unit_check_party_member_exists(s32 unit_id) {
    if (unit_id < BATTLE_UNIT_SLOT_COUNT) {
        if (g_battle_unit_stats[unit_id].entd_slot != BATTLE_ENTD_SLOT_NONE) {
            return &g_battle_unit_stats[unit_id].action_actor_id;
        }
    }
    return 0;
}
