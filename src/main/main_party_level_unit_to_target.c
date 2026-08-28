#include "fft/main_runtime.h"
#include "fft/main_unit.h"
#include "psx/types.h"

/* Raises a roster unit by `level_delta` levels (capped at 99), growing each
 * 24-bit raw stat by raw / (level + growth) per level, and returns the new
 * level.  The growth table is the job's growths_multipliers pairs. */
u32 main_party_level_unit_to_target(party_data_t* party_unit, s32 level_delta) {
    u32 current_level = party_unit->level;
    u32 level = current_level + level_delta;
    u8* growth_table;
    unit_raw_stat_bytes_t* raw_base;
    s32 levels_to_gain;
    s32 level_index;
    unit_raw_stat_bytes_t* raw_stat;
    unit_raw_stat_bytes_t* raw_end;
    u8* stat_growth;
    u32 raw;
    u32 divisor;
    s32 unused;

    raw_base = (unit_raw_stat_bytes_t*)party_unit->raw_stats;
    growth_table = g_job_data_pointer[party_unit->job_id].growths_multipliers;
    if (level >= 100) {
        level = 99;
    }
    levels_to_gain = level - current_level;
    if (levels_to_gain <= 0) {
        return level;
    }
    for (level_index = 0; level_index < levels_to_gain; level_index++) {
        raw_stat = raw_base;
        stat_growth = growth_table;
        raw_end = raw_stat + UNIT_RAW_STAT_COUNT;
        do {
            /* Zero-byte hint: keeps GCC from splitting the stat walk into
             * a second pointer biased to the high byte. */
            __asm__("" : "=r"(raw_stat) : "0"(raw_stat));
            /* Reuse the earlier level temporary: a separate single-set temporary
             * lets sched1 sink this load below the position used by the target. */
            level = party_unit->level;
            raw = raw_stat->low + (raw_stat->middle << 8) + (raw_stat->high << 16);
            if (*stat_growth == 0) {
                divisor = level + 1;
            } else {
                divisor = level + *stat_growth;
            }
            raw += raw / divisor;
            if (raw > 0xffffff) {
                raw = 0xffffff;
            }
            raw_stat->middle = raw >> 8;
            raw_stat->low = raw;
            raw_stat->high = raw >> 16;
            raw_stat++;
            stat_growth += 2;
        } while ((s32)raw_stat < (s32)raw_end);
        party_unit->level++;
    }
    return party_unit->level;
}
