#ifndef UNIT_H
#define UNIT_H

/* Unit records shared across modules: party, ENTD and battle stats, status and identity. */

#include "fft/data.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* names */
/* A packed unit-name ID and its index and text-page class. */
typedef union unit_name_id {
    u16 value;
    struct {
        u8 index;
        u8 name_class;
    } fields;
} unit_name_id_t;
typedef char unit_name_id_size_must_be_2[(sizeof(unit_name_id_t) == 2) ? 1 : -1];

/* ability flags */
/* Active reaction abilities occupy four MSB-first bytes in unit_stats_t. */
typedef enum battle_unit_reaction_set_1 {
    BATTLE_REACTION_SET_1_BRAVE_UP = 0x01,
    BATTLE_REACTION_SET_1_REGENERATOR = 0x02,
    BATTLE_REACTION_SET_1_DRAGON_SPIRIT = 0x04,
    BATTLE_REACTION_SET_1_CAUTION = 0x08,
    BATTLE_REACTION_SET_1_SUNKEN_STATE = 0x10,
    BATTLE_REACTION_SET_1_SPEED_SAVE = 0x20,
    BATTLE_REACTION_SET_1_MA_SAVE = 0x40,
    BATTLE_REACTION_SET_1_PA_SAVE = 0x80,
} battle_unit_reaction_set_1_e;

typedef enum battle_unit_reaction_set_2 {
    BATTLE_REACTION_SET_2_COUNTER_FLOOD = 0x01,
    BATTLE_REACTION_SET_2_COUNTER_TACKLE = 0x02,
    BATTLE_REACTION_SET_2_COUNTER_MAGIC = 0x04,
    BATTLE_REACTION_SET_2_MEATBONE_SLASH = 0x08,
    BATTLE_REACTION_SET_2_CRITICAL_QUICK = 0x10,
    BATTLE_REACTION_SET_2_MP_RESTORE = 0x20,
    BATTLE_REACTION_SET_2_HP_RESTORE = 0x40,
    BATTLE_REACTION_SET_2_FAITH_UP = 0x80,
} battle_unit_reaction_set_2_e;

typedef enum battle_unit_reaction_set_3 {
    BATTLE_REACTION_SET_3_MP_SWITCH = 0x01,
    BATTLE_REACTION_SET_3_DISTRIBUTE = 0x02,
    /* 0x04 is present in the bitset but remains unidentified. */
    BATTLE_REACTION_SET_3_COUNTER = 0x08,
    BATTLE_REACTION_SET_3_AUTO_POTION = 0x10,
    BATTLE_REACTION_SET_3_REFLECT = 0x20,
    BATTLE_REACTION_SET_3_GILGAME_HEART = 0x40,
    BATTLE_REACTION_SET_3_ABSORB_USED_MP = 0x80,
} battle_unit_reaction_set_3_e;

typedef enum battle_unit_reaction_set_4 {
    BATTLE_REACTION_SET_4_HAMEDO = 0x01,
    BATTLE_REACTION_SET_4_ARROW_GUARD = 0x02,
    BATTLE_REACTION_SET_4_BLADE_GRASP = 0x04,
    BATTLE_REACTION_SET_4_CATCH = 0x08,
    BATTLE_REACTION_SET_4_ABANDON = 0x10,
    BATTLE_REACTION_SET_4_FINGER_GUARD = 0x20,
    BATTLE_REACTION_SET_4_WEAPON_GUARD = 0x40,
    BATTLE_REACTION_SET_4_DAMAGE_SPLIT = 0x80,
} battle_unit_reaction_set_4_e;

/* Active support abilities occupy four MSB-first bitsets in unit_stats_t
 * at +0x08f..+0x092. WORLD copies them to world_unit_t at +0x124;
 * these masks name the shared byte domain, not ability IDs. */
typedef enum battle_unit_support_set_1 {
    BATTLE_SUPPORT_SET_1_EQUIP_GUN = 0x01,
    BATTLE_SUPPORT_SET_1_EQUIP_AXE = 0x02,
    BATTLE_SUPPORT_SET_1_EQUIP_SPEAR = 0x04,
    BATTLE_SUPPORT_SET_1_EQUIP_CROSSBOW = 0x08,
    BATTLE_SUPPORT_SET_1_EQUIP_KATANA = 0x10,
    BATTLE_SUPPORT_SET_1_EQUIP_SWORD = 0x20,
    BATTLE_SUPPORT_SET_1_EQUIP_SHIELD = 0x40,
    BATTLE_SUPPORT_SET_1_EQUIP_ARMOR = 0x80,
} battle_unit_support_set_1_e;

typedef enum battle_unit_support_set_2 {
    BATTLE_SUPPORT_SET_2_CONCENTRATE = 0x01,
    BATTLE_SUPPORT_SET_2_MAGIC_DEFENSE_UP = 0x02,
    BATTLE_SUPPORT_SET_2_MAGIC_ATTACK_UP = 0x04,
    BATTLE_SUPPORT_SET_2_DEFENSE_UP = 0x08,
    BATTLE_SUPPORT_SET_2_ATTACK_UP = 0x10,
    BATTLE_SUPPORT_SET_2_GAINED_EXP_UP = 0x20,
    BATTLE_SUPPORT_SET_2_GAINED_JP_UP = 0x40,
    BATTLE_SUPPORT_SET_2_HALF_MP = 0x80,
} battle_unit_support_set_2_e;

typedef enum battle_unit_support_set_3 {
    BATTLE_SUPPORT_SET_3_TWO_SWORDS = 0x01,
    BATTLE_SUPPORT_SET_3_TWO_HANDS = 0x02,
    BATTLE_SUPPORT_SET_3_MAINTENANCE = 0x04,
    BATTLE_SUPPORT_SET_3_THROW_ITEM = 0x08,
    BATTLE_SUPPORT_SET_3_MONSTER_TALK = 0x10,
    BATTLE_SUPPORT_SET_3_MARTIAL_ARTS = 0x20,
    BATTLE_SUPPORT_SET_3_SECRET_HUNT = 0x40,
    BATTLE_SUPPORT_SET_3_TRAIN = 0x80,
} battle_unit_support_set_3_e;

typedef enum battle_unit_support_set_4 {
    BATTLE_SUPPORT_SET_4_NON_CHARGE = 0x04,
    BATTLE_SUPPORT_SET_4_SHORT_CHARGE = 0x08,
    BATTLE_SUPPORT_SET_4_EQUIP_CHANGE = 0x20,
    BATTLE_SUPPORT_SET_4_DEFEND = 0x40,
    BATTLE_SUPPORT_SET_4_MONSTER_SKILL = 0x80,
} battle_unit_support_set_4_e;

/* Active movement abilities occupy three MSB-first bytes in unit_stats_t.
 * MAIN reads these when calculating move, jump and Float behavior. */
typedef enum battle_unit_movement_set_1 {
    BATTLE_MOVEMENT_SET_1_MOVE_HP_UP = 0x01,
    BATTLE_MOVEMENT_SET_1_IGNORE_HEIGHT = 0x02,
    BATTLE_MOVEMENT_SET_1_JUMP_PLUS_3 = 0x04,
    BATTLE_MOVEMENT_SET_1_JUMP_PLUS_2 = 0x08,
    BATTLE_MOVEMENT_SET_1_JUMP_PLUS_1 = 0x10,
    BATTLE_MOVEMENT_SET_1_MOVE_PLUS_3 = 0x20,
    BATTLE_MOVEMENT_SET_1_MOVE_PLUS_2 = 0x40,
    BATTLE_MOVEMENT_SET_1_MOVE_PLUS_1 = 0x80,
} battle_unit_movement_set_1_e;

