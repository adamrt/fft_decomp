#ifndef FFT_DATA_H
#define FFT_DATA_H

#include "fft/battle_unit_position.h"
#include "fft/character_identity.h"
#include "fft/thread.h"
#include "fft/unit_equipment.h"
#include "fft/unit_flags.h"
#include "fft/unit_stats.h"
#include "psx/types.h"

enum {
    ENTD_UNITS_PER_ENCOUNTER = 16,
    ENTD_ENCOUNTERS_PER_FILE = 128,
};

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

/* Human skillset records contain 22 ids, monster records contain 4, and the
 * shared scratch list reserves 24 entries for either representation. */
enum {
    HUMAN_SKILLSET_ABILITY_COUNT = 22,
    MONSTER_SKILLSET_ABILITY_COUNT = 4,
    SKILLSET_ABILITY_LIST_COUNT = 24,
};

typedef enum skillset_ability_filter {
    SKILLSET_ABILITY_FILTER_ACTION = 1,
    SKILLSET_ABILITY_FILTER_REACTION = 2,
    SKILLSET_ABILITY_FILTER_MOVEMENT = 4,
    SKILLSET_ABILITY_FILTER_SUPPORT = 8,
    SKILLSET_ABILITY_FILTER_ALL = 0xf,
} skillset_ability_filter_e;

/* The random R/S/M selector uses a different legacy bit assignment from the
 * skillset-list filter: support and movement are reversed. */
typedef enum rsm_ability_filter {
    RSM_ABILITY_FILTER_REACTION = 2,
    RSM_ABILITY_FILTER_SUPPORT = 4,
    RSM_ABILITY_FILTER_MOVEMENT = 8,
} rsm_ability_filter_e;

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

typedef enum ability_type {
    ABILITY_TYPE_DEFAULT = 0,
    ABILITY_TYPE_ITEM = 1,
    ABILITY_TYPE_THROW = 2,
    ABILITY_TYPE_JUMP = 3,
    ABILITY_TYPE_CHARGE = 4,
    ABILITY_TYPE_MATH = 5,
    ABILITY_TYPE_REACTION = 6,
    ABILITY_TYPE_SUPPORT = 7,
    ABILITY_TYPE_MOVEMENT = 8,
} ability_type_e;

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

/* Reaction behavior flags indexed by ability ID through the biased linker
 * alias g_main_reaction_behavior_flags_by_ability_id. */
typedef enum battle_reaction_behavior_flag {
    BATTLE_REACTION_BEHAVIOR_FLAG_TRIGGER_ABILITY = 0x01,
    BATTLE_REACTION_BEHAVIOR_FLAG_ABILITY = 0x02,
    BATTLE_REACTION_BEHAVIOR_FLAG_TARGET_SELF = 0x04,
    BATTLE_REACTION_BEHAVIOR_FLAG_TARGET_ATTACKER = 0x08,
    BATTLE_REACTION_BEHAVIOR_FLAG_PASSIVE = 0x10,
    BATTLE_REACTION_BEHAVIOR_FLAG_ACTIVE = 0x20,
} battle_reaction_behavior_flag_e;

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

enum {
    ITEM_ID_MASK = 0xff,
    ITEM_ID_COUNT = 0x100,
    ENTD_EQUIPMENT_RANDOM = 0xfe,
    ITEM_TYPE_FILTER_ANY = 0xff,
};

/* The four equippable-type bytes use MSB-first bit order for item types
 * 0x00..0x1f (the Job Data table at 0x800610b8). */
#define EQUIPPABLE_ITEM_TYPE_BYTE_INDEX(type) ((type) >> 3)
#define EQUIPPABLE_ITEM_TYPE_BYTE_MASK(type)  (0x80 >> ((type) & 7))

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

/*
 * WORLD.BIN tutorial command stream. Commands below 0x10 are one-byte shifts;
 * the rest are ID (1) followed by the listed parameters: 0x10 message id (2) +
 * y (1), 0x11 message id (2), 0x13 x, y, size, width mod, height mod (1 each),
 * 0x15 time (1).
 */
enum {
    WORLD_TUTORIAL_CMD_SHIFT_LIMIT = 0x10,
    WORLD_TUTORIAL_CMD_DISPLAY_MESSAGE = 0x10,
    WORLD_TUTORIAL_CMD_CHANGE_DIALOG = 0x11,
    WORLD_TUTORIAL_CMD_WAIT_FOR_MESSAGE = 0x12,
    WORLD_TUTORIAL_CMD_DRAW_CIRCLE = 0x13,
    WORLD_TUTORIAL_CMD_WAIT = 0x14,
    WORLD_TUTORIAL_CMD_WAIT_TIME = 0x15,
    WORLD_TUTORIAL_CMD_WAIT_FOR_MESSAGE_2 = 0x16,
    WORLD_TUTORIAL_CMD_END = 0x17,
};

