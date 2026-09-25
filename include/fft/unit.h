#ifndef FFT_UNIT_H
#define FFT_UNIT_H

/* Unit records shared across modules: party, ENTD and battle stats, status and identity. */

#include "fft/data.h"
#include "psx/types.h"

/* ability flags */
/* Learned support and movement abilities are stored as MSB-first bitsets in
 * battle_stats_t. The same four support bytes are copied to party_data_t at
 * 0x124, so these values are shared by MAIN, BATTLE, and WORLD. */
typedef enum battle_support_set_1 {
    BATTLE_SUPPORT_SET_1_EQUIP_ARMOR = 0x80,
    BATTLE_SUPPORT_SET_1_EQUIP_SHIELD = 0x40,
    BATTLE_SUPPORT_SET_1_EQUIP_SWORD = 0x20,
    BATTLE_SUPPORT_SET_1_EQUIP_KATANA = 0x10,
    BATTLE_SUPPORT_SET_1_EQUIP_CROSSBOW = 0x08,
    BATTLE_SUPPORT_SET_1_EQUIP_SPEAR = 0x04,
    BATTLE_SUPPORT_SET_1_EQUIP_AXE = 0x02,
    BATTLE_SUPPORT_SET_1_EQUIP_GUN = 0x01,
} battle_unit_support_set_1_e;

typedef enum battle_support_set_3 {
    BATTLE_SUPPORT_SET_3_TRAIN = 0x80,
    BATTLE_SUPPORT_SET_3_SECRET_HUNT = 0x40,
    BATTLE_SUPPORT_SET_3_MARTIAL_ARTS = 0x20,
    BATTLE_SUPPORT_SET_3_MONSTER_TALK = 0x10,
    BATTLE_SUPPORT_SET_3_THROW_ITEM = 0x08,
    BATTLE_SUPPORT_SET_3_MAINTENANCE = 0x04,
    BATTLE_SUPPORT_SET_3_TWO_HANDS = 0x02,
    BATTLE_SUPPORT_SET_3_TWO_SWORDS = 0x01,
} battle_unit_support_set_3_e;

/* action */
/*
 * Temporary action results, 0x2c bytes at battle_stats_t+0x18c (0x18c..0x1b7).
 * The g_target_current_action / g_attacker_current_action pointers address this
 * block directly.  status_infliction/status_removal mirror the five
 * status_sets.current bytes.
 */
typedef struct battle_action_data {
    u8 hit;          /* 0x00; 1 hit, 0 miss */
    u8 critical;     /* 0x01 */
    u8 miss_type;    /* 0x02; battle_action_miss_type_e */
    u8 item_lost;    /* 0x03; potion / break / steal / draw out */
    u16 hp_damage;   /* 0x04 */
    u16 hp_healing;  /* 0x06 */
    u16 mp_damage;   /* 0x08 */
    u16 mp_healing;  /* 0x0a */
    s16 gil_change;  /* 0x0c; stolen / lost */
    u16 reaction_id; /* 0x0e */
    /* Target readers and writers consistently access 0x10-0x11 as one halfword. */
    u16 special_effect;       /* 0x10; battle_action_special_effect_e */
    u8 sp_change;             /* 0x12 */
    u8 ct_change;             /* 0x13 */
    u8 pa_change;             /* 0x14 */
    u8 ma_change;             /* 0x15 */
    u8 brave_change;          /* 0x16 */
    u8 faith_change;          /* 0x17 */
    u8 status_change;         /* 0x18 */
    u8 equipment_destroyed;   /* 0x19; battle_action_equipment_slot_mask_e; 0xfe is a selection sentinel */
    u8 stolen_item_id;        /* 0x1a */
    u8 status_infliction[5];  /* 0x1b */
    u8 status_removal[5];     /* 0x20 */
    u8 attack_type;           /* 0x25; battle_action_type_e */
    u16 last_received_attack; /* 0x26; reaction data, varies by ability */
    u8 exp_change;            /* 0x28; Steal EXP */
    u8 jp_change;             /* 0x29; Move-JP Up / JP stolen */
    /* Written as a halfword everywhere (battle_formula_apply_status lhu/sh,
     * arrow_guard_usability, battle_formula_apply_catch, force_attack_miss). */
    u16 attack_accuracy; /* 0x2a */
} battle_action_data_t;
typedef char battle_action_data_size_must_be_0x2c[(sizeof(battle_action_data_t) == 0x2c) ? 1 : -1];

/* character identity */
/*
 * ENTD/party sprite_set and battle character_identity share this byte domain.
 * main_unit_get_spritesheet_palette (0x8005e120) returns values below 0x80
 * directly, but resolves 0x80/0x81 by job and 0x82 from the monster's job.
 * Thus these selectors are NOT resolved SPR IDs: SPR 0x80 is male Calculator,
 * whereas character identity 0x80 is generic male.
 *
 * Unknown 0x5d-0x5f remain unnamed. Duplicate sprites retain distinct IDs.
 * Keep serialized fields u8; this enum names their values, not their storage.
 */
