#ifndef FFT_STATUS_H
#define FFT_STATUS_H

#include "fft/unit_slots.h"
#include "psx/types.h"

/* Canonical status IDs. The owning byte is id >> 3 and its MSB-first mask is
 * 0x80 >> (id & 7); IDs are not byte masks. Entry 0 has no display name but
 * remains addressable in the target's status tables. */
typedef enum battle_status_id {
    BATTLE_STATUS_ID_UNNAMED_00 = 0x00,
    BATTLE_STATUS_ID_CRYSTAL = 0x01,
    BATTLE_STATUS_ID_DEAD = 0x02,
    BATTLE_STATUS_ID_UNDEAD = 0x03,
    BATTLE_STATUS_ID_CHARGING = 0x04,
    BATTLE_STATUS_ID_JUMP = 0x05,
    BATTLE_STATUS_ID_DEFENDING = 0x06,
    BATTLE_STATUS_ID_PERFORMING = 0x07,
    BATTLE_STATUS_ID_PETRIFY = 0x08,
    BATTLE_STATUS_ID_INVITE = 0x09,
    BATTLE_STATUS_ID_DARKNESS = 0x0a,
    BATTLE_STATUS_ID_CONFUSION = 0x0b,
    BATTLE_STATUS_ID_SILENCE = 0x0c,
    BATTLE_STATUS_ID_BLOOD_SUCK = 0x0d,
    BATTLE_STATUS_ID_CURSED = 0x0e,
    BATTLE_STATUS_ID_TREASURE = 0x0f,
    BATTLE_STATUS_ID_OIL = 0x10,
    BATTLE_STATUS_ID_FLOAT = 0x11,
    BATTLE_STATUS_ID_RERAISE = 0x12,
    BATTLE_STATUS_ID_TRANSPARENT = 0x13,
    BATTLE_STATUS_ID_BERSERK = 0x14,
    BATTLE_STATUS_ID_CHICKEN = 0x15,
    BATTLE_STATUS_ID_FROG = 0x16,
    BATTLE_STATUS_ID_CRITICAL = 0x17,
    BATTLE_STATUS_ID_POISON = 0x18,
    BATTLE_STATUS_ID_REGEN = 0x19,
    BATTLE_STATUS_ID_PROTECT = 0x1a,
    BATTLE_STATUS_ID_SHELL = 0x1b,
    BATTLE_STATUS_ID_HASTE = 0x1c,
    BATTLE_STATUS_ID_SLOW = 0x1d,
    BATTLE_STATUS_ID_STOP = 0x1e,
    BATTLE_STATUS_ID_WALL = 0x1f,
    BATTLE_STATUS_ID_FAITH = 0x20,
    BATTLE_STATUS_ID_INNOCENT = 0x21,
    BATTLE_STATUS_ID_CHARM = 0x22,
    BATTLE_STATUS_ID_SLEEP = 0x23,
    BATTLE_STATUS_ID_DONT_MOVE = 0x24,
    BATTLE_STATUS_ID_DONT_ACT = 0x25,
    BATTLE_STATUS_ID_REFLECT = 0x26,
    BATTLE_STATUS_ID_DEATH_SENTENCE = 0x27,
} battle_status_id_e;

enum {
    BATTLE_STATUS_COUNT = 40,
    BATTLE_STATUS_BYTE_COUNT = 5,
    BATTLE_TIMED_STATUS_COUNT = 16,
};

#define BATTLE_STATUS_BYTE_INDEX(id) ((id) >> 3)
#define BATTLE_STATUS_BYTE_MASK(id)  ((u8)(0x80U >> ((id) & 7)))
/* Event infliction helpers number bits LSB-first, unlike stored status IDs. */
#define BATTLE_STATUS_LSB_INDEX(id) (((id) & ~7) | (7 - ((id) & 7)))
#define BATTLE_STATUS_LSB_MASK(id)  ((u8)(1U << ((id) & 7)))
/* The special-status callback table reserves index 0; timed slots begin at Poison. */
#define BATTLE_STATUS_HANDLER_INDEX(id) ((id) + 1)
#define BATTLE_STATUS_CT_INDEX(id)      ((id) - BATTLE_STATUS_ID_POISON)
#define BATTLE_STATUS_PACKED_MASK(id)   ((u32)BATTLE_STATUS_BYTE_MASK(id) << (BATTLE_STATUS_BYTE_INDEX(id) * 8))

typedef struct status_effect_data {
    u8 unknown_00[2];
    u8 order;
    u8 ct;
    u8 flags_1;
    u8 flags_2;
    u8 cancels[BATTLE_STATUS_BYTE_COUNT];    /* 0x06; statuses removed when this one is inflicted */
    u8 cant_stack[BATTLE_STATUS_BYTE_COUNT]; /* 0x0b; current statuses that block this one */
} status_effect_data_t;

typedef char status_effect_data_size_must_be_0x10[(sizeof(status_effect_data_t) == 0x10) ? 1 : -1];

