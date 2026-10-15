#include "fft/main.h"
#include "fft/memory.h"
#include "psx/types.h"

/* Rebuilds the unit's active reaction/support/movement bitfields
 * (0x8b..0x95) from its four innate abilities and equipped R/S/M
 * abilities. */
void main_unit_enable_rsm_flags(unit_stats_t* unit) {
    s32 i;

    mem_zero(&unit->ability_flags, sizeof(unit->ability_flags));
    for (i = 0; i < 4; i++) {
        main_unit_set_rsm_flag(unit, unit->innate_abilities[i]);
    }
    main_unit_set_rsm_flag(unit, unit->abilities.reaction);
    main_unit_set_rsm_flag(unit, unit->abilities.support);
    main_unit_set_rsm_flag(unit, unit->abilities.movement);
}