typedef enum character_identity {
    /* Zero marks an absent ENTD entry; the sprite resolver itself accepts it. */
    CHARACTER_IDENTITY_ENTD_NONE = 0x00,
    CHARACTER_IDENTITY_RAMZA_CHAPTER_1 = 0x01,
    CHARACTER_IDENTITY_RAMZA_CHAPTER_2_3 = 0x02,
    CHARACTER_IDENTITY_RAMZA_CHAPTER_4 = 0x03,
    CHARACTER_IDENTITY_DELITA_CHAPTER_1 = 0x04,
    CHARACTER_IDENTITY_DELITA_CHAPTER_2_3 = 0x05,
    CHARACTER_IDENTITY_DELITA_CHAPTER_4 = 0x06,
    CHARACTER_IDENTITY_ALGUS = 0x07,
    CHARACTER_IDENTITY_ZALBAG = 0x08,
    CHARACTER_IDENTITY_DYCEDARG = 0x09,
    CHARACTER_IDENTITY_LARG = 0x0a,
    CHARACTER_IDENTITY_GOLTANA = 0x0b,
    CHARACTER_IDENTITY_OVELIA = 0x0c,
    CHARACTER_IDENTITY_ORLANDU = 0x0d,
    CHARACTER_IDENTITY_FUNERAL = 0x0e,
    CHARACTER_IDENTITY_REIS_HUMAN = 0x0f,
    CHARACTER_IDENTITY_ZALMO = 0x10,
    CHARACTER_IDENTITY_GAFGARION_ENEMY = 0x11,
    CHARACTER_IDENTITY_MALAK_12 = 0x12,
    CHARACTER_IDENTITY_SIMON = 0x13,
    CHARACTER_IDENTITY_ALMA_BATTLE = 0x14,
    CHARACTER_IDENTITY_OLAN = 0x15,
    CHARACTER_IDENTITY_MUSTADIO_JOIN = 0x16,
    CHARACTER_IDENTITY_GAFGARION_GUEST = 0x17,
    CHARACTER_IDENTITY_DRACLAU = 0x18,
    CHARACTER_IDENTITY_RAFA_GUEST = 0x19,
    CHARACTER_IDENTITY_MALAK = 0x1a,
    CHARACTER_IDENTITY_ELMDOR = 0x1b,
    CHARACTER_IDENTITY_TETA = 0x1c,
    CHARACTER_IDENTITY_BARINTEN = 0x1d,
    CHARACTER_IDENTITY_AGRIAS_JOIN = 0x1e,
    CHARACTER_IDENTITY_BEOWULF = 0x1f,
    CHARACTER_IDENTITY_WIEGRAF_CHAPTER_1 = 0x20,
    CHARACTER_IDENTITY_BALMAFULA = 0x21,
    CHARACTER_IDENTITY_MUSTADIO_GUEST = 0x22,
    CHARACTER_IDENTITY_RUDVICH = 0x23,
    CHARACTER_IDENTITY_VORMAV = 0x24,
    CHARACTER_IDENTITY_ROFEL = 0x25,
    CHARACTER_IDENTITY_IZLUDE = 0x26,
    CHARACTER_IDENTITY_KLETIAN = 0x27,
    CHARACTER_IDENTITY_WIEGRAF_CHAPTER_2_3 = 0x28,
    CHARACTER_IDENTITY_RAFA_JOIN = 0x29,
    CHARACTER_IDENTITY_MELIADOUL_JOIN = 0x2a,
    CHARACTER_IDENTITY_BALK = 0x2b,
    CHARACTER_IDENTITY_ALMA_2C = 0x2c,
    CHARACTER_IDENTITY_CELIA = 0x2d,
    CHARACTER_IDENTITY_LEDE = 0x2e,
    CHARACTER_IDENTITY_MELIADOUL_ENEMY = 0x2f,
    CHARACTER_IDENTITY_ALMA_EVENTS = 0x30,
    CHARACTER_IDENTITY_AJORA = 0x31,
    CHARACTER_IDENTITY_CLOUD = 0x32,
    CHARACTER_IDENTITY_ZALBAG_ZOMBIE = 0x33,
    CHARACTER_IDENTITY_AGRIAS_GUEST = 0x34,
    CHARACTER_IDENTITY_FIXED_FEMALE_CHEMIST_35 = 0x35,
    CHARACTER_IDENTITY_FIXED_FEMALE_PRIEST_36 = 0x36,
    CHARACTER_IDENTITY_FIXED_MALE_WIZARD_37 = 0x37,
    CHARACTER_IDENTITY_FIXED_MALE_ORACLE_38 = 0x38,
    CHARACTER_IDENTITY_FIXED_MALE_SQUIRE_39 = 0x39,
    CHARACTER_IDENTITY_CELIA_3A = 0x3a,
    CHARACTER_IDENTITY_LEDE_3B = 0x3b,
    CHARACTER_IDENTITY_VELIUS = 0x3c,
    CHARACTER_IDENTITY_FIXED_MALE_KNIGHT_3D = 0x3d,
    CHARACTER_IDENTITY_ZALERA = 0x3e,
    CHARACTER_IDENTITY_FIXED_MALE_ARCHER_3F = 0x3f,
    CHARACTER_IDENTITY_HASHMALUM = 0x40,
    CHARACTER_IDENTITY_ALTIMA_FIRST_FORM = 0x41,
    CHARACTER_IDENTITY_FIXED_MALE_WIZARD_42 = 0x42,
    CHARACTER_IDENTITY_QUEKLAIN = 0x43,
    CHARACTER_IDENTITY_FIXED_FEMALE_TIME_MAGE_44 = 0x44,
    CHARACTER_IDENTITY_ADRAMELK = 0x45,
    CHARACTER_IDENTITY_FIXED_MALE_ORACLE_46 = 0x46,
    CHARACTER_IDENTITY_FIXED_FEMALE_SUMMONER_47 = 0x47,
    CHARACTER_IDENTITY_REIS_DRAGON = 0x48,
    CHARACTER_IDENTITY_ALTIMA_SECOND_FORM = 0x49,
    CHARACTER_IDENTITY_GENERIC_FIRST = 0x4a,
    CHARACTER_IDENTITY_BOY = CHARACTER_IDENTITY_GENERIC_FIRST,
    CHARACTER_IDENTITY_GIRL = 0x4b,
    CHARACTER_IDENTITY_YOUNG_MAN = 0x4c,
    CHARACTER_IDENTITY_YOUNG_WOMAN = 0x4d,
    CHARACTER_IDENTITY_MIDDLE_AGED_MAN = 0x4e,
    CHARACTER_IDENTITY_MIDDLE_AGED_WOMAN = 0x4f,
    CHARACTER_IDENTITY_OLD_MAN = 0x50,
    CHARACTER_IDENTITY_OLD_WOMAN = 0x51,
    CHARACTER_IDENTITY_OLD_FUNERAL_MAN = 0x52,
    CHARACTER_IDENTITY_OLD_FUNERAL_WOMAN = 0x53,
    CHARACTER_IDENTITY_FUNERAL_MAN = 0x54,
    CHARACTER_IDENTITY_FUNERAL_WOMAN = 0x55,
    CHARACTER_IDENTITY_FUNERAL_PRIEST = 0x56,
    CHARACTER_IDENTITY_FIXED_MALE_SQUIRE_57 = 0x57,
    CHARACTER_IDENTITY_FIXED_MALE_SQUIRE_58 = 0x58,
    CHARACTER_IDENTITY_FIXED_MALE_SQUIRE_59 = 0x59,
    CHARACTER_IDENTITY_FIXED_MALE_SQUIRE_5A = 0x5a,
    CHARACTER_IDENTITY_FIXED_MALE_SQUIRE_5B = 0x5b,
    CHARACTER_IDENTITY_FIXED_MALE_SQUIRE_5C = 0x5c,

    /* Fixed graphics, independent of the unit's current job. */
    CHARACTER_IDENTITY_FIXED_MALE_SQUIRE = 0x60,
    CHARACTER_IDENTITY_FIXED_FEMALE_SQUIRE = 0x61,
    CHARACTER_IDENTITY_FIXED_MALE_CHEMIST = 0x62,
    CHARACTER_IDENTITY_FIXED_FEMALE_CHEMIST = 0x63,
    CHARACTER_IDENTITY_FIXED_MALE_KNIGHT = 0x64,
    CHARACTER_IDENTITY_FIXED_FEMALE_KNIGHT = 0x65,
    CHARACTER_IDENTITY_FIXED_MALE_ARCHER = 0x66,
    CHARACTER_IDENTITY_FIXED_FEMALE_ARCHER = 0x67,
    CHARACTER_IDENTITY_FIXED_MALE_MONK = 0x68,
    CHARACTER_IDENTITY_FIXED_FEMALE_MONK = 0x69,
    CHARACTER_IDENTITY_FIXED_MALE_PRIEST = 0x6a,
    CHARACTER_IDENTITY_FIXED_FEMALE_PRIEST = 0x6b,
    CHARACTER_IDENTITY_FIXED_MALE_WIZARD = 0x6c,
    CHARACTER_IDENTITY_FIXED_FEMALE_WIZARD = 0x6d,
    CHARACTER_IDENTITY_FIXED_MALE_TIME_MAGE = 0x6e,
    CHARACTER_IDENTITY_FIXED_FEMALE_TIME_MAGE = 0x6f,
    CHARACTER_IDENTITY_FIXED_MALE_SUMMONER = 0x70,
    CHARACTER_IDENTITY_FIXED_FEMALE_SUMMONER = 0x71,
    CHARACTER_IDENTITY_FIXED_MALE_THIEF = 0x72,
    CHARACTER_IDENTITY_FIXED_FEMALE_THIEF = 0x73,
    CHARACTER_IDENTITY_FIXED_MALE_MEDIATOR = 0x74,
    CHARACTER_IDENTITY_FIXED_FEMALE_MEDIATOR = 0x75,
    CHARACTER_IDENTITY_FIXED_MALE_ORACLE = 0x76,
    CHARACTER_IDENTITY_FIXED_FEMALE_ORACLE = 0x77,
    CHARACTER_IDENTITY_FIXED_MALE_GEOMANCER = 0x78,
    CHARACTER_IDENTITY_FIXED_FEMALE_GEOMANCER = 0x79,
    CHARACTER_IDENTITY_FIXED_MALE_LANCER = 0x7a,
    CHARACTER_IDENTITY_FIXED_FEMALE_LANCER = 0x7b,
    CHARACTER_IDENTITY_FIXED_MALE_SAMURAI = 0x7c,
    CHARACTER_IDENTITY_FIXED_FEMALE_SAMURAI = 0x7d,
    CHARACTER_IDENTITY_FIXED_MALE_NINJA = 0x7e,
    CHARACTER_IDENTITY_FIXED_FEMALE_NINJA = 0x7f,

    /* Job-dependent selectors, not fixed graphics. */
    CHARACTER_IDENTITY_GENERIC_MALE = 0x80,
    CHARACTER_IDENTITY_GENERIC_FEMALE = 0x81,
    CHARACTER_IDENTITY_MONSTER = 0x82,
} character_identity_e;

