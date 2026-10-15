#include "fft/main.h"

/* Mark an R/S/M ability active and select reaction abilities.
 *
 * IDs 0x1a6..0x1fd map contiguously into the unit's MSB-first active flags.
 * A reaction ID also becomes the unit's currently equipped reaction ability. */
void main_unit_set_rsm_flag(unit_stats_t* unit, u32 ability_id) {
    u16 index;
    u16 original_ability_id;
    u8* rsm_flags;

    index = ability_id - ABILITY_ID_REACTION_FIRST;
    original_ability_id = ability_id;
    ability_id = index;
    if (index < ABILITY_ID_RANDOM_FIRST - ABILITY_ID_REACTION_FIRST) {
        if (original_ability_id < ABILITY_ID_SUPPORT_FIRST) {
            unit->abilities.reaction = original_ability_id;
        }
        rsm_flags = (u8*)&unit->ability_flags;
        rsm_flags += ability_id >> 3;
        *rsm_flags |= 0x80 >> (ability_id & 7);
    }
}
