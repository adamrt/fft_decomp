#ifndef UNIT_PARTY_H
#define UNIT_PARTY_H

/* Party records: the persistent roster of party_unit_t records and its slots. */

#include "fft/job.h"
#include "fft/unit.h"
#include "psx/types.h"

/* The persistent roster, g_party_units, holds 16 formation slots
 * followed by four guest slots, each with the party_unit_t 0x100-byte stride. */
enum {
    PARTY_MEMBER_SLOT_COUNT = 16,
    PARTY_GUEST_SLOT_FIRST = PARTY_MEMBER_SLOT_COUNT,
    PARTY_ROSTER_SLOT_COUNT = 20,
};

/* party_unit_t.party_id sentinel: `0xff` means the unit does not appear on
 * Formation; tested by the roster scans in src/main and the WORLD/WLDCORE
 * party walks. */
enum {
    PARTY_ID_NONE = 255,
};

/* Serialized birthday and zodiac bits. */
enum {
    PARTY_BIRTHDAY_DAY_MASK = 0x01ff,
    PARTY_BIRTHDAY_DAY_HIGH_BIT_MASK = 0x01,
    PARTY_ZODIAC_SHIFT = 4,
    PARTY_ZODIAC_MASK = 0xf0,
};

/*
 * Persistent roster record. Its 0x100-byte stride is established by the
 * indexed SCUS lookup, while its named prefix is established by the target
 * party_unit_t-to-unit_stats_t initializer. It is distinct from entd_unit_t.
 */
typedef struct party_unit {
    u8 sprite_set;         /* 0x00 */
    u8 party_id;           /* 0x01 */
    u8 job_id;             /* 0x02 */
    u8 palette;            /* 0x03 */
    u8 gender_flags;       /* 0x04 */
    u8 birthday_day;       /* 0x05 */
    u8 zodiac;             /* 0x06 */
    u8 secondary_skillset; /* 0x07 */
    /* Little-endian ability ids; every reader and writer accesses the two bytes separately. */
    u8 reaction_ability[2];                  /* 0x08 */
    u8 support_ability[2];                   /* 0x0a */
    u8 movement_ability[2];                  /* 0x0c */
    u8 equipment[UNIT_EQUIPMENT_SLOT_COUNT]; /* 0x0e; unit_equipment_slot_e */
    u8 experience;                           /* 0x15 */
    u8 level;                                /* 0x16 */
    u8 bravery;                              /* 0x17 */
    u8 faith;                                /* 0x18 */
    unit_raw_stats_t raw_stats;              /* 0x19 */
    unit_career_data_t career;               /* 0x28; unlocked jobs, learned abilities, levels and JP */
    u8 name[UNIT_NAME_SIZE];                 /* 0xbe */
    unit_name_id_t name_id;                  /* 0xce */
    u8 proposition_status;                   /* 0xd0; nonzero while unavailable on a proposition */
    u8 _unused_d1;
    u8 egg_color;        /* 0xd2; create/save/generate unit stores */
    u8 _unused_d3[0x2d]; /* 0xd3; saves and loads copy 0xe0 bytes per record, so 0xe0..0xff is not saved */
} party_unit_t;
typedef char party_unit_size_must_be_256[(sizeof(party_unit_t) == 256) ? 1 : -1];

#endif
