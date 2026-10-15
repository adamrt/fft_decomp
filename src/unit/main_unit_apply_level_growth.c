#include "fft/main.h"
#include "psx/types.h"

/* Applies one level of growth (or its inverse for a level down) to each
 * 24-bit raw stat: raw +/- raw / (growth + level), a zero growth counting as
 * 1, capped at 0xffffff; then refreshes the derived stats and clamps HP/MP. */
void main_unit_apply_level_growth(unit_stats_t* unit, s32 level_down) {
    unit_raw_stat_t* raw_stats;
    job_stat_growth_t* growth;
    u32 value;
    u32 divisor;
    s32 level;
    u32 minimum_growth;
    s32 i;

    raw_stats = unit->raw_stats;
    growth = unit->stat_growths;
    level = unit->level;

    for (i = 0; i < UNIT_RAW_STAT_COUNT; i++) {
        divisor = growth[i].growth;
        value = raw_stats[i].low + (raw_stats[i].middle << 8) + (raw_stats[i].high << 16);
        minimum_growth = divisor;
        if (divisor == 0) {
            minimum_growth = 1;
        }
        divisor = minimum_growth + level;
        rand();
        if (level_down) {
            value -= value / divisor;
        } else {
            value += value / divisor;
        }
        if (value > 0x00ffffff) {
            value = 0x00ffffff;
        }
        raw_stats[i].low = value;
        raw_stats[i].middle = value >> 8;
        raw_stats[i].high = value >> 16;
    }

    main_unit_refresh_stats_and_statuses(unit);
    if (unit->hp > unit->max_hp) {
        unit->hp = unit->max_hp;
    }
    if (unit->mp > unit->max_mp) {
        unit->mp = unit->max_mp;
    }
}
