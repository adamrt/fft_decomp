#include "fft/main.h"
#include "psx/types.h"

/* Level up at 100 EXP: one level of growth, and EXP goes back to 0 (the excess
 * is lost). At level 99 EXP is held at 99 instead. Returns 1 when the level
 * rose. */
s32 main_unit_check_level_up(unit_stats_t* unit) {
    u8 level;

    if (unit->experience < UNIT_LEVEL_UP_EXPERIENCE) {
        return 0;
    }

    level = unit->level;
    if (level >= UNIT_LEVEL_MAX) {
        unit->experience = UNIT_LEVEL_EXPERIENCE_MAX;
        return 0;
    }

    main_unit_apply_level_growth(unit, 0);
    unit->experience = 0;
    unit->level = level + 1;
    return 1;
}
