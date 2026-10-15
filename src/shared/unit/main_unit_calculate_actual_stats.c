#include "fft/main.h"
#include "psx/types.h"

/* Derive max HP, max MP, Speed, PA and MA from the 24-bit raw stats: raw * job
 * multiplier / 1638400, at least 1; HP and MP at most 999, Speed at most 50,
 * PA and MA at most 99. An egg's HP and MP are ten times that, capped at
 * 0xffff.
 *
 * Mode 0 also fills HP and MP and, for a generated unit, first replays the
 * growth of levels 2 to its level (raw += raw / (growth + level - 1) for
 * each). A mode with bit 0 set leaves Speed, PA and MA as they are. */
void main_unit_calculate_actual_stats(unit_stats_t* unit, s32 mode) {
    u32 value;
    u32 multiplier;
    s32 cap;
    s32 max;
    s32 i;
    s32 j;
    u8 level;
    s32 brave_flag;
    s32 divisor;
    s32 is_low;
    job_stat_growth_t* mult;
    unit_raw_stat_t* raw;

    cap = 100;
    max = UNIT_HP_MP_MAX;
    brave_flag = mode & 1;
    if (unit->unit_flags & UNIT_FLAG_EGG) {
        cap = 10;
        max = 0xffff;
    }
    mult = unit->stat_growths;
    level = unit->level;
    raw = unit->raw_stats;

    for (i = 0; i < UNIT_RAW_STAT_COUNT; i++) {
        value = raw[i].low + (raw[i].middle << 8) + (raw[i].high << 16);
        if (mode == 0 && unit->formation_index == BATTLE_FORMATION_INDEX_GENERATED_UNIT) {
            divisor = mult[i].growth;
            divisor = divisor ? divisor : 1;
            for (j = 2; j <= level; j++) {
                value += value / (divisor + j - 1);
            }
        }
        divisor = 100;
        if (value > 0xffffff) {
            value = 0xffffff;
        }
        raw[i].middle = value >> 8;
        is_low = i < UNIT_RAW_STAT_SPEED;
        raw[i].low = value;
        raw[i].high = value >> 16;
        multiplier = mult[i].multiplier;
        if (is_low) {
            divisor = cap;
        }
        value = (value * multiplier) / divisor >> 14;
        if (value == 0) {
            value = 1;
        }
        if (is_low) {
            if (value > max) {
                value = max;
            }
        }
        if (i == UNIT_RAW_STAT_SPEED && value > UNIT_SPEED_MAX) {
            value = UNIT_SPEED_MAX;
        }
        if (i >= UNIT_RAW_STAT_PHYSICAL_ATTACK && value > UNIT_ATTACK_STAT_MAX) {
            value = UNIT_ATTACK_STAT_MAX;
        }
        switch (i) {
        case UNIT_RAW_STAT_HP:
            unit->max_hp = value;
            if (mode == 0) {
                unit->hp = value;
            }
            break;
        case UNIT_RAW_STAT_MP:
            unit->max_mp = value;
            if (mode == 0) {
                unit->mp = value;
            }
            break;
        case UNIT_RAW_STAT_SPEED:
            if (brave_flag == 0) {
                unit->base_attributes[UNIT_ATTRIBUTE_SPEED] = value;
            }
            break;
        case UNIT_RAW_STAT_PHYSICAL_ATTACK:
            if (brave_flag == 0) {
                unit->base_attributes[UNIT_ATTRIBUTE_PHYSICAL_ATTACK] = value;
            }
            break;
        case UNIT_RAW_STAT_MAGIC_ATTACK:
            if (brave_flag == 0) {
                unit->base_attributes[UNIT_ATTRIBUTE_MAGIC_ATTACK] = value;
            }
            break;
        }
    }
}