typedef enum battle_unit_movement_set_2 {
    BATTLE_MOVEMENT_SET_2_ANY_GROUND = 0x01,
    BATTLE_MOVEMENT_SET_2_ANY_WEATHER = 0x02,
    BATTLE_MOVEMENT_SET_2_TELEPORT_2 = 0x04,
    BATTLE_MOVEMENT_SET_2_TELEPORT = 0x08,
    BATTLE_MOVEMENT_SET_2_CANNOT_ENTER_WATER = 0x10,
    BATTLE_MOVEMENT_SET_2_MOVE_GET_JP = 0x20,
    BATTLE_MOVEMENT_SET_2_MOVE_GET_EXP = 0x40,
    BATTLE_MOVEMENT_SET_2_MOVE_MP_UP = 0x80,
} battle_unit_movement_set_2_e;

typedef enum battle_unit_movement_set_3 {
    BATTLE_MOVEMENT_SET_3_MOVE_FIND_ITEM = 0x01,
    BATTLE_MOVEMENT_SET_3_SILENT_WALK = 0x02,
    BATTLE_MOVEMENT_SET_3_FLY = 0x04,
    BATTLE_MOVEMENT_SET_3_FLOAT = 0x08,
    BATTLE_MOVEMENT_SET_3_MOVE_UNDERWATER = 0x10,
    BATTLE_MOVEMENT_SET_3_MOVE_ON_LAVA = 0x20,
    BATTLE_MOVEMENT_SET_3_WALK_ON_WATER = 0x40, /* ability 0x1f7; wades one level deep */
    BATTLE_MOVEMENT_SET_3_MOVE_IN_WATER = 0x80, /* ability 0x1f6; stands on the water surface */
} battle_unit_movement_set_3_e;

/* action */
/* The 20-byte action command shared by unit stats, AI candidates and menus.
 * Targeting reads coordinates as signed or unsigned 16-bit values. Item selection
 * reads and updates only the low byte of the 16-bit item/equipment field. */
typedef struct action_command {
    u8 actor_id;               /* 0x00 */
    u8 skillset_id;            /* 0x01 */
    s16 ability_id;            /* 0x02 */
    s16 calculator_type;       /* 0x04 */
    s16 calculator_multiplier; /* 0x06 */
    u16 item_or_equipment;     /* 0x08 */
    u8 targeting_type;         /* 0x0a; battle_targeting_type_e */
    u8 target_id;              /* 0x0b */
    SVECTOR target_position;   /* 0x0c; tile x, elevation, y; padding is unread */
} action_command_t;
typedef char action_command_size_must_be_20[(sizeof(action_command_t) == 20) ? 1 : -1];

/* action_data_t.ct_change values that are not an amount. Any other value is a gain when
 * bit 7 is set and a loss otherwise. */
enum {
    ACTION_CT_CHANGE_ZERO = 0x7f,  /* CT set to 0 */
    ACTION_CT_CHANGE_QUICK = 0xff, /* CT filled */
};

/* The pending infliction/removal masks are copied and cleared together. */
typedef struct unit_action_status_changes {
    u8 infliction[STATUS_BYTE_COUNT];
    u8 removal[STATUS_BYTE_COUNT];
} unit_action_status_changes_t;
typedef char unit_action_status_changes_size_must_be_10[(sizeof(unit_action_status_changes_t) == 10) ? 1 : -1];

/*
 * Temporary action results, 0x2c bytes at unit_stats_t+0x18c (0x18c..0x1b7).
 * The action target/attacker pointers address this block directly.
 * status_changes.infliction/removal mirror the five status_sets.current bytes.
 */
typedef struct action_data {
    u8 hit;          /* 0x00; 1 hit, 0 miss */
    u8 critical;     /* 0x01 */
    u8 miss_type;    /* 0x02; action_miss_type_e */
    u8 item_lost;    /* 0x03; potion / break / steal / draw out */
    s16 hp_damage;   /* 0x04 */
    s16 hp_healing;  /* 0x06 */
    s16 mp_damage;   /* 0x08 */
    s16 mp_healing;  /* 0x0a */
    s16 gil_change;  /* 0x0c; stolen / lost */
    s16 reaction_id; /* 0x0e */
    /* Target readers and writers consistently access 0x10-0x11 as one 16-bit value. */
    u16 special_effect;                          /* 0x10; action_special_effect_e */
    u8 sp_change;                                /* 0x12 */
    u8 ct_change;                                /* 0x13; ACTION_CT_CHANGE_* or an amount */
    u8 pa_change;                                /* 0x14 */
    u8 ma_change;                                /* 0x15 */
    u8 brave_change;                             /* 0x16 */
    u8 faith_change;                             /* 0x17 */
    u8 miss_display_flags;                       /* 0x18 */
    u8 equipment_destroyed;                      /* 0x19; action_equipment_slot_mask_e */
    u8 stolen_item_id;                           /* 0x1a */
    unit_action_status_changes_t status_changes; /* 0x1b; removal at 0x20 */
    u8 attack_type;                              /* 0x25; action_type_flags_e */
    u16 last_received_attack;                    /* 0x26; reaction data, varies by ability */
    u8 exp_change;                               /* 0x28; Steal EXP */
    u8 jp_change;                                /* 0x29; Move-JP Up / JP stolen */
    /* Written as a 16-bit value everywhere (formula_apply_status lhu/sh,
     * arrow_guard_usability, formula_apply_catch, force_attack_miss). */
    s16 attack_accuracy; /* 0x2a */
} action_data_t;
typedef char action_data_size_must_be_44[(sizeof(action_data_t) == 44) ? 1 : -1];