/* Behavior flags in the four secondary bytes of a default ability record.
 * Only meanings confirmed by reconstructed target consumers are named here. */
typedef enum ability_secondary_flags_1 {
    ABILITY_SECONDARY_FLAG_1_CANNOT_TARGET_SELF = 0x01,
    ABILITY_SECONDARY_FLAG_1_AUTO = 0x02,
    ABILITY_SECONDARY_FLAG_1_WEAPON_STRIKE = 0x04,
    ABILITY_SECONDARY_FLAG_1_WEAPON_RANGE = 0x20,
} ability_secondary_flags_1_e;

typedef enum ability_secondary_flags_2 {
    ABILITY_SECONDARY_FLAG_2_CANNOT_HIT_CASTER = 0x01,
    ABILITY_SECONDARY_FLAG_2_RANDOM_FIRE = 0x08,
    ABILITY_SECONDARY_FLAG_2_CAN_TARGET_ENEMIES = 0x40,
    ABILITY_SECONDARY_FLAG_2_CAN_TARGET_ALLIES = 0x80,
} ability_secondary_flags_2_e;

typedef enum ability_secondary_flags_3 {
    ABILITY_SECONDARY_FLAG_3_SPELL_QUOTE = 0x02,
    ABILITY_SECONDARY_FLAG_3_PERSEVERE = 0x04,
    ABILITY_SECONDARY_FLAG_3_BLOCKED_BY_GOLEM = 0x08,
    ABILITY_SECONDARY_FLAG_3_AFFECTED_BY_SILENCE = 0x20,
    ABILITY_SECONDARY_FLAG_3_CALCULATOR_ELIGIBLE = 0x40,
    ABILITY_SECONDARY_FLAG_3_REFLECTABLE = 0x80,
} ability_secondary_flags_3_e;

typedef enum ability_secondary_flags_4 {
    ABILITY_SECONDARY_FLAG_4_EVADEABLE = 0x02,
    ABILITY_SECONDARY_FLAG_4_REQUIRES_MATERIA_BLADE = 0x04,
    ABILITY_SECONDARY_FLAG_4_REQUIRES_SWORD = 0x08,
    ABILITY_SECONDARY_FLAG_4_BLADE_GRASP_ELIGIBLE = 0x10,
    ABILITY_SECONDARY_FLAG_4_DIRECT_TARGETING = 0x20,
    ABILITY_SECONDARY_FLAG_4_COUNTER_MAGIC = 0x40,
    ABILITY_SECONDARY_FLAG_4_COUNTER_FLOOD = 0x80,
} ability_secondary_flags_4_e;

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

extern party_data_t g_main_party_data[PARTY_ROSTER_SLOT_COUNT];

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

/*
 * WORLD menu unit record, compacted after roster and menu-specific filtering.
 * Records at 0x801c8638 have stride 0x128; roster_slot preserves their roster
 * identity. The builder at 0x801210e8 and sorter at 0x80121c60 produce the
 * ordered g_world_formation_unit_pointers list, not a roster-indexed array.
 * The byte fields from 0x70 on mirror the party_data_t layout above. Magical
 * shield/accessory evade are provisionally placed at 0x50/0x52, after the
 * physical fields at 0x40/0x42, to follow the halfword sequence. Other bytes
 * stay as padding.
 */