/* A selector range, not a count of valid identities. Values above 0x82 have
 * a separate fallback in the target sprite resolver; do not reject them. */
enum {
    CHARACTER_IDENTITY_RAMZA_END = CHARACTER_IDENTITY_DELITA_CHAPTER_1,
    CHARACTER_IDENTITY_HIGH_BIT_MASK = 0x80,
    CHARACTER_IDENTITY_SELECTOR_FIRST = CHARACTER_IDENTITY_GENERIC_MALE,
    CHARACTER_IDENTITY_GENERIC_HUMAN_COUNT = 2,
    CHARACTER_IDENTITY_SELECTOR_COUNT = 3,
};

/* equipment */
typedef enum battle_equipment_categories_1 {
    BATTLE_EQUIPMENT_SET_1_SWORD = 0x10,
    BATTLE_EQUIPMENT_SET_1_KATANA = 0x04,
    BATTLE_EQUIPMENT_SET_1_AXE = 0x02,
} battle_unit_equipment_categories_1_e;

typedef enum battle_equipment_categories_2 {
    BATTLE_EQUIPMENT_SET_2_CROSSBOW = 0x10,
    BATTLE_EQUIPMENT_SET_2_SPEAR = 0x01,
    BATTLE_EQUIPMENT_SET_2_GUN = 0x20,
} battle_unit_equipment_categories_2_e;

