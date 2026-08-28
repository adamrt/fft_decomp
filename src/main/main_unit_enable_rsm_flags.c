#include "fft/main_unit.h"
#include "psx/types.h"

/* Rebuilds the unit's active reaction/support/movement bitfields
 * (0x8b..0x95) from its four innate abilities and equipped R/S/M
 * abilities. */
void main_unit_enable_rsm_flags(battle_stats_t* unit) {
    s32 i;
    u8* walk;

    main_util_clear_byte_data(unit->reaction_abilities, 0xB);
    i = 0;
    /* The original walks innate_abilities[0..3] with a byte pointer that
     * starts at the unit base and keeps the +0x0a in the load. */
    walk = (u8*)unit;
    do {
        main_unit_set_rsm_flag(unit, ((battle_stats_t*)walk)->innate_abilities[0]);
        walk += 2;
        i += 1;
    } while (i < 4);
    main_unit_set_rsm_flag(unit, unit->reaction_ability);
    main_unit_set_rsm_flag(unit, unit->support_ability);
    main_unit_set_rsm_flag(unit, unit->movement_ability);
}