/* Flags that build the shared status-check sets during main initialization.
 * Provisional names remain for sets whose common runtime policy is not yet
 * established. */
typedef enum status_effect_flags_1 {
    STATUS_EFFECT_FLAG_1_KO = 0x01,
    STATUS_EFFECT_FLAG_1_PROVISIONAL_TEAM_LOSS_ENEMY = 0x02,
    STATUS_EFFECT_FLAG_1_PROVISIONAL_TEAM_LOSS_ALLY = 0x04,
    STATUS_EFFECT_FLAG_1_FREEZE_CT = 0x80,
} status_effect_flags_1_e;

typedef enum status_effect_flags_2 {
    STATUS_EFFECT_FLAG_2_IMMORTAL_IMMUNITY = 0x01,
    STATUS_EFFECT_FLAG_2_FORMATION_IMMUNITY = 0x02,
    STATUS_EFFECT_FLAG_2_PROVISIONAL_CHECK_SET_7 = 0x04,
    STATUS_EFFECT_FLAG_2_PROVISIONAL_CHECK_SET_8 = 0x08,
    STATUS_EFFECT_FLAG_2_MOUNT_REMOVAL = 0x10,
    STATUS_EFFECT_FLAG_2_IGNORE_ATTACKS = 0x20,
    STATUS_EFFECT_FLAG_2_PREVENT_REACTION = 0x80,
} status_effect_flags_2_e;

typedef enum main_status_check_set {
    MAIN_STATUS_CHECK_SET_KO = 0,
    MAIN_STATUS_CHECK_SET_PROVISIONAL_TEAM_LOSS_ENEMY = 1,
    MAIN_STATUS_CHECK_SET_PROVISIONAL_TEAM_LOSS_ALLY = 2,
    MAIN_STATUS_CHECK_SET_FREEZE_CT = 3,
    MAIN_STATUS_CHECK_SET_PREVENT_REACTION = 4,
    MAIN_STATUS_CHECK_SET_IMMORTAL_IMMUNITY = 5,
    MAIN_STATUS_CHECK_SET_FORMATION_IMMUNITY = 6,
    MAIN_STATUS_CHECK_SET_PROVISIONAL_7 = 7,
    MAIN_STATUS_CHECK_SET_PROVISIONAL_8 = 8,
    MAIN_STATUS_CHECK_SET_MOUNT_REMOVAL = 9,
    MAIN_STATUS_CHECK_SET_UNMOUNTABLE = 10,
    MAIN_STATUS_CHECK_SET_COUNT = 11,
} main_status_check_set_e;

/* Shared event snapshot for all 21 simulation slots. WORLD 0x800fb9ec writes
 * the saved fields; WORLD 0x800fbd14 and BATTLE 0x801488a8 restore them and
 * apply intervening status differences through BATTLE callbacks. */
typedef struct unit_status_staging {
    u8 innate[BATTLE_UNIT_SLOT_COUNT][BATTLE_STATUS_BYTE_COUNT];
    u8 current[BATTLE_UNIT_SLOT_COUNT][BATTLE_STATUS_BYTE_COUNT];    /* 0x069 */
    u8 inflicted[BATTLE_UNIT_SLOT_COUNT][BATTLE_STATUS_BYTE_COUNT];  /* 0x0d2 */
    u8 removed[BATTLE_UNIT_SLOT_COUNT][BATTLE_STATUS_BYTE_COUNT];    /* 0x13b */
    u8 added[BATTLE_UNIT_SLOT_COUNT][BATTLE_STATUS_BYTE_COUNT];      /* 0x1a4 */
    u8 status_ct[BATTLE_UNIT_SLOT_COUNT][BATTLE_TIMED_STATUS_COUNT]; /* 0x20d */
    u8 charged_ability_ct[BATTLE_UNIT_SLOT_COUNT];                   /* 0x35d */
    u8 death_counter[BATTLE_UNIT_SLOT_COUNT];                        /* 0x372 */
    u8 team_flags[BATTLE_UNIT_SLOT_COUNT];                           /* 0x387 */
    u8 state[BATTLE_UNIT_SLOT_COUNT];                                /* 0x39c; 0 not staged, 1 saved, 2 restored */
    u8 exit_mode[BATTLE_UNIT_SLOT_COUNT];                            /* 0x3b1 */
    u16 hp[BATTLE_UNIT_SLOT_COUNT];                                  /* 0x3c6 */
    u16 flags;                                                       /* 0x3f0; status-specific follow-up flags */
} unit_status_staging_t;

typedef char unit_status_staging_size_must_be_0x3f2[sizeof(unit_status_staging_t) == 0x3f2 ? 1 : -1];

extern const status_effect_data_t g_main_status_effect_data[BATTLE_STATUS_COUNT];
/* Byte-stride alias of g_main_status_effect_data[0].flags_1. The target uses
 * this address directly while walking the 0x10-byte records. */
extern u8 g_main_status_check_sets[MAIN_STATUS_CHECK_SET_COUNT][BATTLE_STATUS_BYTE_COUNT];
#endif