typedef enum battle_equipment_categories_3 {
    BATTLE_EQUIPMENT_SET_3_ARMOR = 0x01,
    BATTLE_EQUIPMENT_SET_3_HAIR_ORNAMENT = 0x02,
    BATTLE_EQUIPMENT_SET_3_HELM = 0x08,
    BATTLE_EQUIPMENT_SET_3_SHIELD = 0x10,
    BATTLE_EQUIPMENT_SET_3_BAG = 0x40,
} battle_unit_equipment_categories_3_e;

typedef enum battle_equipment_categories_4 {
    BATTLE_EQUIPMENT_SET_4_PERFUME = 0x01,
} battle_unit_equipment_categories_4_e;

/*
 * Seven-slot equipment order serialized by party_data_t and battle_stats_t.
 * This differs from the five-slot right-hand-first order used by menu records.
 */
typedef enum unit_equipment_slot {
    UNIT_EQUIPMENT_SLOT_HEAD = 0,
    UNIT_EQUIPMENT_SLOT_BODY = 1,
    UNIT_EQUIPMENT_SLOT_ACCESSORY = 2,
    UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON = 3,
    UNIT_EQUIPMENT_SLOT_RIGHT_HAND_SHIELD = 4,
    UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON = 5,
    UNIT_EQUIPMENT_SLOT_LEFT_HAND_SHIELD = 6,
    UNIT_EQUIPMENT_SLOT_COUNT = 7,
} unit_equipment_slot_e;

/* job */
/* Serialized unit-career layout shared by party records and battle stats.
 * main_unit_generate_out_of_battle (0x80059ffc) clears the 0x96-byte span,
 * while main_job_add_proposition_jp and main_unit_init_battle_data establish
 * the little-endian byte order of each serialized JP value. Each packed level
 * byte stores the even career index in its high nibble and the following odd
 * index in its low nibble. */
enum {
    UNIT_CAREER_JOB_COUNT = JOB_ID_GENERIC_COUNT,
    UNIT_CAREER_JOB_INDEX_BARD = JOB_ID_BARD - JOB_ID_GENERIC_FIRST,
    UNIT_CAREER_JOB_INDEX_DANCER = JOB_ID_DANCER - JOB_ID_GENERIC_FIRST,
    UNIT_CAREER_JOB_INDEX_MIME = JOB_ID_MIME - JOB_ID_GENERIC_FIRST,
    UNIT_UNLOCKED_JOB_BYTE_COUNT = 3,
    /* Mime has JP and a packed job level, but no learned-ability row. */
    UNIT_LEARNED_ABILITY_JOB_COUNT = UNIT_CAREER_JOB_COUNT - 1,
    UNIT_LEARNED_ABILITY_BYTES_PER_JOB = 3,
    UNIT_LEARNED_ABILITY_BYTE_COUNT = UNIT_LEARNED_ABILITY_JOB_COUNT * UNIT_LEARNED_ABILITY_BYTES_PER_JOB,
    UNIT_JOBS_PER_LEVEL_BYTE = 2,
    UNIT_JOB_LEVEL_BYTE_COUNT = UNIT_CAREER_JOB_COUNT / UNIT_JOBS_PER_LEVEL_BYTE,
    UNIT_JOB_LEVEL_BYTE_INDEX_CALCULATOR_BARD = UNIT_CAREER_JOB_INDEX_BARD / UNIT_JOBS_PER_LEVEL_BYTE,
    UNIT_JOB_LEVEL_BYTE_INDEX_DANCER_MIME = UNIT_CAREER_JOB_INDEX_DANCER / UNIT_JOBS_PER_LEVEL_BYTE,
    UNIT_JOB_LEVEL_LOW_NIBBLE_MASK = 0x0f,
    UNIT_JOB_LEVEL_HIGH_NIBBLE_MASK = 0xf0,
    UNIT_JOB_LEVEL_ONE_LOW_NIBBLE = 0x01,
    UNIT_JOB_LEVEL_ONE_HIGH_NIBBLE = 0x10,
    UNIT_JOB_LEVEL_ONE_BOTH_NIBBLES = 0x11,
    UNIT_JOB_POINT_BYTES_PER_JOB = 2,
    UNIT_JOB_POINT_BYTE_COUNT = UNIT_CAREER_JOB_COUNT * UNIT_JOB_POINT_BYTES_PER_JOB,
    UNIT_JOB_POINT_TABLE_PAIR_BYTE_COUNT = UNIT_JOB_POINT_BYTE_COUNT * 2,
    UNIT_CAREER_DATA_BYTE_COUNT = UNIT_UNLOCKED_JOB_BYTE_COUNT + UNIT_LEARNED_ABILITY_BYTE_COUNT
        + UNIT_JOB_LEVEL_BYTE_COUNT + UNIT_JOB_POINT_TABLE_PAIR_BYTE_COUNT,
};

