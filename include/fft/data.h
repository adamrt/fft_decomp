#ifndef FFT_DATA_H
#define FFT_DATA_H

/* Static game data shared across modules: abilities, items, jobs and skillsets. */

#include "psx/types.h"

/* ability */
/*
 * Ability ID classes.  The class boundaries are the SCUS Data Tables
 * "Ability Secondary Data" ranges (Default 0x000-0x16f, Item 0x170-0x17d,
 * Throw 0x17e-0x189, Jump 0x18a-0x195, Charge 0x196-0x19d, Math 0x19e-0x1a5,
 * R/S/M 0x1a6-0x1ff); the R/S/M split into 32 reaction, 32 support and the
 * movement abilities is the one applied by the SCUS ability classifiers.
 */
enum {
    ABILITY_ID_MASK = 0x1ff,
    ABILITY_ID_COUNT = 0x200,
    ABILITY_ID_ITEM_FIRST = 0x170,
    ABILITY_ID_THROW_FIRST = 0x17e,
    ABILITY_ID_JUMP_FIRST = 0x18a,
    ABILITY_ID_CHARGE_FIRST = 0x196,
    ABILITY_ID_MATH_FIRST = 0x19e,
    ABILITY_ID_REACTION_FIRST = 0x1a6,
    ABILITY_ID_SUPPORT_FIRST = 0x1c6,
    ABILITY_ID_MOVEMENT_FIRST = 0x1e6,
    /* 0x1fe/0x1ff request a random R/S/M pick; main_unit_calculate_rsm returns
     * any smaller id unchanged. */
    ABILITY_ID_RANDOM_FIRST = 0x1fe,
};

/* The random R/S/M selector uses a different legacy bit assignment from the
 * skillset-list filter: support and movement are reversed. */
typedef enum rsm_ability_filter {
    RSM_ABILITY_FILTER_REACTION = 2,
    RSM_ABILITY_FILTER_SUPPORT = 4,
    RSM_ABILITY_FILTER_MOVEMENT = 8,
} rsm_ability_filter_e;

/* Packed entries shared by the WORLD, BUNIT, and JOBSTTS ability menus. The
 * low-ten-bit mask is intentionally wider than ABILITY_ID_MASK: bit 9 is
 * unused by supported IDs, while bits 10 and 11 remain outside the ID. */
enum {
    ABILITY_LIST_ENTRY_ID_MASK = 0x03ff,
    ABILITY_LIST_ENTRY_DISABLED_SHIFT = 14,
    ABILITY_LIST_ENTRY_END = -1,
};

typedef enum ability_list_entry_flag {
    ABILITY_LIST_ENTRY_LEARNED = 0x1000,
    ABILITY_LIST_ENTRY_HIDE_DETAILS = 0x2000,
    ABILITY_LIST_ENTRY_DISABLED = 0x4000,
} ability_list_entry_flag_e;

/* Named PSX ability IDs. The R/S/M span is complete except for unnamed slots
 * and PSP-only additions. */
