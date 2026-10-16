#ifndef DATA_STATUS_H
#define DATA_STATUS_H

/* Status data: status ids and their bit masks, the status sets units and records carry, the status effect records
 * and the check sets main builds from them. */

#include "psx/types.h"

/* status ids */
enum {
    STATUS_COUNT = 40,
    STATUS_BYTE_COUNT = 5,
    BATTLE_TIMED_STATUS_COUNT = 16, /* Poison through Death Sentence keep a CT byte each */
};

/* Canonical status IDs. The owning byte is id >> 3 and its MSB-first mask is
 * 0x80 >> (id & 7); IDs are not byte masks. Entry 0 has no display name but
 * remains addressable in the target's status tables. */
typedef enum status_id {
    STATUS_ID_UNNAMED_00 = 0,
    STATUS_ID_CRYSTAL = 1,
    STATUS_ID_DEAD = 2,
    STATUS_ID_UNDEAD = 3,
    STATUS_ID_CHARGING = 4,
    STATUS_ID_JUMP = 5, /* "Jumping" in the name text */
    STATUS_ID_DEFENDING = 6,
    STATUS_ID_PERFORMING = 7,
    STATUS_ID_PETRIFY = 8,
    STATUS_ID_INVITE = 9, /* "Invitation" in the name text */
    STATUS_ID_DARKNESS = 10,
    STATUS_ID_CONFUSION = 11,
    STATUS_ID_SILENCE = 12,
    STATUS_ID_BLOOD_SUCK = 13,
    STATUS_ID_UNNAMED_0E = 14, /* no name text and no data inflicts it; it darkens the unit */
    STATUS_ID_TREASURE = 15,
    STATUS_ID_OIL = 16,
    STATUS_ID_FLOAT = 17,
    STATUS_ID_RERAISE = 18,
    STATUS_ID_TRANSPARENT = 19,
    STATUS_ID_BERSERK = 20,
    STATUS_ID_CHICKEN = 21,
    STATUS_ID_FROG = 22,
    STATUS_ID_CRITICAL = 23,
    STATUS_ID_POISON = 24,
    STATUS_ID_REGEN = 25,
    STATUS_ID_PROTECT = 26,
    STATUS_ID_SHELL = 27,
    STATUS_ID_HASTE = 28,
    STATUS_ID_SLOW = 29,
    STATUS_ID_STOP = 30,
    STATUS_ID_WALL = 31,
    STATUS_ID_FAITH = 32,
    STATUS_ID_INNOCENT = 33,
    STATUS_ID_CHARM = 34,
    STATUS_ID_SLEEP = 35,
    STATUS_ID_DONT_MOVE = 36,
    STATUS_ID_DONT_ACT = 37,
    STATUS_ID_REFLECT = 38,
    STATUS_ID_DEATH_SENTENCE = 39,
} status_id_e;

#define STATUS_BYTE_INDEX(id) ((id) >> 3)

#define STATUS_BYTE_MASK(id) ((u8)(0x80U >> ((id) & 7)))

/* status sets */
/* A unit's innate, immunity and current statuses, STATUS_BYTE_COUNT bytes
 * each. main_unit_copy_job_data copies the three sets as one 15-byte block, so
 * they stay one aggregate that a whole-block byte copy can span; status readers
 * use the named sets. */
typedef struct status_sets {
    u8 innate[5];
    u8 immunity[5];
    u8 current[5];
} status_sets_t;
typedef char status_sets_size_must_be_15[(sizeof(status_sets_t) == 15) ? 1 : -1];

/* Static innate, immunity and starting-status masks embedded in item and job
 * records. This differs from status_sets_t, whose third set is the
 * mutable current-status mask. */
typedef struct initial_status_sets {
    u8 innate[5];
    u8 immunity[5];
    u8 starting[5];
} initial_status_sets_t;
typedef char initial_status_sets_size_must_be_15[(sizeof(initial_status_sets_t) == 15) ? 1 : -1];

/* status effect records */
/* One record per status id in g_main_status_effect_data. */
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
 * (status_check_set_e). The records also set bits 0x08, 0x10, 0x20 and
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

/* check sets */
/* Rows of g_main_status_check_sets. main_status_init_check_data builds rows 0
 * to 9 from the flag bytes of g_main_status_effect_data and writes row 10
 * itself. Each enumerator's comment lists the statuses its row holds in this
 * release, then what the row is used for. */
typedef enum status_check_set {
    /* Crystal, Dead, Petrify, Invite, Blood Suck, Treasure. A unit with none of them still fights for its side. */
    STATUS_CHECK_SET_KO = 0,
    /* Crystal, Invite, Treasure. On an importance-3 unit of either team it decides the battle. */
    STATUS_CHECK_SET_UNIT_LOST = 1,
    /* Crystal, Treasure. Built; no code reads it. */
    STATUS_CHECK_SET_UNREAD_2 = 2,
    /* Crystal, Petrify, Treasure, Stop, Sleep. The unit's CT does not advance. */
    STATUS_CHECK_SET_FREEZE_CT = 3,
    /* Crystal, Dead, Jump, Petrify, Confusion, Blood Suck, Treasure, Berserk, Chicken, Frog, Stop, Sleep, Don't Act.
     * No reaction, no learning on hit, no movement-ability benefit. */
    STATUS_CHECK_SET_PREVENT_REACTION = 4,
    /* Crystal, Dead, Undead, Petrify, Invite, Confusion, Blood Suck, Treasure, Chicken, Frog, Charm, Sleep,
     * Don't Act, Death Sentence. Added to the immunities of an immortal unit with a save- or load-formation flag. */
    STATUS_CHECK_SET_IMMORTAL_IMMUNITY = 5,
    /* Crystal, Dead, Undead, Petrify, Invite, Blood Suck, Treasure, Chicken, Frog, Charm, Death Sentence.
     * Added with set 5, and never inflicted on an immortal unit. */
    STATUS_CHECK_SET_FORMATION_IMMUNITY = 6,
    /* Darkness, Confusion, Silence, Transparent, Berserk, Chicken, Frog, Stop, Charm, Sleep, Don't Move, Don't Act.
     * Built; no code reads it. */
    STATUS_CHECK_SET_UNREAD_7 = 7,
    /* Crystal, Dead, Undead, Jump, Petrify, Invite, Treasure, Critical. Script unit filters 3 and 5 leave out a
     * unit with one of them (filter 5 lets Undead through). */
    STATUS_CHECK_SET_SCRIPT_FILTER_EXCLUDED = 8,
    /* Confusion, Berserk, Poison, Stop, Sleep, Don't Move, Don't Act, Death Sentence. Removed from a mount when its
     * rider ends a move. */
    STATUS_CHECK_SET_MOUNT_REMOVAL = 9,
    /* Crystal, Dead, Petrify, Blood Suck, Treasure, Berserk, Chicken, Frog, Charm; set in code, not from the status
     * data. A chocobo with one of them cannot be mounted. */
    STATUS_CHECK_SET_UNMOUNTABLE = 10,
    STATUS_CHECK_SET_COUNT = 11,
} status_check_set_e;

#endif