typedef char assert_unit_career_data_byte_count[UNIT_CAREER_DATA_BYTE_COUNT == 0x96 ? 1 : -1];

/* zodiac */
enum {
    ZODIAC_SIGN_ORDINARY_COUNT = 12,
    ZODIAC_SIGN_COUNT = 13,
};

/* status */
/* SCUS 0x8005b5dc copies these three consecutive sets as one 15-byte block.
 * Keep a named aggregate so whole-block byte copies do not walk beyond the
 * first five-byte array. Individual status readers still use named sets. */
typedef struct battle_status_sets {
    u8 innate[5];
    u8 immunity[5];
    u8 current[5];
} battle_status_sets_t;
typedef char battle_status_sets_size_must_be_15[(sizeof(battle_status_sets_t) == 15) ? 1 : -1];

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

#define BATTLE_STATUS_BYTE_INDEX(id) ((id) >> 3)

#define BATTLE_STATUS_BYTE_MASK(id) ((u8)(0x80U >> ((id) & 7)))

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

/* The battle simulation owns 21 unit records. Battle-slot search routines use
 * the first value beyond that array as their no-unit sentinel. */
enum {
    BATTLE_UNIT_SLOT_COUNT = 21,
    BATTLE_UNIT_ID_NONE = BATTLE_UNIT_SLOT_COUNT,
};

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

/* unit */
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

typedef enum battle_unit_equipment_stat_index {
    BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_WEAPON_POWER = 0,
    BATTLE_UNIT_EQUIPMENT_STAT_LEFT_WEAPON_POWER = 1,
    BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_WEAPON_EVADE = 2,
    BATTLE_UNIT_EQUIPMENT_STAT_LEFT_WEAPON_EVADE = 3,
    BATTLE_UNIT_EQUIPMENT_STAT_ACCESSORY_PHYSICAL_EVADE = 4,
    BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_SHIELD_PHYSICAL_EVADE = 5,
    BATTLE_UNIT_EQUIPMENT_STAT_LEFT_SHIELD_PHYSICAL_EVADE = 6,
    BATTLE_UNIT_EQUIPMENT_STAT_CLASS_PHYSICAL_EVADE = 7,
    BATTLE_UNIT_EQUIPMENT_STAT_ACCESSORY_MAGIC_EVADE = 8,
    BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_SHIELD_MAGIC_EVADE = 9,
    BATTLE_UNIT_EQUIPMENT_STAT_LEFT_SHIELD_MAGIC_EVADE = 10,
    BATTLE_UNIT_EQUIPMENT_STAT_COUNT = 11,
} battle_unit_equipment_stat_index_e;

/*
 * Battle Stats are the simulation-side unit records.  They occupy 0x1c0
 * bytes, with 21 consecutive slots beginning at 0x801908cc in BATTLE.BIN.
 * Do not confuse this with Misc unit_t Data, the renderer-side record that
 * points back to battle_stats_t at offset 0x134.
 *
 * Unidentified areas intentionally remain explicit padding until evidence
 * supports a name and type.
 */
/* Packed day-of-year/zodiac word at unit offset 0x008. The halfword masks
 * in SCUS 0x8005ac1c and the bitfield stores at 0x8005b0d0 establish both views. */
typedef union battle_unit_birthday {
    u16 value;
    struct {
        u16 birthday : 9;
        u16 unknown : 3;
        u16 zodiac : 4;
    } fields;
} battle_unit_birthday_t;
typedef char battle_birthday_size_must_be_2[(sizeof(battle_unit_birthday_t) == 2) ? 1 : -1];

/*
 * Packed Y and facing data shared by ENTD definitions and runtime battle
 * records.  The US target copies the named bit groups independently.
 */
typedef union battle_unit_position {
    u16 raw;
    struct {
        u16 y : 8;
        u16 facing : 4;
        u16 spell_quote_skillsets : 2;
        u16 stepping_stone : 1;
        u16 higher_elevation : 1;
    } bits;
    u8 bytes[2]; /* bytes[0] is y; battle_unit_modify_entd_unit saves it with lbu/sb */
} battle_unit_position_t;
typedef char battle_unit_position_size_must_be_2[(sizeof(battle_unit_position_t) == 2) ? 1 : -1];

