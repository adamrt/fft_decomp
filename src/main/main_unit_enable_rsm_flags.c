#include "fft/main.h"
#include "psx/types.h"

/* Rebuilds the unit's active reaction/support/movement bitfields
 * (0x8b..0x95) from its four innate abilities and equipped R/S/M
 * abilities. */
void main_unit_enable_rsm_flags(battle_stats_t* unit) {
    s32 i;
    battle_stats_t* walk;

    main_util_clear_byte_data(unit->reaction_abilities, 0xB);
    i = 0;
    /* Keep the base at the unit start and advance it by one ability width;
     * loading through the field preserves the target's +0x0a displacement. */
    walk = unit;
    do {
        main_unit_set_rsm_flag(unit, walk->innate_abilities[0]);
        walk = (battle_stats_t*)((u8*)walk + sizeof(u16));
        i += 1;
    } while (i < 4);
    main_unit_set_rsm_flag(unit, unit->reaction_ability);
    main_unit_set_rsm_flag(unit, unit->support_ability);
    main_unit_set_rsm_flag(unit, unit->movement_ability);
}
