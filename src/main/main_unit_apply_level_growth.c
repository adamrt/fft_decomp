#include "fft/main_unit.h"
#include "psx/types.h"

void main_unit_apply_level_growth(battle_stats_t* unit, s32 level_down) {
    u8* raw_stat_base;
    /* Pin: as a pseudo, loop.c splits raw_stat into two biased induction pointers. */
    register u8* raw_stat __asm__("$18");
    u8* growth;
    u32 value;
    u32 divisor;
    /* Pin: unpinned, level and level_down swap $s6 and $s7. */
    register u8 level __asm__("$23");
    u32 minimum_growth;
    u32 stored_byte;

    raw_stat_base = unit->raw_stats;
    raw_stat = raw_stat_base;
    growth = &unit->raw_stats[UNIT_RAW_STAT_DATA_BYTE_COUNT];
    level = unit->level;

    do {
        divisor = *growth;
        value = raw_stat[0] + (raw_stat[1] << 8) + (raw_stat[2] << 16);
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
        growth += 2;
        if (value > 0x00ffffff) {
            value = 0x00ffffff;
        }
        stored_byte = value >> 8;
        raw_stat[1] = stored_byte;
        stored_byte = value >> 16;
        raw_stat[0] = value;
        raw_stat[2] = stored_byte;
        raw_stat += UNIT_RAW_STAT_SERIALIZED_BYTES;
    } while ((s32)raw_stat < (s32)(raw_stat_base + UNIT_RAW_STAT_DATA_BYTE_COUNT));

    main_unit_refresh_stats_and_statuses(unit);
    if (unit->hp > unit->max_hp) {
        unit->hp = unit->max_hp;
    }
    if (unit->mp > unit->max_mp) {
        unit->mp = unit->max_mp;
    }
}
