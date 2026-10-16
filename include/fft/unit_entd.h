#ifndef UNIT_ENTD_H
#define UNIT_ENTD_H

/* ENTD records: the static unit definitions of each encounter, which the unit initializers read from the ENTD#.ENT
 * files. */

#include "fft/unit.h"
#include "psx/types.h"

/* files */
/* Each ENTD#.ENT file holds 128 encounters of 16 entries; an ENTD id at or
 * above ENTDn_FIRST_EVENT is in file n. */
enum {
    ENTD_UNITS_PER_ENCOUNTER = 16,
    ENTD_ENCOUNTERS_PER_FILE = 128,
    ENTD2_FIRST_EVENT = ENTD_ENCOUNTERS_PER_FILE,
    ENTD3_FIRST_EVENT = 2 * ENTD_ENCOUNTERS_PER_FILE,
    ENTD4_FIRST_EVENT = 3 * ENTD_ENCOUNTERS_PER_FILE,
};

/* The ENTD slots precede the five deployed-party slots in battle storage. */
enum { BATTLE_DEPLOYED_UNIT_SLOT_FIRST = ENTD_UNITS_PER_ENCOUNTER };

/* entries */
enum {
    ENTD_VALUE_RANDOM = 254, /* in an ENTD equipment, level or secondary skillset byte: pick one at random */
};

/* Y, facing and tile-layer fields of an entry; unit_position_t has a
 * wider facing field. */
typedef struct entd_unit_position {
    u16 y : 8;
    u16 facing : 2;
    u16 _unused_01 : 2;            /* bits 10-11 */
    u16 spell_quote_skillsets : 2; /* unit_spell_quote_flags_e */
    u16 stepping_stone : 1;
    u16 higher_elevation : 1;
} entd_unit_position_t;
typedef char entd_unit_position_size_must_be_2[(sizeof(entd_unit_position_t) == 2) ? 1 : -1];

/* ENTD equipment order: head, body, accessory, then one slot per hand
 * (unit_equipment_slot_e splits each hand into weapon and shield). */
typedef enum entd_equipment_slot {
    ENTD_EQUIPMENT_SLOT_HEAD = 0,
    ENTD_EQUIPMENT_SLOT_BODY = 1,
    ENTD_EQUIPMENT_SLOT_ACCESSORY = 2,
    ENTD_EQUIPMENT_SLOT_RIGHT_HAND = 3,
    ENTD_EQUIPMENT_SLOT_LEFT_HAND = 4,
    ENTD_EQUIPMENT_SLOT_COUNT = 5,
} entd_equipment_slot_e;

/*
 * An ENTD entry is the static 0x28-byte encounter definition read from an
 * ENTD#.ENT file. It is input to the SCUS unit initializers; it is neither a
 * persistent party_unit_t record nor a runtime unit_stats_t record. The
 * current-ENTD pointer g_main_entd_current_unit selects one entry, not an
 * encounter.
 *
 * Names below are established by target SCUS initialization routines.
 * The remaining bytes intentionally retain neutral names until their runtime
 * consumers are matched.
 */
typedef struct entd_unit {
    u8 sprite_set;  /* 0x00 */
    u8 unit_flags;  /* 0x01; unit_flags_e */
    u8 name_id;     /* 0x02 */
    u8 level;       /* 0x03; 0 or 0xfe: L - rnd(0..L/8); 100 + n: L + n; L: top party level */
    u8 birth_month; /* 0x04; 1..12 for a fixed birthday */
    u8 birth_day;   /* 0x05; 1..31 for a fixed birthday */
    u8 bravery;     /* 0x06; above 100: random 45..74 */
    u8 faith;       /* 0x07; above 100: random 45..74 */
    /* Career job index (0 base job, 1 Chemist ... 19 Mime) whose level and
     * unlock requirements the unit gets; 20 or more gives no job levels. */
    u8 job_unlock;                           /* 0x08 */
    u8 job_level;                            /* 0x09; low nibble: that job's level */
    u8 job_id;                               /* 0x0a */
    u8 secondary_skillset;                   /* 0x0b */
    unit_abilities_t abilities;              /* 0x0c; reaction, support, movement */
    u8 equipment[ENTD_EQUIPMENT_SLOT_COUNT]; /* 0x12 */
    u8 palette;                              /* 0x17 */
    u8 team_flags;                           /* 0x18 */
    s8 x;                                    /* 0x19 */
    entd_unit_position_t position;           /* 0x1a */
    u8 experience;                           /* 0x1c; 100 and above: random 0..99 */
    u8 primary_skillset;                     /* 0x1d; 0 or 0xff: the job's own */
    u8 war_trophy;                           /* 0x1e; copied to unit_stats_t.war_trophy */
    u8 bonus_money_modifier;                 /* 0x1f; copied to unit_stats_t.bonus_money_modifier */
    u8 unit_id;                              /* 0x20 */
    /* 0x21-0x27 are copied as seven bytes to unit_stats_t ai_target_x onward
     * by a loop that indexes ai_target_xy, so nothing names the members after it. */
    u8 ai_target_xy[2];       /* 0x21; AI "stay near" X, Y */
    u8 ai_flags;              /* 0x23; ai_decision_flags_e; 0x80 = upper level of the stay-near tile */
    u8 ai_target_unit_id;     /* 0x24; unit_stats_t prioritized_target */
    u8 ai_targeting_flags_1;  /* 0x25; copied to ai_unit_decision_t +0x06 */
    u8 ai_targeting_flags_2;  /* 0x26; ai_targeting_flags_2_e */
    u8 ai_initial_enemy_flag; /* 0x27 */
} entd_unit_t;
typedef char entd_unit_size_must_be_40[(sizeof(entd_unit_t) == 40) ? 1 : -1];

typedef struct entd_encounter {
    entd_unit_t units[ENTD_UNITS_PER_ENCOUNTER];
} entd_encounter_t;
typedef char entd_encounter_size_must_be_640[(sizeof(entd_encounter_t) == 640) ? 1 : -1];

typedef struct entd_file {
    entd_encounter_t encounters[ENTD_ENCOUNTERS_PER_FILE];
} entd_file_t;
typedef char entd_file_size_must_be_81920[(sizeof(entd_file_t) == 81920) ? 1 : -1];

#endif