typedef enum ability_id {
    ABILITY_ID_BLACK_MAGIC_FROG = 0x1d,
    ABILITY_ID_SUMMON_MAGIC_GOLEM = 0x41,
    ABILITY_ID_SONG_ANGEL_SONG = 0x56,
    ABILITY_ID_SONG_LIFE_SONG = 0x57,
    ABILITY_ID_SONG_CHEER_SONG = 0x58,
    ABILITY_ID_SONG_BATTLE_SONG = 0x59,
    ABILITY_ID_SONG_MAGIC_SONG = 0x5a,
    ABILITY_ID_SONG_NAMELESS_SONG = 0x5b,
    ABILITY_ID_SONG_LAST_SONG = 0x5c,
    ABILITY_ID_DANCE_WITCH_HUNT = 0x5d,
    ABILITY_ID_DANCE_WIZNAIBUS = 0x5e,
    ABILITY_ID_DANCE_SLOW_DANCE = 0x5f,
    ABILITY_ID_DANCE_POLKA_POLKA = 0x60,
    ABILITY_ID_DANCE_DISILLUSION = 0x61,
    ABILITY_ID_DANCE_NAMELESS_DANCE = 0x62,
    ABILITY_ID_DANCE_LAST_DANCE = 0x63,
    ABILITY_ID_STEAL_HELMET = 0x6e,
    ABILITY_ID_STEAL_ARMOR = 0x6f,
    ABILITY_ID_STEAL_SHIELD = 0x70,
    ABILITY_ID_STEAL_WEAPON = 0x71,
    ABILITY_ID_STEAL_ACCESSORY = 0x72,
    ABILITY_ID_TALK_PERSUADE = 0x75,
    ABILITY_ID_TALK_PRAISE = 0x76,
    ABILITY_ID_TALK_THREATEN = 0x77,
    ABILITY_ID_TALK_PREACH = 0x78,
    ABILITY_ID_TALK_SOLUTION = 0x79,
    ABILITY_ID_TALK_DEATH_SENTENCE = 0x7a,
    ABILITY_ID_TALK_NEGOTIATE = 0x7b,
    ABILITY_ID_BATTLE_SKILL_HEAD_BREAK = 0x8a,
    ABILITY_ID_BATTLE_SKILL_ARMOR_BREAK = 0x8b,
    ABILITY_ID_BATTLE_SKILL_SHIELD_BREAK = 0x8c,
    ABILITY_ID_BATTLE_SKILL_WEAPON_BREAK = 0x8d,
    ABILITY_ID_BATTLE_SKILL_SPEED_BREAK = 0x8f,
    ABILITY_ID_BATTLE_SKILL_POWER_BREAK = 0x90,
    ABILITY_ID_BATTLE_SKILL_MIND_BREAK = 0x91,
    ABILITY_ID_BASIC_SKILL_ACCUMULATE = 0x92,
    ABILITY_ID_BASIC_SKILL_THROW_STONE = 0x94,
    ABILITY_ID_BASIC_SKILL_YELL = 0x96,
    ABILITY_ID_MIGHT_SWORD_SHELLBUST_STAB = 0xa0,
    ABILITY_ID_MIGHT_SWORD_BLASTAR_PUNCH = 0xa1,
    ABILITY_ID_MIGHT_SWORD_HELLCRY_PUNCH = 0xa2,
    ABILITY_ID_MIGHT_SWORD_ICEWOLF_BITE = 0xa3,
    ABILITY_ID_MONSTER_SKILL_SPEED_RUIN = 0xc5,
    ABILITY_ID_MONSTER_SKILL_POWER_RUIN = 0xc6,
    ABILITY_ID_MONSTER_SKILL_MIND_RUIN = 0xc7,
    ABILITY_ID_ELMDOR_BLOOD_SUCK = 0xc8,
    ABILITY_ID_MONSTER_SKILL_MOLDBALL_VIRUS = 0x149,
    ABILITY_ID_FROG_ATTACK = 0x16f,
    ABILITY_ID_THROW_SHURIKEN = 0x17e,
    ABILITY_ID_THROW_KNIFE = 0x17f,
    ABILITY_ID_THROW_BALL = 0x189,
    ABILITY_ID_JUMP_VERTICAL_8 = 0x195,
    ABILITY_ID_CHARGE_PLUS_1 = 0x196,
    ABILITY_ID_CHARGE_PLUS_20 = 0x19d,
    ABILITY_ID_REACTION_PA_SAVE = 0x1a6,
    ABILITY_ID_REACTION_MA_SAVE = 0x1a7,
    ABILITY_ID_REACTION_SPEED_SAVE = 0x1a8,
    ABILITY_ID_REACTION_SUNKEN_STATE = 0x1a9,
    ABILITY_ID_REACTION_CAUTION = 0x1aa,
    ABILITY_ID_REACTION_DRAGON_SPIRIT = 0x1ab,
    ABILITY_ID_REACTION_REGENERATOR = 0x1ac,
    ABILITY_ID_REACTION_BRAVE_UP = 0x1ad,
    ABILITY_ID_REACTION_FAITH_UP = 0x1ae,
    ABILITY_ID_REACTION_HP_RESTORE = 0x1af,
    ABILITY_ID_REACTION_MP_RESTORE = 0x1b0,
    ABILITY_ID_REACTION_CRITICAL_QUICK = 0x1b1,
    ABILITY_ID_REACTION_MEATBONE_SLASH = 0x1b2,
    ABILITY_ID_REACTION_COUNTER_MAGIC = 0x1b3,
    ABILITY_ID_REACTION_COUNTER_TACKLE = 0x1b4,
    ABILITY_ID_REACTION_COUNTER_FLOOD = 0x1b5,
    ABILITY_ID_REACTION_ABSORB_USED_MP = 0x1b6,
    ABILITY_ID_REACTION_GILGAME_HEART = 0x1b7,
    ABILITY_ID_REACTION_REFLECT = 0x1b8,
    ABILITY_ID_REACTION_AUTO_POTION = 0x1b9,
    ABILITY_ID_REACTION_COUNTER = 0x1ba,
    ABILITY_ID_REACTION_DISTRIBUTE = 0x1bc,
    ABILITY_ID_REACTION_MP_SWITCH = 0x1bd,
    ABILITY_ID_REACTION_DAMAGE_SPLIT = 0x1be,
    ABILITY_ID_REACTION_WEAPON_GUARD = 0x1bf,
    ABILITY_ID_REACTION_FINGER_GUARD = 0x1c0,
    ABILITY_ID_REACTION_ABANDON = 0x1c1,
    ABILITY_ID_REACTION_CATCH = 0x1c2,
    ABILITY_ID_REACTION_BLADE_GRASP = 0x1c3,
    ABILITY_ID_REACTION_ARROW_GUARD = 0x1c4,
    ABILITY_ID_REACTION_HAMEDO = 0x1c5,
    ABILITY_ID_SUPPORT_EQUIP_ARMOR = 0x1c6,
    ABILITY_ID_SUPPORT_EQUIP_SHIELD = 0x1c7,
    ABILITY_ID_SUPPORT_EQUIP_SWORD = 0x1c8,
    ABILITY_ID_SUPPORT_EQUIP_KNIFE = 0x1c9,
    ABILITY_ID_SUPPORT_EQUIP_CROSSBOW = 0x1ca,
    ABILITY_ID_SUPPORT_EQUIP_SPEAR = 0x1cb,
    ABILITY_ID_SUPPORT_EQUIP_AXE = 0x1cc,
    ABILITY_ID_SUPPORT_EQUIP_GUN = 0x1cd,
    ABILITY_ID_SUPPORT_HALF_OF_MP = 0x1ce,
    ABILITY_ID_SUPPORT_GAINED_JP_UP = 0x1cf,
    ABILITY_ID_SUPPORT_GAINED_EXP_UP = 0x1d0,
    ABILITY_ID_SUPPORT_ATTACK_UP = 0x1d1,
    ABILITY_ID_SUPPORT_DEFENSE_UP = 0x1d2,
    ABILITY_ID_SUPPORT_MAGIC_ATTACK_UP = 0x1d3,
    ABILITY_ID_SUPPORT_MAGIC_DEFEND_UP = 0x1d4,
    ABILITY_ID_SUPPORT_CONCENTRATE = 0x1d5,
    ABILITY_ID_SUPPORT_TRAIN = 0x1d6,
    ABILITY_ID_SUPPORT_SECRET_HUNT = 0x1d7,
    ABILITY_ID_SUPPORT_MARTIAL_ARTS = 0x1d8,
    ABILITY_ID_SUPPORT_MONSTER_TALK = 0x1d9,
    ABILITY_ID_SUPPORT_THROW_ITEM = 0x1da,
    ABILITY_ID_SUPPORT_MAINTENANCE = 0x1db,
    ABILITY_ID_SUPPORT_TWO_HANDS = 0x1dc,
    ABILITY_ID_SUPPORT_TWO_SWORDS = 0x1dd,
    ABILITY_ID_SUPPORT_MONSTER_SKILL = 0x1de,
    ABILITY_ID_SUPPORT_DEFEND = 0x1df,
    ABILITY_ID_SUPPORT_EQUIP_CHANGE = 0x1e0,
    ABILITY_ID_SUPPORT_SHORT_CHARGE = 0x1e2,
    ABILITY_ID_SUPPORT_NON_CHARGE = 0x1e3,
    ABILITY_ID_MOVEMENT_MOVE_PLUS_1 = 0x1e6,
    ABILITY_ID_MOVEMENT_MOVE_PLUS_2 = 0x1e7,
    ABILITY_ID_MOVEMENT_MOVE_PLUS_3 = 0x1e8,
    ABILITY_ID_MOVEMENT_JUMP_PLUS_1 = 0x1e9,
    ABILITY_ID_MOVEMENT_JUMP_PLUS_2 = 0x1ea,
    ABILITY_ID_MOVEMENT_JUMP_PLUS_3 = 0x1eb,
    ABILITY_ID_MOVEMENT_IGNORE_HEIGHT = 0x1ec,
    ABILITY_ID_MOVEMENT_MOVE_HP_UP = 0x1ed,
    ABILITY_ID_MOVEMENT_MOVE_MP_UP = 0x1ee,
    ABILITY_ID_MOVEMENT_MOVE_GET_EXP = 0x1ef,
    ABILITY_ID_MOVEMENT_MOVE_GET_JP = 0x1f0,
    ABILITY_ID_MOVEMENT_TELEPORT = 0x1f2,
    ABILITY_ID_MOVEMENT_TELEPORT_2 = 0x1f3,
    ABILITY_ID_MOVEMENT_ANY_WEATHER = 0x1f4,
    ABILITY_ID_MOVEMENT_ANY_GROUND = 0x1f5,
    ABILITY_ID_MOVEMENT_MOVE_IN_WATER = 0x1f6,
    ABILITY_ID_MOVEMENT_WALK_ON_WATER = 0x1f7,
    ABILITY_ID_MOVEMENT_MOVE_ON_LAVA = 0x1f8,
    ABILITY_ID_MOVEMENT_MOVE_UNDERWATER = 0x1f9,
    ABILITY_ID_MOVEMENT_FLOAT = 0x1fa,
    ABILITY_ID_MOVEMENT_FLY = 0x1fb,
    ABILITY_ID_MOVEMENT_SILENT_WALK = 0x1fc,
    ABILITY_ID_MOVEMENT_MOVE_FIND_ITEM = 0x1fd,
} ability_id_e;