/* action_data_t.miss_display_flags, loaded from the ability's AI flags. */
typedef enum action_miss_display_flags {
    ACTION_MISS_DISPLAY_TEAM_MASK = 0x03,       /* the AI target-allies / target-enemies pair */
    ACTION_MISS_DISPLAY_DODGE_ANIMATION = 0x80, /* the ability's evade-with-motion bit */
} action_miss_display_flags_e;

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
    CHARACTER_IDENTITY_ENTD_NONE = 0,
    CHARACTER_IDENTITY_RAMZA_CHAPTER_1 = 1,
    CHARACTER_IDENTITY_RAMZA_CHAPTER_2_3 = 2,
    CHARACTER_IDENTITY_RAMZA_CHAPTER_4 = 3,
    CHARACTER_IDENTITY_DELITA_CHAPTER_1 = 4,
    CHARACTER_IDENTITY_DELITA_CHAPTER_2_3 = 5,
    CHARACTER_IDENTITY_DELITA_CHAPTER_4 = 6,
    CHARACTER_IDENTITY_ALGUS = 7,
    CHARACTER_IDENTITY_ZALBAG = 8,
    CHARACTER_IDENTITY_DYCEDARG = 9,
    CHARACTER_IDENTITY_LARG = 10,
    CHARACTER_IDENTITY_GOLTANA = 11,
    CHARACTER_IDENTITY_OVELIA = 12,
    CHARACTER_IDENTITY_ORLANDU = 13,
    CHARACTER_IDENTITY_FUNERAL = 14,
    CHARACTER_IDENTITY_REIS_HUMAN = 15,
    CHARACTER_IDENTITY_ZALMO = 16,
    CHARACTER_IDENTITY_GAFGARION_ENEMY = 17,
    CHARACTER_IDENTITY_MALAK_12 = 18,
    CHARACTER_IDENTITY_SIMON = 19,
    CHARACTER_IDENTITY_ALMA_BATTLE = 20,
    CHARACTER_IDENTITY_OLAN = 21,
    CHARACTER_IDENTITY_MUSTADIO_JOIN = 22,
    CHARACTER_IDENTITY_GAFGARION_GUEST = 23,
    CHARACTER_IDENTITY_DRACLAU = 24,
    CHARACTER_IDENTITY_RAFA_GUEST = 25,
    CHARACTER_IDENTITY_MALAK = 26,
    CHARACTER_IDENTITY_ELMDOR = 27,
    CHARACTER_IDENTITY_TETA = 28,
    CHARACTER_IDENTITY_BARINTEN = 29,
    CHARACTER_IDENTITY_AGRIAS_JOIN = 30,
    CHARACTER_IDENTITY_BEOWULF = 31,
    CHARACTER_IDENTITY_WIEGRAF_CHAPTER_1 = 32,
    CHARACTER_IDENTITY_BALMAFULA = 33,
    CHARACTER_IDENTITY_MUSTADIO_GUEST = 34,
    CHARACTER_IDENTITY_RUDVICH = 35,
    CHARACTER_IDENTITY_VORMAV = 36,
    CHARACTER_IDENTITY_ROFEL = 37,
    CHARACTER_IDENTITY_IZLUDE = 38,
    CHARACTER_IDENTITY_KLETIAN = 39,
    CHARACTER_IDENTITY_WIEGRAF_CHAPTER_2_3 = 40,
    CHARACTER_IDENTITY_RAFA_JOIN = 41,
    CHARACTER_IDENTITY_MELIADOUL_JOIN = 42,
    CHARACTER_IDENTITY_BALK = 43,
    CHARACTER_IDENTITY_ALMA_2C = 44,
    CHARACTER_IDENTITY_CELIA = 45,
    CHARACTER_IDENTITY_LEDE = 46,
    CHARACTER_IDENTITY_MELIADOUL_ENEMY = 47,
    CHARACTER_IDENTITY_ALMA_EVENTS = 48,
    CHARACTER_IDENTITY_AJORA = 49,
    CHARACTER_IDENTITY_CLOUD = 50,
    CHARACTER_IDENTITY_ZALBAG_ZOMBIE = 51,
    CHARACTER_IDENTITY_AGRIAS_GUEST = 52,
    CHARACTER_IDENTITY_FIXED_FEMALE_CHEMIST_35 = 53,
    CHARACTER_IDENTITY_FIXED_FEMALE_PRIEST_36 = 54,
    CHARACTER_IDENTITY_FIXED_MALE_WIZARD_37 = 55,
    CHARACTER_IDENTITY_FIXED_MALE_ORACLE_38 = 56,
    CHARACTER_IDENTITY_FIXED_MALE_SQUIRE_39 = 57,
    CHARACTER_IDENTITY_CELIA_3A = 58,
    CHARACTER_IDENTITY_LEDE_3B = 59,
    CHARACTER_IDENTITY_VELIUS = 60,
    CHARACTER_IDENTITY_FIXED_MALE_KNIGHT_3D = 61,
    CHARACTER_IDENTITY_ZALERA = 62,
    CHARACTER_IDENTITY_FIXED_MALE_ARCHER_3F = 63,
    CHARACTER_IDENTITY_HASHMALUM = 64,
    CHARACTER_IDENTITY_ALTIMA_FIRST_FORM = 65,
    CHARACTER_IDENTITY_FIXED_MALE_WIZARD_42 = 66,
    CHARACTER_IDENTITY_QUEKLAIN = 67,
    CHARACTER_IDENTITY_FIXED_FEMALE_TIME_MAGE_44 = 68,
    CHARACTER_IDENTITY_ADRAMELK = 69,
    CHARACTER_IDENTITY_FIXED_MALE_ORACLE_46 = 70,
    CHARACTER_IDENTITY_FIXED_FEMALE_SUMMONER_47 = 71,
    CHARACTER_IDENTITY_REIS_DRAGON = 72,
    CHARACTER_IDENTITY_ALTIMA_SECOND_FORM = 73,
    CHARACTER_IDENTITY_GENERIC_FIRST = 74,
    CHARACTER_IDENTITY_BOY = CHARACTER_IDENTITY_GENERIC_FIRST,
    CHARACTER_IDENTITY_GIRL = 75,
    CHARACTER_IDENTITY_YOUNG_MAN = 76,
    CHARACTER_IDENTITY_YOUNG_WOMAN = 77,
    CHARACTER_IDENTITY_MIDDLE_AGED_MAN = 78,
    CHARACTER_IDENTITY_MIDDLE_AGED_WOMAN = 79,
    CHARACTER_IDENTITY_OLD_MAN = 80,
    CHARACTER_IDENTITY_OLD_WOMAN = 81,
    CHARACTER_IDENTITY_OLD_FUNERAL_MAN = 82,
    CHARACTER_IDENTITY_OLD_FUNERAL_WOMAN = 83,
    CHARACTER_IDENTITY_FUNERAL_MAN = 84,
    CHARACTER_IDENTITY_FUNERAL_WOMAN = 85,
    CHARACTER_IDENTITY_FUNERAL_PRIEST = 86,
    CHARACTER_IDENTITY_FIXED_MALE_SQUIRE_57 = 87,
    CHARACTER_IDENTITY_FIXED_MALE_SQUIRE_58 = 88,
    CHARACTER_IDENTITY_FIXED_MALE_SQUIRE_59 = 89,
    CHARACTER_IDENTITY_FIXED_MALE_SQUIRE_5A = 90,
    CHARACTER_IDENTITY_FIXED_MALE_SQUIRE_5B = 91,
    CHARACTER_IDENTITY_FIXED_MALE_SQUIRE_5C = 92,

    /* Fixed graphics, independent of the unit's current job. */
    CHARACTER_IDENTITY_FIXED_MALE_SQUIRE = 96,
    CHARACTER_IDENTITY_FIXED_FEMALE_SQUIRE = 97,
    CHARACTER_IDENTITY_FIXED_MALE_CHEMIST = 98,
    CHARACTER_IDENTITY_FIXED_FEMALE_CHEMIST = 99,
    CHARACTER_IDENTITY_FIXED_MALE_KNIGHT = 100,
    CHARACTER_IDENTITY_FIXED_FEMALE_KNIGHT = 101,
    CHARACTER_IDENTITY_FIXED_MALE_ARCHER = 102,
    CHARACTER_IDENTITY_FIXED_FEMALE_ARCHER = 103,
    CHARACTER_IDENTITY_FIXED_MALE_MONK = 104,
    CHARACTER_IDENTITY_FIXED_FEMALE_MONK = 105,
    CHARACTER_IDENTITY_FIXED_MALE_PRIEST = 106,
    CHARACTER_IDENTITY_FIXED_FEMALE_PRIEST = 107,
    CHARACTER_IDENTITY_FIXED_MALE_WIZARD = 108,
    CHARACTER_IDENTITY_FIXED_FEMALE_WIZARD = 109,
    CHARACTER_IDENTITY_FIXED_MALE_TIME_MAGE = 110,
    CHARACTER_IDENTITY_FIXED_FEMALE_TIME_MAGE = 111,
    CHARACTER_IDENTITY_FIXED_MALE_SUMMONER = 112,
    CHARACTER_IDENTITY_FIXED_FEMALE_SUMMONER = 113,
    CHARACTER_IDENTITY_FIXED_MALE_THIEF = 114,
    CHARACTER_IDENTITY_FIXED_FEMALE_THIEF = 115,
    CHARACTER_IDENTITY_FIXED_MALE_MEDIATOR = 116,
    CHARACTER_IDENTITY_FIXED_FEMALE_MEDIATOR = 117,
    CHARACTER_IDENTITY_FIXED_MALE_ORACLE = 118,
    CHARACTER_IDENTITY_FIXED_FEMALE_ORACLE = 119,
    CHARACTER_IDENTITY_FIXED_MALE_GEOMANCER = 120,
    CHARACTER_IDENTITY_FIXED_FEMALE_GEOMANCER = 121,
    CHARACTER_IDENTITY_FIXED_MALE_LANCER = 122,
    CHARACTER_IDENTITY_FIXED_FEMALE_LANCER = 123,
    CHARACTER_IDENTITY_FIXED_MALE_SAMURAI = 124,
    CHARACTER_IDENTITY_FIXED_FEMALE_SAMURAI = 125,
    CHARACTER_IDENTITY_FIXED_MALE_NINJA = 126,
    CHARACTER_IDENTITY_FIXED_FEMALE_NINJA = 127,

    /* Job-dependent selectors, not fixed graphics. */
    CHARACTER_IDENTITY_GENERIC_MALE = 128,
    CHARACTER_IDENTITY_GENERIC_FEMALE = 129,
    CHARACTER_IDENTITY_MONSTER = 130,
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
    BATTLE_EQUIPMENT_SET_1_AXE = 0x02,
    BATTLE_EQUIPMENT_SET_1_KATANA = 0x04,
    BATTLE_EQUIPMENT_SET_1_SWORD = 0x10,
} battle_unit_equipment_categories_1_e;

