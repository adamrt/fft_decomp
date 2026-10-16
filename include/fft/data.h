#ifndef DATA_H
#define DATA_H

/* Small game data shared across modules: calendar dates, calculator, elements and statuses. Abilities, items,
 * jobs and skillsets have their own data_*.h headers. */

#include "psx/types.h"

/* calendar */
/* Calendar month and day, stored as separate bytes. */
typedef struct calendar_date {
    u8 month;
    u8 day;
} calendar_date_t;
typedef char calendar_date_size_must_be_2[sizeof(calendar_date_t) == 2 ? 1 : -1];

/* calculator */
/* Flags stored by the eight Math Skill abilities at 0x80061054. The first
 * four select a unit attribute; the last four select its divisor test. */
enum {
    CALCULATOR_ATTRIBUTE_CT = 0x80,
    CALCULATOR_ATTRIBUTE_LEVEL = 0x40,
    CALCULATOR_ATTRIBUTE_EXP = 0x20,
    CALCULATOR_ATTRIBUTE_HEIGHT = 0x10,
    CALCULATOR_ATTRIBUTE_MASK = 0xf0,
    CALCULATOR_MULTIPLE_PRIME = 0x08,
    CALCULATOR_MULTIPLE_FIVE = 0x04,
    CALCULATOR_MULTIPLE_FOUR = 0x02,
    CALCULATOR_MULTIPLE_THREE = 0x01,
    CALCULATOR_MULTIPLE_MASK = 0x0f,
};

/* Attribute abilities occupy the first four slots, followed by four divisor tests. */
enum {
    CALCULATOR_ATTRIBUTE_ABILITY_COUNT = 4,
    CALCULATOR_MULTIPLE_ABILITY_COUNT = 4,
    CALCULATOR_ABILITY_COUNT = CALCULATOR_ATTRIBUTE_ABILITY_COUNT + CALCULATOR_MULTIPLE_ABILITY_COUNT,
    CALCULATOR_PRIME_COUNT = 54,
};

/* element */
/* Element bits shared by abilities, equipment, jobs and unit affinity sets. */
typedef enum element_flags {
    BATTLE_ELEMENT_DARK = 0x01,
    BATTLE_ELEMENT_HOLY = 0x02,
    BATTLE_ELEMENT_WATER = 0x04,
    BATTLE_ELEMENT_EARTH = 0x08,
    BATTLE_ELEMENT_WIND = 0x10,
    BATTLE_ELEMENT_ICE = 0x20,
    BATTLE_ELEMENT_LIGHTNING = 0x40,
    BATTLE_ELEMENT_FIRE = 0x80,
} element_flags_e;

/* Shared ordering for job, item-attribute, and runtime unit affinity arrays.
 * Job records contain only the first four entries; item and runtime records
 * also contain Strengthen. */
typedef enum elemental_affinity_index {
    ELEMENTAL_AFFINITY_ABSORB = 0,
    ELEMENTAL_AFFINITY_NULLIFY = 1,
    ELEMENTAL_AFFINITY_HALF = 2,
    ELEMENTAL_AFFINITY_WEAK = 3,
    ELEMENTAL_AFFINITY_STRENGTHEN = 4,
} elemental_affinity_index_e;

enum { ELEMENTAL_AFFINITY_COUNT = 5 };

/* status */
enum {
    STATUS_COUNT = 40,
    STATUS_BYTE_COUNT = 5,
    BATTLE_TIMED_STATUS_COUNT = 16, /* Poison through Death Sentence keep a CT byte each */
};

typedef struct status_effect_data {
    u8 _unused_00[2];
    u8 order;
    u8 ct;
    u8 flags_1;
    u8 flags_2;
    u8 cancels[STATUS_BYTE_COUNT];    /* 0x06; statuses removed when this one is inflicted */
    u8 cant_stack[STATUS_BYTE_COUNT]; /* 0x0b; current statuses that block this one */
} status_effect_data_t;
typedef char status_effect_data_size_must_be_16[(sizeof(status_effect_data_t) == 16) ? 1 : -1];

/* Flags that build the shared status-check sets during main initialization
 * (main_status_check_set_e). The records also set bits 0x08, 0x10, 0x20 and
 * 0x40 of flags_1 and 0x20 of flags_2, which nothing reads. */
typedef enum status_effect_flags_1 {
    STATUS_EFFECT_FLAG_1_KO = 0x01,
    STATUS_EFFECT_FLAG_1_UNIT_LOST = 0x02,
    STATUS_EFFECT_FLAG_1_CHECK_SET_2 = 0x04,
    STATUS_EFFECT_FLAG_1_FREEZE_CT = 0x80,
} status_effect_flags_1_e;

typedef enum status_effect_flags_2 {
    STATUS_EFFECT_FLAG_2_IMMORTAL_IMMUNITY = 0x01,
    STATUS_EFFECT_FLAG_2_FORMATION_IMMUNITY = 0x02,
    STATUS_EFFECT_FLAG_2_CHECK_SET_7 = 0x04,
    STATUS_EFFECT_FLAG_2_SCRIPT_FILTER_EXCLUDED = 0x08,
    STATUS_EFFECT_FLAG_2_MOUNT_REMOVAL = 0x10,
    STATUS_EFFECT_FLAG_2_IGNORE_ATTACKS = 0x20,
    STATUS_EFFECT_FLAG_2_PREVENT_REACTION = 0x80,
} status_effect_flags_2_e;

#endif