typedef enum ability_secondary_flags_3 {
    ABILITY_SECONDARY_FLAG_3_SPELL_QUOTE = 0x02,
    ABILITY_SECONDARY_FLAG_3_PERSEVERE = 0x04,
    ABILITY_SECONDARY_FLAG_3_BLOCKED_BY_GOLEM = 0x08,
    ABILITY_SECONDARY_FLAG_3_AFFECTED_BY_SILENCE = 0x20,
    ABILITY_SECONDARY_FLAG_3_CALCULATOR_ELIGIBLE = 0x40,
    ABILITY_SECONDARY_FLAG_3_REFLECTABLE = 0x80,
} ability_secondary_flags_3_e;

/* Flags in byte 0x03 of the eight-byte primary ability records. */
typedef enum ability_data_flags {
    ABILITY_DATA_FLAG_SPELL_QUOTE = 0x40,
} ability_data_flags_e;

/* Default-ability secondary records at 0x8005fbf0.
 * The 14-byte stride/range are used by main_ability_get_range; BATTLE
 * 0x8018b738 reads flags_1, 0x8019e6b8 reads flags_2, and 0x80195c70
 * copies element into the considered AI ability. Its loader reads CT at
 * 0x80195c64 and AoE at 0x80195cec. Other bytes stay provisional. */
typedef struct ability_secondary_data {
    u8 range;                /* 0x00 */
    u8 aoe;                  /* 0x01 */
    u8 vertical;             /* 0x02; passed as the vertical tolerance at 0x8017a47c */
    u8 flags_1;              /* 0x03; ability_secondary_flags_1_e */
    u8 flags_2;              /* 0x04; ability_secondary_flags_2_e */
    u8 flags_3;              /* 0x05; ability_secondary_flags_3_e */
    u8 flags_4;              /* 0x06; ability_secondary_flags_4_e */
    u8 element;              /* 0x07 */
    u8 formula;              /* 0x08 */
    u8 x;                    /* 0x09; multi-hit formulas take the hit count from it */
    u8 y;                    /* 0x0a */
    u8 status_infliction_id; /* 0x0b; selects the six-byte status record at 0x8019bd78 */
    u8 ct;                   /* 0x0c */
    u8 mp_cost;              /* 0x0d; ability-list eligibility compares this with max MP at 0x8019a89c */
} ability_secondary_data_t;
typedef char ability_secondary_data_size_must_be_14[(sizeof(ability_secondary_data_t) == 14) ? 1 : -1];

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

/* Six-byte status-infliction records at 0x80063fc4; byte 0 selects how the
 * five-byte status set is applied. */
