#ifndef FFT_UNIT_STATS_H
#define FFT_UNIT_STATS_H

#include "psx/types.h"

/* Five 24-bit raw values serialized in HP, MP, Speed, PA, MA order. */
typedef enum unit_raw_stat_index {
    UNIT_RAW_STAT_HP = 0,
    UNIT_RAW_STAT_MP = 1,
    UNIT_RAW_STAT_SPEED = 2,
    UNIT_RAW_STAT_PHYSICAL_ATTACK = 3,
    UNIT_RAW_STAT_MAGIC_ATTACK = 4,
    UNIT_RAW_STAT_COUNT = 5,
} unit_raw_stat_index_e;

enum {
    UNIT_RAW_STAT_SERIALIZED_BYTES = 3,
    UNIT_RAW_STAT_DATA_BYTE_COUNT = UNIT_RAW_STAT_COUNT * UNIT_RAW_STAT_SERIALIZED_BYTES,
};

typedef struct unit_raw_stat_bytes {
    u8 low;
    u8 middle;
    u8 high;
} unit_raw_stat_bytes_t;

typedef char
    unit_raw_stat_bytes_size_must_be_3[(sizeof(unit_raw_stat_bytes_t) == UNIT_RAW_STAT_SERIALIZED_BYTES) ? 1 : -1];

/* Calculated battle attributes use PA, MA, Speed order. */
typedef enum unit_attribute_index {
    UNIT_ATTRIBUTE_PHYSICAL_ATTACK = 0,
    UNIT_ATTRIBUTE_MAGIC_ATTACK = 1,
    UNIT_ATTRIBUTE_SPEED = 2,
    UNIT_ATTRIBUTE_COUNT = 3,
} unit_attribute_index_e;

#endif
