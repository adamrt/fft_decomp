#include "fft/battle.h"
#include "psx/types.h"

void main_unit_calculate_actual_stats(battle_stats_t* unit, s32 mode) {
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
    u8* mult;
    u8* raw;

    cap = 100;
    max = 999;
    brave_flag = mode & 1;
    if (unit->unit_flags & UNIT_FLAG_EGG) {
        cap = 10;
        max = 0xffff;
    }
    mult = unit->raw_stats + UNIT_RAW_STAT_DATA_BYTE_COUNT;
    level = unit->level;
    raw = unit->raw_stats;

    for (i = 0; i < UNIT_RAW_STAT_COUNT; i++) {
        value = raw[i * UNIT_RAW_STAT_SERIALIZED_BYTES] + (raw[i * UNIT_RAW_STAT_SERIALIZED_BYTES + 1] << 8)
            + (raw[i * UNIT_RAW_STAT_SERIALIZED_BYTES + 2] << 16);
        if (mode == 0 && unit->formation_index == BATTLE_FORMATION_INDEX_GENERATED_UNIT) {
            divisor = mult[i * 2];
            divisor = divisor ? divisor : 1;
            for (j = 2; j <= level; j++) {
                value += value / (divisor + j - 1);
            }
        }
        divisor = 100;
        if (value > 0xffffff) {
            value = 0xffffff;
        }
        raw[i * UNIT_RAW_STAT_SERIALIZED_BYTES + 1] = value >> 8;
        is_low = i < UNIT_RAW_STAT_SPEED;
        raw[i * UNIT_RAW_STAT_SERIALIZED_BYTES] = value;
        raw[i * UNIT_RAW_STAT_SERIALIZED_BYTES + 2] = value >> 16;
        multiplier = mult[i * 2 + 1];
        if (is_low) {
            divisor = cap;
        }
        value = (value * multiplier) / divisor >> 14;
        if (value == 0) {
            value = 1;
        }
        if (is_low) {
            if (value > (u32)max) {
                value = max;
            }
        }
        if (i == UNIT_RAW_STAT_SPEED && value >= 0x33) {
            value = 0x32;
        }
        if (i >= UNIT_RAW_STAT_PHYSICAL_ATTACK && value >= 0x64) {
            value = 0x63;
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