typedef struct status_infliction_data {
    u8 type;
    u8 statuses[5];
} status_infliction_data_t;
typedef char battle_status_infliction_record_size_must_be_6[(sizeof(status_infliction_data_t) == 6) ? 1 : -1];

/* item */
enum {
    ITEM_ATTRIBUTE_PHYSICAL_ATTACK = 0,
    ITEM_ATTRIBUTE_MAGICAL_ATTACK = 1,
    ITEM_ATTRIBUTE_SPEED = 2,
};

/* Item categories stored in item_data_t.type and in the four-byte job/unit
 * equippable-type set.  The table layout starts at 0x80062eb8. */
typedef enum item_type {
    ITEM_TYPE_UNARMED = 0x00,
    ITEM_TYPE_KNIFE = 0x01,
    ITEM_TYPE_NINJA_BLADE = 0x02,
    ITEM_TYPE_SWORD = 0x03,
    ITEM_TYPE_KNIGHT_SWORD = 0x04,
    ITEM_TYPE_KATANA = 0x05,
    ITEM_TYPE_AXE = 0x06,
    ITEM_TYPE_ROD = 0x07,
    ITEM_TYPE_STAFF = 0x08,
    ITEM_TYPE_FLAIL = 0x09,
    ITEM_TYPE_GUN = 0x0a,
    ITEM_TYPE_CROSSBOW = 0x0b,
    ITEM_TYPE_BOW = 0x0c,
    ITEM_TYPE_INSTRUMENT = 0x0d,
    ITEM_TYPE_BOOK = 0x0e,
    ITEM_TYPE_POLEARM = 0x0f,
    ITEM_TYPE_POLE = 0x10,
    ITEM_TYPE_BAG = 0x11,
    ITEM_TYPE_CLOTH = 0x12,
    ITEM_TYPE_SHIELD = 0x13,
    ITEM_TYPE_HELMET = 0x14,
    ITEM_TYPE_HAT = 0x15,
    ITEM_TYPE_HAIR_ADORNMENT = 0x16,
    ITEM_TYPE_ARMOR = 0x17,
    ITEM_TYPE_CLOTHING = 0x18,
    ITEM_TYPE_ROBE = 0x19,
    ITEM_TYPE_SHOES = 0x1a,
    ITEM_TYPE_ARMGUARD = 0x1b,
    ITEM_TYPE_RING = 0x1c,
    ITEM_TYPE_ARMLET = 0x1d,
    ITEM_TYPE_CLOAK = 0x1e,
    ITEM_TYPE_PERFUME = 0x1f,
    ITEM_TYPE_THROWING = 0x20,
    ITEM_TYPE_BOMB = 0x21,
    ITEM_TYPE_CONSUMABLE = 0x22,
    ITEM_TYPE_COUNT = 0x23,
} item_type_e;

typedef enum item_type_flag {
    ITEM_TYPE_FLAG_UNREMOVABLE = 0x01,
    ITEM_TYPE_FLAG_RARE = 0x02,
    ITEM_TYPE_FLAG_UNKNOWN_04 = 0x04,
    ITEM_TYPE_FLAG_ACCESSORY = 0x08,
    ITEM_TYPE_FLAG_BODY_ARMOR = 0x10,
    ITEM_TYPE_FLAG_HEADGEAR = 0x20,
    ITEM_TYPE_FLAG_SHIELD = 0x40,
    ITEM_TYPE_FLAG_WEAPON = 0x80,
} item_type_flag_e;

typedef enum weapon_flag {
    WEAPON_FLAG_FORCED_TWO_HANDS = 0x01,
    WEAPON_FLAG_THROWABLE = 0x02,
    WEAPON_FLAG_TWO_HANDS_COMPATIBLE = 0x04,
    WEAPON_FLAG_TWO_SWORDS_COMPATIBLE = 0x08,
    WEAPON_FLAG_ARC = 0x10,
    WEAPON_FLAG_DIRECT = 0x20,
    WEAPON_FLAG_LUNGING = 0x40,
    WEAPON_FLAG_STRIKING = 0x80,
} weapon_flag_e;

/* Item IDs index the 256-row primary item table at 0x80062eb8. Only IDs used
 * as named boundaries or fixed inventory entries are listed here. Item 0 is
 * Nothing, the unarmed item record. Rows 0xfe and 0xff otherwise contain zero
 * data but retain ITEM_TYPE_FLAG_RARE; ENTD uses them for Random and None,
 * while the random selector also uses 0xff as an Any-type filter. */
typedef enum item_id {
    ITEM_ID_NOTHING = 0x00,
    ITEM_ID_DAGGER = 0x01,
    ITEM_ID_POTION = 0xf0,
    ITEM_ID_HI_POTION = 0xf1,
    ITEM_ID_X_POTION = 0xf2,
    ITEM_ID_ETHER = 0xf3,
    ITEM_ID_HI_ETHER = 0xf4,
    ITEM_ID_ELIXIR = 0xf5,
    ITEM_ID_ANTIDOTE = 0xf6,
    ITEM_ID_EYE_DROP = 0xf7,
    ITEM_ID_ECHO_GRASS = 0xf8,
    ITEM_ID_MAIDENS_KISS = 0xf9,
    ITEM_ID_SOFT = 0xfa,
    ITEM_ID_HOLY_WATER = 0xfb,
    ITEM_ID_REMEDY = 0xfc,
    ITEM_ID_PHOENIX_DOWN = 0xfd,
    ITEM_ID_NONE = 0xff,
} item_id_e;

/* The four equippable-type bytes use MSB-first bit order for item types
 * 0x00..0x1f (the Job Data table at 0x800610b8). */
#define EQUIPPABLE_ITEM_TYPE_BYTE_INDEX(type) ((type) >> 3)