typedef enum battle_equipment_categories_2 {
    BATTLE_EQUIPMENT_SET_2_SPEAR = 0x01,
    BATTLE_EQUIPMENT_SET_2_CROSSBOW = 0x10,
    BATTLE_EQUIPMENT_SET_2_GUN = 0x20,
} battle_unit_equipment_categories_2_e;

typedef enum battle_equipment_categories_3 {
    BATTLE_EQUIPMENT_SET_3_ARMOR = 0x01,
    BATTLE_EQUIPMENT_SET_3_RIBBON = 0x02,
    BATTLE_EQUIPMENT_SET_3_HELM = 0x08,
    BATTLE_EQUIPMENT_SET_3_SHIELD = 0x10,
    BATTLE_EQUIPMENT_SET_3_BAG = 0x40,
} battle_unit_equipment_categories_3_e;

typedef enum battle_equipment_categories_4 {
    BATTLE_EQUIPMENT_SET_4_PERFUME = 0x01,
} battle_unit_equipment_categories_4_e;

/*
 * Seven-slot equipment order serialized by party_unit_t and unit_stats_t.
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

s32 unit_is_two_handing_weapon(const s16* slots, s32 two_hands_support);

/* job */
/* Career slots follow the generic jobs' order. Slot zero holds the unit's base
 * job, including a special character's job; generic Squire has job ID 74. */
typedef enum unit_career_job_index {
    UNIT_CAREER_JOB_INDEX_BASE = 0,
    UNIT_CAREER_JOB_INDEX_CHEMIST = 1,
    UNIT_CAREER_JOB_INDEX_KNIGHT = 2,
    UNIT_CAREER_JOB_INDEX_ARCHER = 3,
    UNIT_CAREER_JOB_INDEX_MONK = 4,
    UNIT_CAREER_JOB_INDEX_PRIEST = 5,
    UNIT_CAREER_JOB_INDEX_WIZARD = 6,
    UNIT_CAREER_JOB_INDEX_TIME_MAGE = 7,
    UNIT_CAREER_JOB_INDEX_SUMMONER = 8,
    UNIT_CAREER_JOB_INDEX_THIEF = 9,
    UNIT_CAREER_JOB_INDEX_MEDIATOR = 10,
    UNIT_CAREER_JOB_INDEX_ORACLE = 11,
    UNIT_CAREER_JOB_INDEX_GEOMANCER = 12,
    UNIT_CAREER_JOB_INDEX_LANCER = 13,
    UNIT_CAREER_JOB_INDEX_SAMURAI = 14,
    UNIT_CAREER_JOB_INDEX_NINJA = 15,
    UNIT_CAREER_JOB_INDEX_CALCULATOR = 16,
    UNIT_CAREER_JOB_INDEX_BARD = 17,
    UNIT_CAREER_JOB_INDEX_DANCER = 18,
    UNIT_CAREER_JOB_INDEX_MIME = 19,
} unit_career_job_index_e;

/* Career limits: JP-derived levels stop at 8; ENTD nibbles can encode higher levels. */
enum {
    UNIT_JOB_LEVEL_MAX = 8,
    UNIT_JOB_POINT_MAX = 9999,
};

/* Serialized unit-career layout shared by party records and battle stats.
 * main_unit_generate_out_of_battle (0x80059ffc) clears the 0x96-byte span,
 * while main_job_add_proposition_jp and main_unit_init_battle_data establish
 * the little-endian byte order of each serialized JP value. Each packed level
 * byte stores the even career index in its high nibble and the following odd
 * index in its low nibble. */
