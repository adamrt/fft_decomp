#ifndef FFT_BATTLE_ABILITY_H
#define FFT_BATTLE_ABILITY_H

#include "psx/types.h"

/* Primary ability-record byte 3 at 0x8005ebf0. Learn-on-hit is consumed at
 * 0x8018e6b8; WORLD 0x80122b08 and JOBSTTS 0x801e03bc prove the JP-learning
 * flag's negative polarity. */
typedef enum ability_type_flag {
    ABILITY_TYPE_FLAG_LEARN_ON_HIT = 0x20,
    ABILITY_TYPE_FLAG_CANNOT_LEARN_WITH_JP = 0x80,
} ability_type_flag_e;

enum {
    ABILITY_TYPE_FLAG_CANNOT_LEARN_WITH_JP_SHIFT = 7,
};

/* Eight-byte ability records at 0x8005ebf0. Bytes 4..7 hold the AI behaviour
 * flags: the AI loaders copy them as halfwords, the strike simulator reads
 * them as bytes, and the random-action picker tests byte 7 bit 0x80. */
typedef struct ability_data {
    u16 jp_cost;   /* 0x00 */
    u8 learn_rate; /* 0x02; learn-on-hit roll out of 100 (0x8018e6b8) */
    u8 type_flags; /* 0x03; ability_type_flag_e */
    union {
        u16 halfwords[2];
        u8 bytes[4];
    } ai_flags; /* 0x04 */
} ability_data_t;

typedef char battle_ability_primary_record_size_must_be_8[(sizeof(ability_data_t) == 8) ? 1 : -1];

/* Bit flags in status_infliction_data_t.type and the active-ability copy. The
 * target status applicator checks these in descending-bit precedence. */
typedef enum battle_status_infliction_type {
    BATTLE_STATUS_INFLICTION_TYPE_CANCEL = 0x10,
    BATTLE_STATUS_INFLICTION_TYPE_SEPARATE = 0x20,
    BATTLE_STATUS_INFLICTION_TYPE_RANDOM_ONE = 0x40,
    BATTLE_STATUS_INFLICTION_TYPE_ALL_OR_NOTHING = 0x80,
} battle_status_infliction_type_e;

/* Six-byte status-infliction records at 0x80063fc4; byte 0 selects how the
 * five-byte status set is applied. */
typedef struct status_infliction_data {
    u8 type;
    u8 statuses[5];
} status_infliction_data_t;

typedef char battle_status_infliction_record_size_must_be_6[(sizeof(status_infliction_data_t) == 6) ? 1 : -1];

extern ability_data_t g_main_ability_data[];
extern status_infliction_data_t g_main_status_infliction_data[];
#endif
