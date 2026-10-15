#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_effect.h"
#include "fft/battle_map.h"
#include "fft/battle_status.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/* Return the target only when its vertical span overlaps the attacker's.
 *
 * The coordinate arguments are unused here but are part of the caller ABI. */
s32 target_validate_height_overlap(s32 attacker_id, s32 target_x, s32 target_y, s32 target_elevation, s32 target_id) {
    battle_unit_height_data_t height_data;
    u32 target_walking_height;
    u32 target_total_height;
    u32 attacker_total_height;
    u32 attacker_walking_height;

    battle_unit_calculate_height_data(&height_data, target_id);
    if ((height_data.unit_flags & BATTLE_UNIT_HEIGHT_UNAVAILABLE) != 0) {
        return -1;
    }
    target_walking_height = height_data.walking_height;
    target_total_height = height_data.total_height;
    battle_unit_calculate_height_data(&height_data, attacker_id);
    if ((height_data.unit_flags & BATTLE_UNIT_HEIGHT_UNAVAILABLE) != 0) {
        return -1;
    }
    attacker_total_height = height_data.total_height;
    attacker_walking_height = height_data.walking_height;
    if (target_walking_height < attacker_total_height && target_total_height >= attacker_walking_height) {
        return target_id;
    }
    return -1;
}