#define EQUIPPABLE_ITEM_TYPE_BYTE_MASK(type) (0x80 >> ((type) & 7))

/* world_formation_unit_t.equipment halfwords carry the item id in the low ten
 * bits and flags above (bit 14 is tested by world_shop_buy_from_fitting_room
 * and the job wheel); the WORLD item tables are indexed by the masked id
 * (world_item_get_price, world_item_get_type). The range boundaries follow the
 * retail item table: throwable weapons start with Shuriken, followed by
 * shields, headgear, body armor, accessories, and consumables. */
enum {
    WORLD_ITEM_ID_MASK = 0x3ff,
    ITEM_ID_THROWABLE_FIRST = 0x7a,
    ITEM_ID_SHIELD_FIRST = 0x80,
    ITEM_ID_HEADGEAR_FIRST = 0x90,
    ITEM_ID_BODY_ARMOR_FIRST = 0xac,
    ITEM_ID_ACCESSORY_FIRST = 0xd0,
    ITEM_ID_CONSUMABLE_FIRST = 0xf0,
};

/* Broad item categories shared by the WORLD and EQUIP menu classifiers.
 * Throwable weapons and consumables both use OTHER because neither maps to a
 * normal equipment slot category. */
typedef enum item_menu_category {
    ITEM_MENU_CATEGORY_WEAPON = 0,
    ITEM_MENU_CATEGORY_SHIELD = 1,
    ITEM_MENU_CATEGORY_HEADGEAR = 2,
    ITEM_MENU_CATEGORY_BODY_ARMOR = 3,
    ITEM_MENU_CATEGORY_ACCESSORY = 4,
    ITEM_MENU_CATEGORY_OTHER = 5,
} item_menu_category_e;

typedef struct item_data {
    u8 palette;
    u8 sprite_id;
    u8 required_level;
    u8 type_flags;
    u8 secondary_data_id;
    u8 type;
    u8 unused_06;
    u8 attributes;
    u16 price;
    u8 shop_availability;
    u8 unused_0b;
} item_data_t;
typedef char item_data_size_must_be_12[(sizeof(item_data_t) == 12) ? 1 : -1];

/* Secondary item records selected by item_data_t.secondary_data_id. Offsets
 * are fixed by main_unit_set_equipment_stats and
 * main_unit_calculate_entd_equipment. */
typedef struct weapon_data {
    u8 range;      /* 0x00 */
    u8 flags;      /* 0x01; weapon_flag_e */
    u8 formula;    /* 0x02 */
    u8 unknown_03; /* 0x03 */
    u8 power;      /* 0x04; copied to battle_stats_t equipment-stat slots 0x3c/0x3d */
    u8 evade;      /* 0x05; copied to battle_stats_t 0x3e/0x3f */
    u8 element;    /* 0x06 */
    u8 proc_id;    /* 0x07; status-infliction or spell ID copied to the active ability */
} weapon_data_t;
typedef char weapon_data_size_must_be_8[(sizeof(weapon_data_t) == 8) ? 1 : -1];

/* Three-byte item ability records at 0x80063f98. Item abilities select these
 * records directly; formula handlers use z and ability setup uses the status
 * infliction id. */
typedef struct item_secondary_data {
    u8 formula;
    u8 z;
    u8 status_infliction_id;
} item_secondary_data_t;
typedef char item_secondary_data_size_must_be_3[(sizeof(item_secondary_data_t) == 3) ? 1 : -1];

/* Shield records at 0x80063eb8 (g_main_item_shield_data), indexed by
 * item_data_t.secondary_data_id for items with type flag 0x40. */
typedef struct shield_data {
    u8 physical_evade; /* 0x00; battle_stats_t 0x40 */
    u8 magic_evade;    /* 0x01; battle_stats_t 0x44 */
} shield_data_t;

/* Accessory records at 0x80063f58 (g_main_item_accessory_data), indexed by
 * item_data_t.secondary_data_id for items with type flag 0x08. */
typedef struct accessory_data {
    u8 physical_evade;
    u8 magic_evade;
} accessory_data_t;

typedef struct armor_data {
    u8 hp_bonus; /* 0x00; battle_stats_t 0x41/0x42 */
    u8 mp_bonus; /* 0x01; battle_stats_t 0x45/0x46 */
} armor_data_t;

/* Static innate, immunity and starting-status masks embedded in item and job
 * records. This differs from battle_status_sets_t, whose third set is the
 * mutable current-status mask. */
typedef struct initial_status_sets {
    u8 innate[5];
    u8 immunity[5];
    u8 starting[5];
} initial_status_sets_t;
typedef char initial_status_sets_size_must_be_15[(sizeof(initial_status_sets_t) == 15) ? 1 : -1];

/* 25-byte item_t Attribute record at 0x800642c4. MAIN applies these bonuses
 * to battle units; WORLD 0x80122f9c reads the same first five bytes for
 * equipment previews. */
typedef struct item_attribute {
    u8 attributes[3];                  /* 0x00; ITEM_ATTRIBUTE_* indices */
    u8 move;                           /* 0x03 */
    u8 jump;                           /* 0x04 */
    initial_status_sets_t status_sets; /* 0x05 */
    u8 elemental_affinity[5];          /* 0x14; elemental_affinity_index_e */
} item_attribute_t;
typedef char item_attribute_size_must_be_25[(sizeof(item_attribute_t) == 25) ? 1 : -1];

/* Flat byte view of item attributes used by the status-icon renderers. */
typedef struct main_item_attribute_flat {
    u8 attributes[3];
    u8 move;
    u8 jump;
    u8 innate[5];
    u8 immunity[5];
    u8 starting[5];
    u8 elemental_affinity[5];
} main_item_attribute_flat_t;

/* The right/left weapon slot pair tested by equip_unit_is_two_handing_weapon;
 * equip_unit_data_t.equipment[0..1] in the caller. */
