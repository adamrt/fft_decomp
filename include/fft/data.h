#ifndef DATA_H
#define DATA_H

/* Small game data shared across programs: calendar dates, the calculator's ability flags and elements. Abilities,
 * items, jobs, skillsets and statuses have their own data_*.h headers. */

#include "psx/types.h"

/* calendar */
/* Calendar month and day, stored as separate bytes. */
typedef struct calendar_date {
    u8 month;
    u8 day;
} calendar_date_t;
typedef char calendar_date_size_must_be_2[sizeof(calendar_date_t) == 2 ? 1 : -1];

/* calculator */
/* Flags stored by the eight Math Skill abilities in g_ability_math_rsm_data.
 * The first four select a unit attribute; the last four select its divisor
 * test. */
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

/* elements */
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

#endif