typedef struct battle_stats {
    u8 character_identity;            /* 0x000 */
    u8 entd_slot;                     /* 0x001; 0xff means absent */
    u8 formation_index;               /* 0x002 */
    u8 job_id;                        /* 0x003 */
    u8 sprite_palette;                /* 0x004 */
    u8 team_flags;                    /* 0x005 */
    u8 unit_flags;                    /* 0x006; mixed sex, monster, guest, and formation flags */
    u8 death_counter;                 /* 0x007 */
    battle_unit_birthday_t birthday;  /* 0x008; zodiac is the high nibble */
    u16 innate_abilities[4];          /* 0x00a */
    u8 primary_skillset;              /* 0x012 */
    u8 secondary_skillset;            /* 0x013 */
    u16 reaction_ability;             /* 0x014 */
    u16 support_ability;              /* 0x016 */
    u16 movement_ability;             /* 0x018 */
    u8 equipment[7];                  /* 0x01a; unit_equipment_slot_e */
    u8 experience;                    /* 0x021 */
    u8 level;                         /* 0x022 */
    u8 original_brave;                /* 0x023 */
    u8 brave;                         /* 0x024 */
    u8 original_faith;                /* 0x025 */
    u8 faith;                         /* 0x026 */
    u8 transparent_removal_flag;      /* 0x027 */
    u16 hp;                           /* 0x028 */
    u16 max_hp;                       /* 0x02a */
    u16 mp;                           /* 0x02c */
    u16 max_mp;                       /* 0x02e */
    u8 base_attributes[3];            /* 0x030; unit_attribute_index_e */
    u8 equipment_attributes[3];       /* 0x033; unit_attribute_index_e */
    u8 attributes[3];                 /* 0x036; unit_attribute_index_e */
    u8 ct;                            /* 0x039 */
    u8 move;                          /* 0x03a */
    u8 jump;                          /* 0x03b */
    u8 equipment_stats[11];           /* 0x03c; battle_unit_equipment_stat_index_e */
    u8 x;                             /* 0x047 */
    battle_unit_position_t position;  /* 0x048 */
    u8 equipment_categories[4];       /* 0x04a */
    battle_status_sets_t status_sets; /* 0x04e; innate / immunity / current */
    u8 status_ct[16];                 /* 0x05d */
    u8 elemental_affinity[5];         /* 0x06d; elemental_affinity_index_e */
    u8 raw_stats[0x19];               /* 0x072 */
    u8 reaction_abilities[4];         /* 0x08b; battle_unit_reaction_set_N_e */
    u8 support_abilities[4];          /* 0x08f */
    u8 movement_abilities[3];         /* 0x093 */
    u8 unlocked_jobs[3];              /* 0x096 */
    u8 learned_abilities[57];         /* 0x099..0x0d1; three bytes per learned-ability row */
    u8 job_levels[10];                /* 0x0d2; two packed job-level nibbles per byte */
    u16 job_points[20];               /* 0x0dc; SCUS party-to-battle copy at 0x8005b0d0 */
    u16 total_job_points[20];         /* 0x104; contiguous with current JP, through 0x12b */
    /* main_unit_store_character_names (0x8005c984) copies 16/16/8/8
     * bytes to these four destinations. All names use the game's encoding. */
    u8 name[16];                       /* 0x12c */
    u8 job_name[16];                   /* 0x13c */
    u8 primary_skillset_name[8];       /* 0x14c */
    u8 secondary_skillset_name[8];     /* 0x154 */
    u8 ko_count;                       /* 0x15c */
    u8 charged_ability_ct;             /* 0x15d */
    u8 graphic_variant;                /* 0x15e; copied from job data and consumed by WORLD formation graphics */
    u8 spritesheet_id;                 /* 0x15f */
    u8 job_portrait_palette;           /* 0x160 */
    u8 unit_id;                        /* 0x161 */
    u8 base_job_skillset;              /* 0x162 */
    u8 war_trophy;                     /* 0x163 */
    u8 bonus_money_modifier;           /* 0x164 */
    u8 ai_target_x;                    /* 0x165 */
    u8 ai_target_y;                    /* 0x166 */
    u8 ai_flags;                       /* 0x167 */
    u8 prioritized_target;             /* 0x168 */
    u8 ai_targeting_flags_1;           /* 0x169; copied to battle_ai_unit_decision_t +0x06 */
    u8 ai_targeting_flags_2;           /* 0x16a; battle_ai_targeting_flags_2_e */
    u8 entd_unknown_right;             /* 0x16b */
    u16 quote_name_id;                 /* 0x16c */
    u8 action_actor_id;                /* 0x16e */
    u8 last_skillset_id;               /* 0x16f */
    s16 last_ability_id;               /* 0x170 */
    u16 calculator_type_ability;       /* 0x172 */
    u16 calculator_multiplier_ability; /* 0x174 */
    u16 used_item_or_equipment;        /* 0x176 */
    u8 action_target_kind;             /* 0x178; battle_action_target_kind_e */
    u8 action_target_id;               /* 0x179 */
    s16 action_target_x;               /* 0x17a */
    s16 action_target_elevation;       /* 0x17c */
    s16 action_target_y;               /* 0x17e */
    u8 death_on_chocobo;               /* 0x180 */
    u8 action_tile_flags;              /* 0x181 */
    u8 mount_info;                     /* 0x182 */
    u8 existence;                      /* 0x183; battle_unit_existence_state_e */
    u8 equipped_flags;                 /* 0x184; battle_unit_equipped_flags_e */
    u8 _pad185;
    u8 has_turn;                 /* 0x186 */
    u8 movement_taken;           /* 0x187 */
    u8 action_taken;             /* 0x188 */
    u8 ability_outcome;          /* 0x189 */
    u8 misc_unit_id;             /* 0x18a */
    u8 ability_ct;               /* 0x18b */
    battle_action_data_t action; /* 0x18c through 0x1b7 */
    u8 auto_battle_setting;      /* 0x1b8 */
    u8 auto_battle_target;       /* 0x1b9 */
    u8 initial_team_flags;       /* 0x1ba */
    u8 inflicted_status[5];      /* 0x1bb */
} battle_stats_t;
typedef char battle_stats_size_must_be_0x1c0[(sizeof(battle_stats_t) == 0x1c0) ? 1 : -1];
typedef char battle_stats_innate_status_must_be_0x4e[((unsigned long)&((battle_stats_t*)0)->status_sets.innate == 0x4e)
        ? 1
        : -1];
typedef char battle_stats_status_immunity_must_be_0x53
    [((unsigned long)&((battle_stats_t*)0)->status_sets.immunity == 0x53) ? 1 : -1];