typedef struct weapon_pair {
    s16 slot_a;
    s16 slot_b;
} weapon_pair_t;

/* 0x80057b20..0x80057b5b: seven independent signed-byte order lists.
 * main_item_init_order_tables establishes capacities 12/8/8/8/8/8/8 with -1
 * terminators; member categories remain unproven. */
typedef struct item_type_order_tables {
    s8 order_0[12];
    s8 order_1[8];
    s8 order_2[8];
    s8 order_3[8];
    s8 order_4[8];
    s8 order_5[8];
    s8 order_6[8];
} item_type_order_tables_t;

/* job */
enum {
    JOB_JUMP_VALUE_MASK = 0x7f,
    JOB_JUMP_STEPPING_STONE = 0x80,
};

/*
 * 0x30-byte job record (g_job_data_pointer). The named fields are those copied
 * by main_unit_copy_job_data, main_unit_calculate_abilities and
 * main_unit_set_equipment_stats into battle_stats_t.
 */
typedef struct job_data {
    u8 skillset;                       /* 0x00 */
    u8 innate_abilities[8];            /* 0x01; four little-endian u16 ability ids, byte-copied */
    u8 equipment_categories[4];        /* 0x09 */
    u8 growths_multipliers[10];        /* 0x0d; copied to battle_stats_t 0x81 */
    u8 move;                           /* 0x17 */
    u8 jump;                           /* 0x18; JOB_JUMP_* */
    u8 class_evade;                    /* 0x19; battle_stats_t equipment-stat slot 0x43 */
    initial_status_sets_t status_sets; /* 0x1a */
    u8 elemental_affinity[4];          /* 0x29; affinity indices 0 through 3 */
    u8 spritesheet_id;                 /* 0x2d */
    u8 job_portrait_palette;           /* 0x2e */
    u8 graphic_variant;                /* 0x2f; copied to battle_stats_t 0x15e */
} job_data_t;
typedef char job_data_size_must_be_48[(sizeof(job_data_t) == 48) ? 1 : -1];

/* PSX job-table IDs. Use PSX names, not the parenthesized PSP translations.
 * Unnamed table slots are deliberately omitted; neither a blank label nor
 * "None" proves a sentinel. Character/variant suffixes distinguish separate
 * records with the same job. The PSP-only additions beginning at 0xa0 are not
 * part of this target. */