typedef struct world_formation_unit {
    s16 level;        /* 0x000 */
    s16 guest_marker; /* 0x002; 2 for guest slots */
    u8 _pad04[4];
    s16 experience;  /* 0x008 */
    s16 party_index; /* 0x00a; portrait */
    s16 hp;          /* 0x00c */
    u8 _pad0e[2];
    s16 max_hp; /* 0x010 */
    s16 mp;     /* 0x012 */
    u8 _pad14[2];
    s16 max_mp; /* 0x016 */
    s16 ct;     /* 0x018 */
    u8 _pad1a[2];
    s16 max_ct; /* 0x01c; always 100 (world_formation_build_unit_record) */
    u8 _pad1e[4];
    u8 name_index[2]; /* 0x022; s16 formation index; starts the 14-byte display snapshot at WORLD 0x80114bc8 */
    s16 job_id;       /* 0x024; "Current Job" */
    s16 brave;        /* 0x026 */
    s16 faith;        /* 0x028 */
    s16 zodiac;       /* 0x02a; battle_stats_t birthday >> 12 (world_formation_build_unit_record) */
    s16 roster_slot;  /* 0x02c; "Roster Slot Index" */
    u8 _pad2e[2];
    s16 move;                     /* 0x030 */
    s16 speed;                    /* 0x032 */
    s16 jump;                     /* 0x034 */
    s16 right_weapon_power;       /* 0x036 */
    s16 left_weapon_power;        /* 0x038 */
    s16 right_weapon_evade;       /* 0x03a */
    s16 left_weapon_evade;        /* 0x03c */
    s16 uses_monster_skillset;    /* 0x03e; primary skillset is in the monster range 0xb0..0xdf */
    s16 two_handed;               /* 0x040; world_item_check_two_hands_for_weapons result (WORLD 0x80115198) */
    s16 physical_attack;          /* 0x042 */
    s16 physical_class_evade;     /* 0x044 */
    s16 physical_shield_evade;    /* 0x046 */
    s16 physical_accessory_evade; /* 0x048 */
    u8 _pad4a[2];
    s16 magical_attack;          /* 0x04c */
    s16 magical_class_evade;     /* 0x04e */
    s16 magical_shield_evade;    /* 0x050 */
    s16 magical_accessory_evade; /* 0x052 */
    u16 equipment[5];            /* 0x054; right hand, left hand, head, body, accessory */
    s16 primary_skillset;        /* 0x05e */
    s16 secondary_skillset;      /* 0x060 */
    s16 reaction_ability;        /* 0x062 */
    s16 support_ability;         /* 0x064 */
    s16 movement_ability;        /* 0x066 */
    u8 _pad68[8];
    u8 gender_flags; /* 0x070; unit_flags_e (fft/unit_flags.h) */
    u8 _pad71;
    u8 sprite_set;               /* 0x072; "Sprite Set ID": party_data_t.sprite_set / character identity */
    u8 equippable_item_types[4]; /* 0x073; bit per item type, read by can_unit_equip_item_id */
    u8 unlocked_jobs[3];         /* 0x077 */
    u8 learned_abilities[0x39];  /* 0x07a through 0x0b2 */
    u8 job_levels[0x0a];         /* 0x0b3 */
    u8 _padbd;
    u16 job_points[0x14];       /* 0x0be; per-job JP */
    u16 total_job_points[0x14]; /* 0x0e6; per-job total JP */
    u8 name[0x10];              /* 0x10e; "unit_t Nickname" */
    u8 graphic_variant;         /* 0x11e; added to formation graphic entry 0x3d when sprite_set == 0x82
                                   (world_formation_build_unit_graphic_entry) */
    u8 proposition_status;      /* 0x11f; "Proposition Byte" */
    u16 birthday;               /* 0x120; battle_stats_t birthday & 0x1ff; egg readers split
                                   it into nibbles */
    u16 egg_color; /* 0x122; party_data_t 0xd2; the formation sprite drawer (0x8011751c) loads it as a halfword palette
                      index */
    u8 support_sets[2]; /* 0x124; with 0x126-0x127, a 4-byte copy of battle_stats_t.support_abilities */
    u8 support_sets_3;  /* 0x126; "unit_t Support Sets #3", SUPPORT_SET_3_* (fft/job.h) */
    u8 _pad127;
} world_formation_unit_t;

typedef char world_formation_unit_t_size_must_be_0x128[(sizeof(world_formation_unit_t) == 0x128) ? 1 : -1];

/* Slot-indexed view of the five ability halfwords at 0x5e (primary skillset,
 * secondary skillset, reaction, support, movement).  The WORLD ability-slot
 * menu walks them by slot number; the bytes are the named fields above. */
typedef struct world_formation_unit_ability_slots {
    u8 _pad00[0x5e];
    u16 ability_slots[5]; /* 0x05e */
} world_formation_unit_ability_slots_t;

typedef char world_formation_unit_ability_slots_offset_must_be_0x5e
    [((unsigned long)&((world_formation_unit_ability_slots_t*)0)->ability_slots == 0x5e) ? 1 : -1];

extern u8 g_main_item_poach_table[48][2];

/* Linker alias biased so a reaction ability ID can index the 32-byte table at
 * 0x8005ebd0 directly; only IDs 0x1a6..0x1c5 are valid. */
extern const u8 g_main_reaction_behavior_flags_by_ability_id[];

/* Signed count of accepted records; pointer entry 20 is a separate extra
 * record at 0x801c9d58, installed by 0x80121fac–0x80121fb8. */
extern s16 g_world_formation_record_count; /* 0x801c8634 */

/* Menu callers cache the builder's return here; rebuilding the record list
 * does not itself refresh this byte. */
extern u8 g_world_formation_unit_count;
extern world_formation_unit_t* g_world_formation_unit_pointers[];

/* Current encounter, NULL while loading, or (entd_encounter_t *)-1 when
 * no encounter is selected. */
entd_encounter_t* main_entd_get_encounter(void);

extern accessory_data_t g_main_item_accessory_data[];
extern armor_data_t g_main_item_helm_armor_secondary_data[];
extern item_data_t g_main_item_primary_data[];
extern shield_data_t g_main_item_shield_data[];

#endif