enum {
    UNIT_CAREER_JOB_COUNT = JOB_ID_GENERIC_COUNT,
    UNIT_UNLOCKED_JOB_BYTE_COUNT = 3,
    /* Mime has JP and a packed job level, but no learned-ability row. */
    UNIT_LEARNED_ABILITY_JOB_COUNT = UNIT_CAREER_JOB_COUNT - 1,
    /* Each job has 24 learned-ability bits, MSB-first within each byte. */
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

/* Twenty job-unlock bits occupy the high bits of the three-byte career value. */
enum {
    UNIT_UNLOCKED_JOB_DANCER = 0x20,
    UNIT_UNLOCKED_JOB_BARD = 0x40,
    UNIT_UNLOCKED_JOB_SQUIRE = 0x800000,
};
enum {
    UNIT_UNLOCKED_JOB_MASK = 0xfffff0,
};

/* Most JP access is 16-bit; party generation and proposition awards use bytes. */
typedef union unit_job_point_table {
    u16 values[UNIT_CAREER_JOB_COUNT];
    struct {
        u8 low;
        u8 high;
    } entries[UNIT_CAREER_JOB_COUNT];
} unit_job_point_table_t;
typedef char
    unit_job_point_table_size_must_be_40[(sizeof(unit_job_point_table_t) == UNIT_JOB_POINT_BYTE_COUNT) ? 1 : -1];

/* Party-to-battle initialization copies both adjacent tables as one block. */
typedef struct unit_job_points {
    unit_job_point_table_t current;
    unit_job_point_table_t total;
} unit_job_points_t;
typedef char
    unit_job_points_size_must_be_80[(sizeof(unit_job_points_t) == UNIT_JOB_POINT_TABLE_PAIR_BYTE_COUNT) ? 1 : -1];

/* Shared 150-byte progression block cleared when a party unit is generated. */
typedef struct unit_career_data {
    u8 unlocked_jobs[UNIT_UNLOCKED_JOB_BYTE_COUNT];                                           /* 0x00 */
    u8 learned_abilities[UNIT_LEARNED_ABILITY_JOB_COUNT][UNIT_LEARNED_ABILITY_BYTES_PER_JOB]; /* 0x03 */
    u8 job_levels[UNIT_JOB_LEVEL_BYTE_COUNT]; /* 0x3c; two packed nibbles per byte */
    unit_job_points_t job_points;             /* 0x46; current JP followed by total JP */
} unit_career_data_t;
typedef char unit_career_data_size_must_be_150[(sizeof(unit_career_data_t) == UNIT_CAREER_DATA_BYTE_COUNT) ? 1 : -1];

/* zodiac */
/* Unshifted zodiac IDs. Party and battle records store these in the high
 * nibble; ordinary birthdays never produce Serpentarius. */
typedef enum zodiac_sign {
    ZODIAC_SIGN_ARIES = 0,
    ZODIAC_SIGN_TAURUS = 1,
    ZODIAC_SIGN_GEMINI = 2,
    ZODIAC_SIGN_CANCER = 3,
    ZODIAC_SIGN_LEO = 4,
    ZODIAC_SIGN_VIRGO = 5,
    ZODIAC_SIGN_LIBRA = 6,
    ZODIAC_SIGN_SCORPIO = 7,
    ZODIAC_SIGN_SAGITTARIUS = 8,
    ZODIAC_SIGN_CAPRICORN = 9,
    ZODIAC_SIGN_AQUARIUS = 10,
    ZODIAC_SIGN_PISCES = 11,
    ZODIAC_SIGN_SERPENTARIUS = 12,
} zodiac_sign_e;

enum {
    ZODIAC_SIGN_ORDINARY_COUNT = 12,
    ZODIAC_SIGN_COUNT = 13,
};

/* status */
/* SCUS 0x8005b5dc copies these three consecutive sets as one 15-byte block.
 * Keep a named aggregate so whole-block byte copies do not walk beyond the
 * first five-byte array. Individual status readers still use named sets. */
typedef struct status_sets {
    u8 innate[5];
    u8 immunity[5];
    u8 current[5];
} status_sets_t;
typedef char status_sets_size_must_be_15[(sizeof(status_sets_t) == 15) ? 1 : -1];

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

/* Rows of g_main_status_check_sets. main_status_init_check_data builds rows 0
 * to 9 from the flag bytes of g_main_status_effect_data and writes row 10
 * itself. Each enumerator's comment lists the statuses its row holds in this
 * release, then what the row is used for. */
typedef enum main_status_check_set {
    /* Crystal, Dead, Petrify, Invite, Blood Suck, Treasure. A unit with none of them still fights for its side. */
    MAIN_STATUS_CHECK_SET_KO = 0,
    /* Crystal, Invite, Treasure. On an importance-3 unit of either team it decides the battle. */
    MAIN_STATUS_CHECK_SET_UNIT_LOST = 1,
    /* Crystal, Treasure. Built; no code reads it. */
    MAIN_STATUS_CHECK_SET_UNREAD_2 = 2,
    /* Crystal, Petrify, Treasure, Stop, Sleep. The unit's CT does not advance. */
    MAIN_STATUS_CHECK_SET_FREEZE_CT = 3,
    /* Crystal, Dead, Jump, Petrify, Confusion, Blood Suck, Treasure, Berserk, Chicken, Frog, Stop, Sleep, Don't Act.
     * No reaction, no learning on hit, no movement-ability benefit. */
    MAIN_STATUS_CHECK_SET_PREVENT_REACTION = 4,
    /* Crystal, Dead, Undead, Petrify, Invite, Confusion, Blood Suck, Treasure, Chicken, Frog, Charm, Sleep,
     * Don't Act, Death Sentence. Added to the immunities of an immortal unit with a save- or load-formation flag. */
    MAIN_STATUS_CHECK_SET_IMMORTAL_IMMUNITY = 5,
    /* Crystal, Dead, Undead, Petrify, Invite, Blood Suck, Treasure, Chicken, Frog, Charm, Death Sentence.
     * Added with set 5, and never inflicted on an immortal unit. */
    MAIN_STATUS_CHECK_SET_FORMATION_IMMUNITY = 6,
    /* Darkness, Confusion, Silence, Transparent, Berserk, Chicken, Frog, Stop, Charm, Sleep, Don't Move, Don't Act.
     * Built; no code reads it. */
    MAIN_STATUS_CHECK_SET_UNREAD_7 = 7,
    /* Crystal, Dead, Undead, Jump, Petrify, Invite, Treasure, Critical. Script unit filters 3 and 5 leave out a
     * unit with one of them (filter 5 lets Undead through). */
    MAIN_STATUS_CHECK_SET_SCRIPT_FILTER_EXCLUDED = 8,
    /* Confusion, Berserk, Poison, Stop, Sleep, Don't Move, Don't Act, Death Sentence. Removed from a mount when its
     * rider ends a move. */
    MAIN_STATUS_CHECK_SET_MOUNT_REMOVAL = 9,
    /* Crystal, Dead, Petrify, Blood Suck, Treasure, Berserk, Chicken, Frog, Charm; set in code, not from the status
     * data. A chocobo with one of them cannot be mounted. */
    MAIN_STATUS_CHECK_SET_UNMOUNTABLE = 10,
    MAIN_STATUS_CHECK_SET_COUNT = 11,
} main_status_check_set_e;

/* The battle simulation owns 21 unit records. Battle-slot search routines use
 * the first value beyond that array as their no-unit sentinel. */
enum {
    BATTLE_UNIT_RUNTIME_SLOT_COUNT = 16, /* simultaneously present units, including event ghosts */
    BATTLE_UNIT_SLOT_COUNT = 21,
    BATTLE_UNIT_ID_NONE = BATTLE_UNIT_SLOT_COUNT,
};

/* Shared event snapshot for all 21 simulation slots. WORLD 0x800fb9ec writes
 * the saved fields; WORLD 0x800fbd14 and BATTLE 0x801488a8 restore them and
 * apply intervening status differences through BATTLE callbacks. */
typedef struct unit_status_staging {
    u8 innate[BATTLE_UNIT_SLOT_COUNT][STATUS_BYTE_COUNT];
    u8 current[BATTLE_UNIT_SLOT_COUNT][STATUS_BYTE_COUNT];           /* 0x069 */
    u8 inflicted[BATTLE_UNIT_SLOT_COUNT][STATUS_BYTE_COUNT];         /* 0x0d2 */
    u8 removed[BATTLE_UNIT_SLOT_COUNT][STATUS_BYTE_COUNT];           /* 0x13b */
    u8 added[BATTLE_UNIT_SLOT_COUNT][STATUS_BYTE_COUNT];             /* 0x1a4 */
    u8 status_ct[BATTLE_UNIT_SLOT_COUNT][BATTLE_TIMED_STATUS_COUNT]; /* 0x20d */
    u8 charged_ability_ct[BATTLE_UNIT_SLOT_COUNT];                   /* 0x35d */
    u8 death_counter[BATTLE_UNIT_SLOT_COUNT];                        /* 0x372 */
    u8 team_flags[BATTLE_UNIT_SLOT_COUNT];                           /* 0x387 */
    u8 state[BATTLE_UNIT_SLOT_COUNT];                                /* 0x39c; 0 not staged, 1 saved, 2 restored */
    u8 inflict_status_mode[BATTLE_UNIT_SLOT_COUNT];                  /* 0x3b1; unit_inflict_status_mode_e */
    u16 hp[BATTLE_UNIT_SLOT_COUNT];                                  /* 0x3c6 */
    u16 jump_float_flags;                                            /* 0x3f0; UNIT_STATUS_STAGING_*_REMOVED */
} unit_status_staging_t;
typedef char unit_status_staging_size_must_be_1010[sizeof(unit_status_staging_t) == 1010 ? 1 : -1];

/* unit_status_staging_t.jump_float_flags: a staged change removes the status from some unit. */
enum {
    UNIT_STATUS_STAGING_JUMP_REMOVED = 0x1,
    UNIT_STATUS_STAGING_FLOAT_REMOVED = 0x2,
};

/* unit_status_staging_t.state of one unit. */
typedef enum unit_status_staging_state {
    UNIT_STATUS_STAGING_NONE = 0,
    UNIT_STATUS_STAGING_SAVED = 1,
    UNIT_STATUS_STAGING_RESTORED = 2, /* blocks a second restore */
} unit_status_staging_state_e;

/* unit_status_staging_t.inflict_status_mode of one unit: the change the
 * InflictStatus event thread stages and *_unit_apply_staged_status_data
 * applies. */
typedef enum unit_inflict_status_mode {
    UNIT_INFLICT_STATUS_MODE_NONE = 0,
    UNIT_INFLICT_STATUS_MODE_REVIVE = 1, /* 1 HP, Critical added, Dead removed */
    UNIT_INFLICT_STATUS_MODE_CRYSTAL = 2,
    UNIT_INFLICT_STATUS_MODE_POISON = 3,
} unit_inflict_status_mode_e;

/* Per-byte status removal masks applied while an event stages a unit: three
 * eight-byte sets, of which the first STATUS_BYTE_COUNT bytes are used
 * (BATTLE 0x80169758, WORLD 0x80156be8). */
typedef enum unit_status_staging_mask {
    UNIT_STATUS_STAGING_MASK_BATTLE_STARTED = 0,
    UNIT_STATUS_STAGING_MASK_PRE_BATTLE = 1,
    /* Monster skillsets, both Altima forms and the six undead jobs. */
    UNIT_STATUS_STAGING_MASK_PRE_BATTLE_MONSTER_OR_UNDEAD = 2,
} unit_status_staging_mask_e;

enum { UNIT_STATUS_STAGING_MASK_COUNT = 3 };

extern u8 g_unit_status_staging_masks[UNIT_STATUS_STAGING_MASK_COUNT][8];
extern unit_status_staging_t* g_unit_status_staging_data;
void unit_clear_status_staging_data(void);
s32 unit_save_staged_status_data(s32 battle_id);
void unit_apply_staged_status_data(s32 battle_id, s32 runtime_id);
void unit_update_staged_status_data(u16 unit_id, u16 restore);
void status_queue_current_status_graphics(s32 battle_id, s32 enabled);

/* unit */
/* Character progression; excess EXP is discarded on a level-up. */
enum {
    UNIT_LEVEL_MAX = 99,
    UNIT_LEVEL_UP_EXPERIENCE = 100,
    UNIT_LEVEL_EXPERIENCE_MAX = UNIT_LEVEL_UP_EXPERIENCE - 1,
};

/* Calculated stat caps; eggs use the full u16 range for HP and MP. */
enum {
    UNIT_HP_MP_MAX = 999,
    UNIT_ATTACK_STAT_MAX = 99,
    UNIT_SPEED_MAX = 50,
    UNIT_BRAVE_FAITH_MAX = 100,
};

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

/* The save format stores each unsigned 24-bit value in three bytes. */
typedef struct unit_raw_stat {
    u8 low;
    u8 middle;
    u8 high;
} unit_raw_stat_t;
typedef char unit_raw_stat_size_must_be_3[(sizeof(unit_raw_stat_t) == UNIT_RAW_STAT_SERIALIZED_BYTES) ? 1 : -1];

/* HP, MP, speed, physical attack and magic attack in UNIT_RAW_STAT_* order.
 * Generation, growth and derived-stat calculation select all five entries by index;
 * saved and battle units copy the complete record. */
typedef unit_raw_stat_t unit_raw_stats_t[UNIT_RAW_STAT_COUNT];
typedef char unit_raw_stats_size_must_be_15[(sizeof(unit_raw_stats_t) == UNIT_RAW_STAT_DATA_BYTE_COUNT) ? 1 : -1];

/* CT points needed for a unit turn; speed supplies points each clocktick. */
enum {
    UNIT_TURN_CT_THRESHOLD = 100,
};

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

/* Packed birthday and zodiac at unit offset 0x008. Field updates preserve
 * the three reserved bits between the day-of-year and zodiac. */
typedef struct battle_unit_birthday {
    u16 birthday : 9;
    u16 _unused_01 : 3; /* reserved bits 9..11 */
    u16 zodiac : 4;
} battle_unit_birthday_t;
typedef char battle_unit_birthday_size_must_be_2[(sizeof(battle_unit_birthday_t) == 2) ? 1 : -1];

/* Suppress random spell quotes for the matching primary or secondary skillset. */
typedef enum unit_spell_quote_flags {
    UNIT_SPELL_QUOTE_SUPPRESS_SECONDARY = 0x01,
    UNIT_SPELL_QUOTE_SUPPRESS_PRIMARY = 0x02,
} unit_spell_quote_flags_e;

/*
 * Runtime Y, facing and tile-layer fields. ENTD uses the narrower facing
 * field in entd_unit_position_t below; initialization copies each group.
 */
typedef struct battle_unit_position {
    u16 y : 8;
    u16 facing : 4;
    u16 spell_quote_skillsets : 2; /* unit_spell_quote_flags_e */
    u16 stepping_stone : 1;
    u16 higher_elevation : 1;
} battle_unit_position_t;
typedef char battle_unit_position_size_must_be_2[(sizeof(battle_unit_position_t) == 2) ? 1 : -1];

/* deployment */
typedef enum battle_formation_index {
    BATTLE_FORMATION_INDEX_GENERATED_UNIT = 254,
    BATTLE_FORMATION_INDEX_NONE = 255,
} battle_formation_index_e;

/* Deployment records share the byte written by ATTACK and read by BATTLE. */
typedef struct unit_deployment_flags {
    u8 facing : 4;
    u8 off_field : 1;
    u8 _unused_00_bit5 : 2;
    u8 upper_level : 1;
} unit_deployment_flags_t;

typedef struct unit_deployment_coordinates {
    u8 party_id;
    u8 x;
    u8 y;
    unit_deployment_flags_t flags;
} unit_deployment_coordinates_t;
typedef char unit_deployment_coordinates_size_must_be_4[(sizeof(unit_deployment_coordinates_t) == 4) ? 1 : -1];

/* Equipped or ENTD-selected reaction/support/movement ability IDs. */
typedef struct unit_abilities {
    u16 reaction;
    u16 support;
    u16 movement;
} unit_abilities_t;
typedef char unit_abilities_size_must_be_6[(sizeof(unit_abilities_t) == 6) ? 1 : -1];

/* teams and control */
/* Team bits select one of four independently tracked battle teams. */
enum {
    BATTLE_TEAM_COUNT = 4,
    BATTLE_TEAM_SHIFT = 4,
    BATTLE_TEAM_INDEX_MASK = 0x03,
};

/* Battle Stats 0x05 and unit runtime record 0x13d share this byte. */
typedef enum battle_team_flags {
    BATTLE_TEAM_MASK = 0x30,
    BATTLE_TEAM_OR_PLAYER_CONTROL_MASK = 0x38,
    BATTLE_TEAM_BLUE = 0x00,
    BATTLE_TEAM_RED = 0x10,
    BATTLE_TEAM_GREEN = 0x20,
    BATTLE_TEAM_LIGHT_BLUE = 0x30,
    BATTLE_TEAM_IMPORTANCE_MASK = 0x03, /* 0..3; 3 decides the battle outcome and cannot be knocked back */
    BATTLE_TEAM_FLAG_IMMORTAL = 0x04,
    BATTLE_TEAM_FLAG_PLAYER_CONTROLLED = 0x08,
    /* ENTD presence, read when the units are loaded: neither bit leaves the
     * unit out, RANDOMLY_PRESENT alone leaves it out half the time, and both
     * load its graphics for a later AddUnit. */
    BATTLE_TEAM_FLAG_RANDOMLY_PRESENT = 0x40,
    BATTLE_TEAM_FLAG_ALWAYS_PRESENT = 0x80,
    BATTLE_TEAM_PRESENCE_MASK = 0xc0,
} battle_unit_team_flags_e;

/* unit_stats_t.auto_battle_setting and the AI copies of it. The BATTLE and
 * WORLD auto-battle menus store it and the AI reads it. */
typedef enum ai_autobattle_mode {
    AI_AUTOBATTLE_MANUAL = 0, /* the player gives the orders */
    AI_AUTOBATTLE_TARGETED = 12,
    AI_AUTOBATTLE_RECOVERY = 14,
    AI_AUTOBATTLE_FADING_LIFE = 16,
    AI_AUTOBATTLE_RETREAT = 17,
} ai_autobattle_mode_e;

/* unit stats */
/* Battle Stats 0x182 links the two simulation records in a mounted pair. */
typedef enum battle_mount_info_flags {
    BATTLE_MOUNT_INFO_PARTNER_ID_MASK = 0x1f,
    BATTLE_MOUNT_INFO_FLAG_MOUNT = 0x40,
    BATTLE_MOUNT_INFO_FLAG_RIDER = 0x80,
    BATTLE_MOUNT_INFO_ROLE_MASK = 0xc0,
} battle_mount_info_flags_e;

/* Lifecycle states stored in Battle Stats 0x183. */
typedef enum battle_unit_existence_state {
    BATTLE_UNIT_EXISTENCE_OFF_FIELD = 0, /* defined but not placed; AddUnit can use the slot */
    BATTLE_UNIT_EXISTENCE_ACTIVE = 1,
    BATTLE_UNIT_EXISTENCE_PENDING_REMOVAL = 2,
    BATTLE_UNIT_EXISTENCE_DISABLED = 128,
    BATTLE_UNIT_EXISTENCE_NONE = 255, /* empty slot */
} battle_unit_existence_state_e;

typedef enum battle_unit_equipped_flags {
    BATTLE_UNIT_EQUIPPED_FLAG_MATERIA_BLADE = 0x04,
    BATTLE_UNIT_EQUIPPED_FLAG_SWORD = 0x08,
} battle_unit_equipped_flags_e;

/* unit_stats_t.entd_slot value for an unused or removed slot. */
enum {
    BATTLE_ENTD_SLOT_NONE = 0xff,
};

/* Current and calculated unit attributes, also built for WORLD menus. BATTLE
 * stores 21 consecutive 448-byte records at 0x801908cc. A live battle_unit_t
 * points to its stats through the member at 0x134. */
typedef struct unit_stats {
    u8 character_identity;                               /* 0x000 */
    u8 entd_slot;                                        /* 0x001; 0xff means absent */
    u8 formation_index;                                  /* 0x002 */
    u8 job_id;                                           /* 0x003 */
    u8 sprite_palette;                                   /* 0x004 */
    u8 team_flags;                                       /* 0x005 */
    u8 unit_flags;                                       /* 0x006; mixed sex, monster, guest, and formation flags */
    u8 death_counter;                                    /* 0x007 */
    battle_unit_birthday_t birthday;                     /* 0x008; zodiac is the high nibble */
    u16 innate_abilities[4];                             /* 0x00a */
    u8 primary_skillset;                                 /* 0x012 */
    u8 secondary_skillset;                               /* 0x013 */
    unit_abilities_t abilities;                          /* 0x014; reaction, support, movement */
    u8 equipment[7];                                     /* 0x01a; unit_equipment_slot_e */
    u8 experience;                                       /* 0x021 */
    u8 level;                                            /* 0x022 */
    u8 original_brave;                                   /* 0x023 */
    u8 brave;                                            /* 0x024 */
    u8 original_faith;                                   /* 0x025 */
    u8 faith;                                            /* 0x026 */
    u8 transparent_removal_flag;                         /* 0x027 */
    u16 hp;                                              /* 0x028 */
    u16 max_hp;                                          /* 0x02a */
    u16 mp;                                              /* 0x02c */
    u16 max_mp;                                          /* 0x02e */
    u8 base_attributes[3];                               /* 0x030; unit_attribute_index_e */
    u8 equipment_attributes[3];                          /* 0x033; unit_attribute_index_e */
    u8 attributes[3];                                    /* 0x036; unit_attribute_index_e */
    u8 ct;                                               /* 0x039 */
    u8 move;                                             /* 0x03a */
    u8 jump;                                             /* 0x03b */
    u8 equipment_stats[11];                              /* 0x03c; battle_unit_equipment_stat_index_e */
    u8 x;                                                /* 0x047 */
    battle_unit_position_t position;                     /* 0x048 */
    u8 equipment_categories[4];                          /* 0x04a */
    status_sets_t status_sets;                           /* 0x04e; innate / immunity / current */
    u8 status_ct[16];                                    /* 0x05d */
    u8 elemental_affinity[5];                            /* 0x06d; elemental_affinity_index_e */
    unit_raw_stats_t raw_stats;                          /* 0x072 */
    job_stat_growth_t stat_growths[UNIT_RAW_STAT_COUNT]; /* 0x081; job_data_t.stat_growths */
    /* Active R/S/M flags: one contiguous 88-bit set indexed by ability ID. */
    struct {
        u8 reaction[4];        /* 0x00; battle_unit_reaction_set_N_e */
        u8 support[4];         /* 0x04; battle_unit_support_set_N_e */
        u8 movement[3];        /* 0x08; battle_unit_movement_set_N_e */
    } ability_flags;           /* 0x08b */
    unit_career_data_t career; /* 0x096; unlocked jobs, learned abilities, levels and JP */
    /* main_unit_store_character_names (0x8005c984) copies 16/16/8/8
     * bytes to these four destinations. All names use the game's encoding. */
    u8 name[16];                     /* 0x12c */
    u8 job_name[16];                 /* 0x13c */
    u8 primary_skillset_name[8];     /* 0x14c */
    u8 secondary_skillset_name[8];   /* 0x154 */
    u8 ko_count;                     /* 0x15c */
    u8 charged_ability_ct;           /* 0x15d */
    u8 monster_graphic;              /* 0x15e; copied from job data and consumed by WORLD formation graphics */
    u8 spritesheet_id;               /* 0x15f */
    u8 job_portrait_palette;         /* 0x160 */
    u8 unit_id;                      /* 0x161 */
    u8 base_job_skillset;            /* 0x162 */
    u8 war_trophy;                   /* 0x163 */
    u8 bonus_money_modifier;         /* 0x164 */
    u8 ai_target_x;                  /* 0x165 */
    u8 ai_target_y;                  /* 0x166 */
    u8 ai_flags;                     /* 0x167; ai_decision_flags_e */
    u8 prioritized_target;           /* 0x168 */
    u8 ai_targeting_flags_1;         /* 0x169; copied to ai_unit_decision_t +0x06 */
    u8 ai_targeting_flags_2;         /* 0x16a; ai_targeting_flags_2_e */
    u8 ai_initial_enemy_flag;        /* 0x16b; ENTD 0x27 */
    unit_name_id_t quote_name_id;    /* 0x16c */
    action_command_t action_command; /* 0x16e..0x181 */
    u8 mount_info;                   /* 0x182 */
    u8 existence;                    /* 0x183; battle_unit_existence_state_e */
    u8 equipped_flags;               /* 0x184; battle_unit_equipped_flags_e */
    u8 _unused_185;
    u8 has_turn;            /* 0x186 */
    u8 movement_taken;      /* 0x187 */
    u8 action_taken;        /* 0x188 */
    u8 ability_outcome;     /* 0x189 */
    u8 battle_id;           /* 0x18a; index in g_battle_unit_stats */
    u8 ability_ct;          /* 0x18b */
    action_data_t action;   /* 0x18c through 0x1b7 */
    u8 auto_battle_setting; /* 0x1b8; ai_autobattle_mode_e */
    u8 auto_battle_target;  /* 0x1b9 */
    u8 initial_team_flags;  /* 0x1ba */
    u8 inflicted_status[5]; /* 0x1bb */
} unit_stats_t;
typedef char unit_stats_size_must_be_448[(sizeof(unit_stats_t) == 448) ? 1 : -1];

/* unit_stats_t.ability_outcome. */
typedef enum battle_unit_ability_outcome {
    BATTLE_ABILITY_OUTCOME_TARGETED = 0x01,      /* target of a primary action */
    BATTLE_ABILITY_OUTCOME_AFFECTED = 0x02,      /* that action changed the unit */
    BATTLE_ABILITY_OUTCOME_LEARN_CHECKED = 0x04, /* the learn-on-hit pass consumed AFFECTED */
} battle_unit_ability_outcome_e;

extern unit_stats_t g_battle_unit_stats[BATTLE_UNIT_SLOT_COUNT];

unit_stats_t* battle_unit_get_existing_pointer(u32 battle_id);
unit_stats_t* battle_unit_get_stats_from_battle_id(u32 battle_id);
s32 unit_has_any_current_status(s32 battle_id);
void unit_increase_blue_team_brave_by_10(void);

/* encounters */
enum {
    ENTD_VALUE_RANDOM = 254, /* in an ENTD equipment, level or secondary skillset byte: pick one at random */
};

typedef struct entd_unit_position {
    u16 y : 8;
    u16 facing : 2;
    u16 _unused_01 : 2;            /* bits 10-11 */
    u16 spell_quote_skillsets : 2; /* unit_spell_quote_flags_e */
    u16 stepping_stone : 1;
    u16 higher_elevation : 1;
} entd_unit_position_t;
typedef char entd_unit_position_size_must_be_2[(sizeof(entd_unit_position_t) == 2) ? 1 : -1];

enum {
    ENTD_UNITS_PER_ENCOUNTER = 16,
    ENTD_ENCOUNTERS_PER_FILE = 128,
    ENTD2_FIRST_EVENT = ENTD_ENCOUNTERS_PER_FILE,
    ENTD3_FIRST_EVENT = 2 * ENTD_ENCOUNTERS_PER_FILE,
    ENTD4_FIRST_EVENT = 3 * ENTD_ENCOUNTERS_PER_FILE,
};

/* The ENTD slots precede the five deployed-party slots in battle storage. */
enum { BATTLE_DEPLOYED_UNIT_SLOT_FIRST = ENTD_UNITS_PER_ENCOUNTER };

/* The persistent roster at 0x80057f74 contains 16 formation slots followed
 * by four guest slots, each with the party_unit_t 0x100-byte stride. */
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

/*
 * An ENTD entry is the static 0x28-byte encounter definition read from an
 * ENTD#.ENT file.  It is input to the SCUS unit initializers; it is neither a
 * persistent party_unit_t record nor a runtime unit_stats_t record.
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
    u8 level;                                /* 0x03; 0 or 0xfe: L - rnd(0..L/8); 100 + n: L + n; L: top party level */
    u8 birth_month;                          /* 0x04; 1..12 for a fixed birthday */
    u8 birth_day;                            /* 0x05; 1..31 for a fixed birthday */
    u8 bravery;                              /* 0x06; above 100: random 45..74 */
    u8 faith;                                /* 0x07; above 100: random 45..74 */
    u8 job_unlock;                           /* 0x08 */
    u8 job_level;                            /* 0x09 */
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
    u8 war_trophy;                           /* 0x1e; 0x8005e478 copies to battle +0x163 */
    u8 bonus_money_modifier;                 /* 0x1f; same initializer copies to battle +0x164 */
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

/* unit_name_id_t.fields.name_class selects the unit-name text page. */
/* party */
typedef enum party_name_class {
    PARTY_NAME_CLASS_SPECIAL = 0,
    PARTY_NAME_CLASS_GENERIC_MALE = 1,
    PARTY_NAME_CLASS_GENERIC_FEMALE = 2,
    PARTY_NAME_CLASS_GENERIC_MONSTER = 3
} party_name_class_e;

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
    u8 reaction_ability[2];     /* 0x08 */
    u8 support_ability[2];      /* 0x0a */
    u8 movement_ability[2];     /* 0x0c */
    u8 equipment[7];            /* 0x0e; unit_equipment_slot_e */
    u8 experience;              /* 0x15 */
    u8 level;                   /* 0x16 */
    u8 bravery;                 /* 0x17 */
    u8 faith;                   /* 0x18 */
    unit_raw_stats_t raw_stats; /* 0x19 */
    unit_career_data_t career;  /* 0x28; unlocked jobs, learned abilities, levels and JP */
    u8 name[16];                /* 0xbe */
    unit_name_id_t name_id;     /* 0xce */
    u8 proposition_status;      /* 0xd0; nonzero while unavailable on a proposition */
    u8 _unused_d1;
    u8 egg_color;        /* 0xd2; create/save/generate unit stores */
    u8 _unused_d3[0x2d]; /* 0xd3; saves and loads copy 0xe0 bytes per record, so 0xe0..0xff is not saved */
} party_unit_t;
typedef char party_unit_size_must_be_256[(sizeof(party_unit_t) == 256) ? 1 : -1];