typedef enum job_id {
    JOB_ID_SQUIRE_RAMZA_CHAPTER_1 = 0x01,
    JOB_ID_SQUIRE_RAMZA_CHAPTER_2_3 = 0x02,
    JOB_ID_SQUIRE_RAMZA_CHAPTER_4 = 0x03,
    JOB_ID_SQUIRE_DELITA = 0x04,
    JOB_ID_HOLY_KNIGHT_DELITA = 0x05,
    JOB_ID_ARC_KNIGHT_DELITA = 0x06,
    JOB_ID_SQUIRE_ALGUS = 0x07,
    JOB_ID_ARC_KNIGHT_ZALBAG = 0x08,
    JOB_ID_LUNE_KNIGHT_DYCEDARG = 0x09,
    JOB_ID_DUKE_LARG = 0x0a,
    JOB_ID_DUKE_GOLTANA = 0x0b,
    JOB_ID_PRINCESS_OVELIA = 0x0c,
    JOB_ID_HOLY_SWORDSMAN_ORLANDU = 0x0d,
    JOB_ID_HIGH_PRIEST_FUNERAL = 0x0e,
    JOB_ID_DRAGONER_REIS = 0x0f,
    JOB_ID_HOLY_PRIEST_ZALMO = 0x10,
    JOB_ID_DARK_KNIGHT_GAFGARION_ENEMY = 0x11,
    JOB_ID_HELL_KNIGHT_MALAK_INNATE_KO = 0x12,
    JOB_ID_BISHOP_SIMON = 0x13,
    JOB_ID_CLERIC_ALMA_STARTING_KO = 0x14,
    JOB_ID_ASTROLOGIST_OLAN = 0x15,
    JOB_ID_ENGINEER_MUSTADIO_JOIN = 0x16,
    JOB_ID_DARK_KNIGHT_GAFGARION_GUEST = 0x17,
    JOB_ID_CARDINAL_DRACLAU = 0x18,
    JOB_ID_HEAVEN_KNIGHT_RAFA_GUEST = 0x19,
    JOB_ID_HELL_KNIGHT_MALAK = 0x1a,
    JOB_ID_ARC_KNIGHT_ELMDOR = 0x1b,
    JOB_ID_DELITAS_SIS_TETA = 0x1c,
    JOB_ID_ARC_DUKE_BARINTEN = 0x1d,
    JOB_ID_HOLY_KNIGHT_AGRIAS_JOIN = 0x1e,
    JOB_ID_TEMPLE_KNIGHT_BEOWULF = 0x1f,
    JOB_ID_WHITE_KNIGHT_WIEGRAF_CHAPTER_1 = 0x20,
    JOB_ID_ARC_WITCH_BALMAFULA = 0x21,
    JOB_ID_ENGINEER_MUSTADIO_GUEST = 0x22,
    JOB_ID_BI_COUNT_RUDVICH = 0x23,
    JOB_ID_DIVINE_KNIGHT_VORMAV = 0x24,
    JOB_ID_DIVINE_KNIGHT_ROFEL = 0x25,
    JOB_ID_KNIGHT_BLADE_IZLUDE = 0x26,
    JOB_ID_SORCERER_KLETIAN = 0x27,
    JOB_ID_WHITE_KNIGHT_WIEGRAF_CHAPTER_3 = 0x28,
    JOB_ID_HEAVEN_KNIGHT_RAFA_JOIN = 0x29,
    JOB_ID_DIVINE_KNIGHT_MELIADOUL_JOIN = 0x2a,
    JOB_ID_ENGINEER_BALK = 0x2b,
    JOB_ID_CLERIC_ALMA_INNATE_KO = 0x2c,
    JOB_ID_ASSASSIN_CELIA = 0x2d,
    JOB_ID_ASSASSIN_LEDE = 0x2e,
    JOB_ID_DIVINE_KNIGHT_MELIADOUL_ENEMY = 0x2f,
    JOB_ID_CLERIC_ALMA = 0x30,
    JOB_ID_PHONY_SAINT_AJORA = 0x31,
    JOB_ID_SOLDIER_CLOUD = 0x32,
    JOB_ID_ARC_KNIGHT_ZALBAG_VAMPIRE = 0x33,
    JOB_ID_HOLY_KNIGHT_AGRIAS_GUEST = 0x34,
    JOB_ID_CHEMIST_35 = 0x35,
    JOB_ID_PRIEST_36 = 0x36,
    JOB_ID_WIZARD_37 = 0x37,
    JOB_ID_ORACLE_38 = 0x38,
    JOB_ID_ORACLE_39 = 0x39,

    JOB_ID_WARLOCK_VELIUS = 0x3c,
    JOB_ID_KNIGHT_UNDEAD = 0x3d,
    JOB_ID_ANGEL_OF_DEATH_ZALERA = 0x3e,
    JOB_ID_ARCHER_UNDEAD = 0x3f,
    JOB_ID_REGULATOR_HASHMALUM = 0x40,
    JOB_ID_HOLY_ANGEL_ALTIMA = 0x41,
    JOB_ID_WIZARD_UNDEAD = 0x42,
    JOB_ID_IMPURE_KING_QUEKLAIN = 0x43,
    JOB_ID_TIME_MAGE_UNDEAD = 0x44,
    JOB_ID_GHOST_OF_FURY_ADRAMELK = 0x45,
    JOB_ID_ORACLE_UNDEAD = 0x46,
    JOB_ID_SUMMONER_UNDEAD = 0x47,
    JOB_ID_HOLY_DRAGON_REIS = 0x48,
    JOB_ID_ARCH_ANGEL_ALTIMA = 0x49,

    JOB_ID_SQUIRE = 0x4a,
    JOB_ID_CHEMIST = 0x4b,
    JOB_ID_KNIGHT = 0x4c,
    JOB_ID_ARCHER = 0x4d,
    JOB_ID_MONK = 0x4e,
    JOB_ID_PRIEST = 0x4f,
    JOB_ID_WIZARD = 0x50,
    JOB_ID_TIME_MAGE = 0x51,
    JOB_ID_SUMMONER = 0x52,
    JOB_ID_THIEF = 0x53,
    JOB_ID_MEDIATOR = 0x54,
    JOB_ID_ORACLE = 0x55,
    JOB_ID_GEOMANCER = 0x56,
    JOB_ID_LANCER = 0x57,
    JOB_ID_SAMURAI = 0x58,
    JOB_ID_NINJA = 0x59,
    JOB_ID_CALCULATOR = 0x5a,
    JOB_ID_BARD = 0x5b,
    JOB_ID_DANCER = 0x5c,
    JOB_ID_MIME = 0x5d,

    JOB_ID_CHOCOBO = 0x5e,
    JOB_ID_BLACK_CHOCOBO = 0x5f,
    JOB_ID_RED_CHOCOBO = 0x60,
    JOB_ID_GOBLIN = 0x61,
    JOB_ID_BLACK_GOBLIN = 0x62,
    JOB_ID_GOBBLEDEGUCK = 0x63,
    JOB_ID_BOMB = 0x64,
    JOB_ID_GRENADE = 0x65,
    JOB_ID_EXPLOSIVE = 0x66,
    JOB_ID_RED_PANTHER = 0x67,
    JOB_ID_CUAR = 0x68,
    JOB_ID_VAMPIRE = 0x69,
    JOB_ID_PISCO_DEMON = 0x6a,
    JOB_ID_SQUIDLARKIN = 0x6b,
    JOB_ID_MINDFLARE = 0x6c,
    JOB_ID_SKELETON = 0x6d,
    JOB_ID_BONE_SNATCH = 0x6e,
    JOB_ID_LIVING_BONE = 0x6f,
    JOB_ID_GHOUL = 0x70,
    JOB_ID_GUST = 0x71,
    JOB_ID_REVNANT = 0x72,
    JOB_ID_FLOTIBALL = 0x73,
    JOB_ID_AHRIMAN = 0x74,
    JOB_ID_PLAGUE = 0x75,
    JOB_ID_JURAVIS = 0x76,
    JOB_ID_STEEL_HAWK = 0x77,
    JOB_ID_COCATORIS = 0x78,
    JOB_ID_URIBO = 0x79,
    JOB_ID_PORKY = 0x7a,
    JOB_ID_WILDBOW = 0x7b,
    JOB_ID_WOODMAN = 0x7c,
    JOB_ID_TRENT = 0x7d,
    JOB_ID_TAIJU = 0x7e,
    JOB_ID_BULL_DEMON = 0x7f,
    JOB_ID_MINITAURUS = 0x80,
    JOB_ID_SACRED = 0x81,
    JOB_ID_MORBOL = 0x82,
    JOB_ID_OCHU = 0x83,
    JOB_ID_GREAT_MORBOL = 0x84,
    JOB_ID_BEHEMOTH = 0x85,
    JOB_ID_KING_BEHEMOTH = 0x86,
    JOB_ID_DARK_BEHEMOTH = 0x87,
    JOB_ID_DRAGON = 0x88,
    JOB_ID_BLUE_DRAGON = 0x89,
    JOB_ID_RED_DRAGON = 0x8a,
    JOB_ID_HYUDRA = 0x8b,
    JOB_ID_HYDRA = 0x8c,
    JOB_ID_TIAMAT = 0x8d,

    JOB_ID_BYBLOS = 0x90,
    JOB_ID_STEEL_GIANT = 0x91,
    JOB_ID_APANDA = 0x96,
    JOB_ID_SERPENTARIUS = 0x97,
    JOB_ID_HOLY_DRAGON_98 = 0x98,
    JOB_ID_ARCHAIC_DEMON = 0x99,
    JOB_ID_ULTIMA_DEMON = 0x9a,
} job_id_e;