typedef char
    battle_stats_current_status_must_be_0x58[((unsigned long)&((battle_stats_t*)0)->status_sets.current == 0x58) ? 1
                                                                                                                 : -1];

typedef union entd_unit_position {
    u16 raw;
    struct {
        u16 y : 8;
        u16 facing : 2;
        u16 _bits10_11 : 2;
        u16 spell_quote_skillsets : 2;
        u16 stepping_stone : 1;
        u16 higher_elevation : 1;
    } bits;
} entd_unit_position_t;
typedef char entd_unit_position_size_must_be_2[(sizeof(entd_unit_position_t) == 2) ? 1 : -1];

enum {
    ENTD_UNITS_PER_ENCOUNTER = 16,
    ENTD_ENCOUNTERS_PER_FILE = 128,
};

/* The persistent roster at 0x80057f74 contains 16 formation slots followed
 * by four guest slots, each with the party_data_t 0x100-byte stride. */
enum {
    PARTY_ROSTER_SLOT_COUNT = 20,
    PARTY_GUEST_SLOT_FIRST = 16,
};

/* party_data_t.party_id sentinel: `0xff` means the unit does not appear on
 * Formation; tested by the roster scans in src/main and the WORLD/WLDCORE
 * party walks. */
enum {
    PARTY_ID_NONE = 0xff,
};

/*
 * An ENTD entry is the static 0x28-byte encounter definition read from an
 * ENTD#.ENT file.  It is input to the SCUS unit initializers; it is neither a
 * persistent party_data_t record nor a runtime battle_stats_t record.
 * The current-ENTD pointer at 0x80066238 selects one entry, not an encounter.
 *
 * Names below are established by target SCUS initialization routines.
 * The remaining bytes intentionally retain neutral names until their runtime
 * consumers are matched.
 */
typedef enum entd_equipment_slot {
    ENTD_EQUIPMENT_SLOT_HEAD = 0,
    ENTD_EQUIPMENT_SLOT_BODY = 1,
    ENTD_EQUIPMENT_SLOT_ACCESSORY = 2,
    ENTD_EQUIPMENT_SLOT_RIGHT_HAND = 3,
    ENTD_EQUIPMENT_SLOT_LEFT_HAND = 4,
    ENTD_EQUIPMENT_SLOT_COUNT = 5,
} entd_equipment_slot_e;

typedef struct entd_unit {
    u8 sprite_set;                           /* 0x00 */
    u8 unit_flags;                           /* 0x01; unit_flags_e */
    u8 name_id;                              /* 0x02 */
    u8 level;                                /* 0x03 */
    u8 birthday[2];                          /* 0x04; month, day (read bytewise) */
    u8 bravery;                              /* 0x06 */
    u8 faith;                                /* 0x07 */
    u8 job_unlock;                           /* 0x08 */
    u8 job_level;                            /* 0x09 */
    u8 job_id;                               /* 0x0a */
    u8 secondary_skillset;                   /* 0x0b */
    u16 reaction_ability;                    /* 0x0c */
    u16 support_ability;                     /* 0x0e */
    u16 movement_ability;                    /* 0x10 */
    u8 equipment[ENTD_EQUIPMENT_SLOT_COUNT]; /* 0x12 */
    u8 palette;                              /* 0x17 */
    u8 battle_flags;                         /* 0x18 */
    s8 x;                                    /* 0x19 */
    entd_unit_position_t position;           /* 0x1a */
    u8 experience;                           /* 0x1c */
    u8 primary_skillset;                     /* 0x1d */
    u8 war_trophy;                           /* 0x1e; 0x8005e478 copies to battle +0x163 */
    u8 bonus_money_modifier;                 /* 0x1f; same initializer copies to battle +0x164 */
    u8 unit_id;                              /* 0x20 */
    u8 ai_target_xy[2];   /* 0x21; AI "stay near" X, Y (copied to battle_stats_t ai_target_x onward) */
    u8 ai_flags;          /* 0x23 */
    u8 ai_target_unit_id; /* 0x24 */
    u8 _pad25[3];
} entd_unit_t;
typedef char entd_unit_size_must_be_0x28[(sizeof(entd_unit_t) == 0x28) ? 1 : -1];

typedef struct entd_encounter {
    entd_unit_t units[ENTD_UNITS_PER_ENCOUNTER];
} entd_encounter_t;
typedef char entd_encounter_size_must_be_0x280[(sizeof(entd_encounter_t) == 0x280) ? 1 : -1];

typedef struct entd_file {
    entd_encounter_t encounters[ENTD_ENCOUNTERS_PER_FILE];
} entd_file_t;
typedef char entd_file_size_must_be_0x14000[(sizeof(entd_file_t) == 0x14000) ? 1 : -1];

/* The high byte of party_data_t.name_id selects the text bank used for the
 * four persistent-unit name classes. */
typedef enum party_name_class {
    PARTY_NAME_CLASS_SPECIAL = 0x0000,
    PARTY_NAME_CLASS_GENERIC_MALE = 0x0100,
    PARTY_NAME_CLASS_GENERIC_FEMALE = 0x0200,
    PARTY_NAME_CLASS_GENERIC_MONSTER = 0x0300
} party_name_class_e;

enum {
    PARTY_NAME_CLASS_SHIFT = 8,
    PARTY_NAME_CLASS_MASK = 0x0300,
    PARTY_BIRTHDAY_DAY_MASK = 0x01ff,
    PARTY_BIRTHDAY_DAY_HIGH_BIT_MASK = 0x01,
    PARTY_ZODIAC_SHIFT = 4,
    PARTY_ZODIAC_MASK = 0xf0,
};