/* Shared by ENTD definitions, persistent party records, and runtime battle
 * units. The byte combines persistence controls with unit-kind flags. */
typedef enum unit_flags {
    UNIT_FLAG_SAVE_FORMATION = 0x01,
    UNIT_FLAG_EGG = 0x04, /* a roster egg; on ENTD battle units it hides the numeric stats and scales HP/MP by 10 */
    UNIT_FLAG_LOAD_FORMATION = 0x08,
    UNIT_FLAG_JOIN_AFTER_EVENT = 0x10,
    UNIT_FLAG_MONSTER = 0x20,
    UNIT_FLAG_FEMALE = 0x40,
    UNIT_FLAG_MALE = 0x80,
} unit_flags_e;

/* animation */
/* unit_animation_state_t.rotate_mode: how the facing nibble steps toward target_facing. */
typedef enum unit_rotate_mode {
    UNIT_ROTATE_MODE_SHORTEST = 0,  /* the direction with fewer steps */
    UNIT_ROTATE_MODE_INCREMENT = 1, /* facing + 1 each step */
    UNIT_ROTATE_MODE_DECREMENT = 2, /* facing - 1 each step */
} unit_rotate_mode_e;

/* Per-unit facing/animation state written by the event interpreters and
 * advanced every frame by *_unit_update_facing_and_pending_animations.
 * Seven bytes per unit, 21 units. */
typedef struct unit_animation_state {
    u8 target_facing;
    u8 rotate_mode;     /* unit_rotate_mode_e */
    u8 frames_per_step; /* facing steps once step_counter reaches this */
    u8 step_counter;
    u8 rotating;       /* nonzero while stepping toward target_facing */
    u8 initial_facing; /* facing captured when animation states reset (0xff when
                          absent); RotateUnit direction 0x14 rotates back to it */
    u8 delay;          /* frames to wait before the first step */
} unit_animation_state_t;
typedef char unit_animation_state_size_must_be_7[sizeof(unit_animation_state_t) == 7 ? 1 : -1];

/* event coordinates */
extern SVECTOR g_unit_runtime_coords;

SVECTOR* unit_copy_world_coords_to_scratch(u32 runtime_id);
void unit_set_animation(s32 runtime_id, u32 animation);

#endif