/* Table/range sizes are counts, not additional job IDs. The special-monster
 * predicates include undead human jobs and unnamed slots, not only monsters. */
enum {
    JOB_ID_SPECIAL_MONSTER_1_FIRST = JOB_ID_WARLOCK_VELIUS,
    JOB_ID_SPECIAL_MONSTER_1_COUNT = 14,
    JOB_ID_GENERIC_FIRST = JOB_ID_SQUIRE,
    JOB_ID_GENERIC_LAST = JOB_ID_MIME,
    JOB_ID_GENERIC_COUNT = 20,
    JOB_ID_MONSTER_FIRST = JOB_ID_CHOCOBO,
    JOB_ID_MONSTER_LAST = JOB_ID_TIAMAT,
    JOB_ID_MONSTER_COUNT = JOB_ID_MONSTER_LAST - JOB_ID_MONSTER_FIRST + 1,
    JOB_ID_CHOCOBO_FAMILY_LAST = JOB_ID_RED_CHOCOBO,
    JOB_ID_CHOCOBO_FAMILY_COUNT = JOB_ID_CHOCOBO_FAMILY_LAST - JOB_ID_CHOCOBO + 1,
    JOB_ID_MORBOL_FAMILY_LAST = JOB_ID_GREAT_MORBOL,
    JOB_ID_MORBOL_FAMILY_COUNT = JOB_ID_MORBOL_FAMILY_LAST - JOB_ID_MORBOL + 1,
    JOB_ID_SPECIAL_MONSTER_2_FIRST = JOB_ID_BYBLOS,
    JOB_ID_SPECIAL_MONSTER_2_COUNT = 11,
    JOB_ID_COUNT = 160,
};

/* skillset */
/* PSX skillset IDs established by the main executable's table at 0x80064a94.
 * IDs after Mimic are character- and enemy-specific, often with duplicate
 * display names, so they remain unnamed until a consumer distinguishes them. */
typedef enum skillset_id {
    SKILLSET_ID_ATTACK = 0x01,
    SKILLSET_ID_DEFEND = 0x02,
    SKILLSET_ID_EQUIP_CHANGE = 0x03,
    SKILLSET_ID_BLANK_04 = 0x04,
    SKILLSET_ID_BASIC_SKILL = 0x05,
    SKILLSET_ID_ITEM = 0x06,
    SKILLSET_ID_BATTLE_SKILL = 0x07,
    SKILLSET_ID_CHARGE = 0x08,
    SKILLSET_ID_PUNCH_ART = 0x09,
    SKILLSET_ID_WHITE_MAGIC = 0x0a,
    SKILLSET_ID_BLACK_MAGIC = 0x0b,
    SKILLSET_ID_TIME_MAGIC = 0x0c,
    SKILLSET_ID_SUMMON_MAGIC = 0x0d,
    SKILLSET_ID_STEAL = 0x0e,
    SKILLSET_ID_TALK_SKILL = 0x0f,
    SKILLSET_ID_YIN_YANG_MAGIC = 0x10,
    SKILLSET_ID_ELEMENTAL = 0x11,
    SKILLSET_ID_JUMP = 0x12,
    SKILLSET_ID_DRAW_OUT = 0x13,
    SKILLSET_ID_THROW = 0x14,
    SKILLSET_ID_MATH_SKILL = 0x15,
    SKILLSET_ID_SING = 0x16,
    SKILLSET_ID_DANCE = 0x17,
    SKILLSET_ID_MIMIC = 0x18,
    SKILLSET_ID_ELMDOR_BLOOD_SUCK = 0xa7,
    SKILLSET_ID_FROG_ATTACK = 0xa9,
    SKILLSET_ID_NONE = 0xff, /* empty ENTD skillset and AI ability-list terminator */
} skillset_id_e;

enum {
    SKILLSET_ID_MONSTER_FIRST = 0xb0,
    SKILLSET_ID_END = 0xe0,
};

typedef enum skillset_ability_filter {
    SKILLSET_ABILITY_FILTER_ACTION = 1,
    SKILLSET_ABILITY_FILTER_REACTION = 2,
    SKILLSET_ABILITY_FILTER_MOVEMENT = 4,
    SKILLSET_ABILITY_FILTER_SUPPORT = 8,
    SKILLSET_ABILITY_FILTER_ALL = 0xf,
} skillset_ability_filter_e;

/* status */
enum {
    BATTLE_STATUS_COUNT = 40,
    BATTLE_STATUS_BYTE_COUNT = 5,
    BATTLE_TIMED_STATUS_COUNT = 16,
};

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

/* other */
/* Human skillset records contain 22 ids, monster records contain 4, and the
 * shared scratch list reserves 24 entries for either representation. */
enum {
    HUMAN_SKILLSET_ABILITY_COUNT = 22,
    MONSTER_SKILLSET_ABILITY_COUNT = 4,
    SKILLSET_ABILITY_LIST_COUNT = 24,
};

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
    CALCULATOR_ABILITY_COUNT = 8,
    CALCULATOR_PRIME_COUNT = 54,
};

#endif