/*
 * Persistent roster record. Its 0x100-byte stride is established by the
 * indexed SCUS lookup, while its named prefix is established by the target
 * party_data_t-to-battle_stats_t initializer. It is distinct from entd_unit_t.
 */
typedef struct party_data {
    u8 sprite_set;         /* 0x00 */
    u8 party_id;           /* 0x01 */
    u8 job_id;             /* 0x02 */
    u8 palette;            /* 0x03 */
    u8 gender_flags;       /* 0x04 */
    u8 birthday_day;       /* 0x05 */
    u8 zodiac;             /* 0x06 */
    u8 secondary_skillset; /* 0x07 */
    /* Little-endian ability ids; every reader and writer accesses the two bytes separately. */
    u8 reaction_ability[2];   /* 0x08 */
    u8 support_ability[2];    /* 0x0a */
    u8 movement_ability[2];   /* 0x0c */
    u8 equipment[7];          /* 0x0e; unit_equipment_slot_e */
    u8 experience;            /* 0x15 */
    u8 level;                 /* 0x16 */
    u8 bravery;               /* 0x17 */
    u8 faith;                 /* 0x18 */
    u8 raw_stats[15];         /* 0x19; unit_raw_stat_index_e, 3 bytes each */
    u8 unlocked_jobs[3];      /* 0x28 */
    u8 learned_abilities[57]; /* 0x2b */
    u8 job_levels[10];        /* 0x64 */
    u8 job_points[40];        /* 0x6e; per-job Base Job JP */
    u8 total_job_points[40];  /* 0x96; per-job Total Base Job JP */
    u8 name[16];              /* 0xbe */
    u8 name_id[2];            /* 0xce; little-endian, accessed bytewise like the ability ids */
    u8 proposition_status;    /* 0xd0; nonzero while unavailable on a proposition */
    u8 _pad_d1;
    u8 egg_color; /* 0xd2; create/save/generate unit stores */
    u8 _pad_d3[0x2d];
} party_data_t;
typedef char party_data_size_must_be_0x100[(sizeof(party_data_t) == 0x100) ? 1 : -1];

/* Shared by ENTD definitions, persistent party records, and runtime battle
 * units. The byte combines persistence controls with unit-kind flags. */
typedef enum unit_flags {
    UNIT_FLAG_SAVE_FORMATION = 0x01,
    UNIT_FLAG_EGG = 0x04,
    UNIT_FLAG_LOAD_FORMATION = 0x08,
    UNIT_FLAG_JOIN_AFTER_EVENT = 0x10,
    UNIT_FLAG_MONSTER = 0x20,
    UNIT_FLAG_FEMALE = 0x40,
    UNIT_FLAG_MALE = 0x80,
} unit_flags_e;

/* Provisional signed view of battle_ai_command_action_t (Misc +0x15c) as
 * returned by battle_unit_get_target_id_ptr_by_battle_id; 0x800ea068 reads the
 * ability id with a signed halfword load. */
typedef struct world_unit_command_action {
    u8 unit_id;     /* 0x00 */
    u8 skillset;    /* 0x01 */
    s16 ability_id; /* 0x02 */
} world_unit_command_action_t;

/* Per-unit facing/animation state written by world_script_rotate_unit_animation and
 * advanced every frame by world_unit_update_facing_and_pending_animations.
 * Seven bytes per unit, 21 units. */
typedef struct world_unit_animation_state {
    u8 target_facing;
    u8 rotate_mode;     /* 0 = shortest way, 1 = increment, 2 = decrement */
    u8 frames_per_step; /* facing steps once step_counter reaches this */
    u8 step_counter;
    u8 rotating;       /* nonzero while stepping toward target_facing */
    u8 initial_facing; /* facing captured by world_unit_reset_animation_states (0xff when
                          absent); RotateUnit direction 0x14 rotates back to it */
    u8 delay;          /* frames to wait before the first step */
} world_unit_animation_state_t;
typedef char world_unit_animation_state_size_must_be_7[sizeof(world_unit_animation_state_t) == 7 ? 1 : -1];

/* Provisional: three-halfword map tile coordinate staged for battle_unit_place_in_new_location by
 * the WarpUnit event instructions. */
typedef struct world_unit_coordinates {
    s16 x;         /* 0x00 */
    s16 elevation; /* 0x02 */
    s16 y;         /* 0x04 */
} world_unit_coordinates_t;

/* other */
/* Battle Stats 0x05 and Miscellaneous Unit Data 0x13d share this byte. */
typedef enum battle_team_flags {
    BATTLE_TEAM_MASK = 0x30,
    BATTLE_TEAM_OR_PLAYER_CONTROL_MASK = 0x38,
    BATTLE_TEAM_BLUE = 0x00,
    BATTLE_TEAM_RED = 0x10,
    BATTLE_TEAM_GREEN = 0x20,
    BATTLE_TEAM_LIGHT_BLUE = 0x30,
    BATTLE_TEAM_FLAG_IMMORTAL = 0x04,
    BATTLE_TEAM_FLAG_PLAYER_CONTROLLED = 0x08,
} battle_unit_team_flags_e;

typedef enum battle_formation_index {
    BATTLE_FORMATION_INDEX_GENERATED_UNIT = 0xfe,
    BATTLE_FORMATION_INDEX_NONE = 0xff,
} battle_formation_index_e;

enum {
    ITEM_ID_MASK = 0xff,
    ITEM_ID_COUNT = 0x100,
    ENTD_EQUIPMENT_RANDOM = 0xfe,
    ITEM_TYPE_FILTER_ANY = 0xff,
};

#endif
