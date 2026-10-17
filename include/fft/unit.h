#ifndef UNIT_H
#define UNIT_H

/* Unit records: the battle stats record every program builds and the types it shares with the ENTD entries
 * (unit_entd.h) and party records (unit_party.h), unit height and motion records, and MAIN's unit graphics queue,
 * unit generation and stat building. */

#include "fft/job.h"
#include "fft/status.h"
#include "psx/libgte.h"
#include "psx/types.h"

struct entd_unit;
struct unit_stats;

/* BATTLE's unit record, defined in unit_runtime.h; headers that only pass units around need just this name. */
typedef struct unit unit_t;

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

/* unit_name_id_t.fields.name_class selects the unit-name text page. */
typedef enum party_name_class {
    PARTY_NAME_CLASS_SPECIAL = 0,
    PARTY_NAME_CLASS_GENERIC_MALE = 1,
    PARTY_NAME_CLASS_GENERIC_FEMALE = 2,
    PARTY_NAME_CLASS_GENERIC_MONSTER = 3
} party_name_class_e;

/* Bytes of a unit name in the game's text encoding. */
enum { UNIT_NAME_SIZE = 16 };

/* unit flags */
/* Shared by ENTD definitions, persistent party records, and runtime battle
 * units. The byte combines persistence controls with unit-kind flags. */
typedef enum unit_flags {
    UNIT_FLAG_SAVE_FORMATION = 0x01,
    UNIT_FLAG_EGG = 0x04, /* a roster egg; on ENTD battle units it hides the numeric stats and scales HP/MP by 10 */
    UNIT_FLAG_LOAD_FORMATION = 0x08,
    UNIT_FLAG_JOIN_AFTER_EVENT = 0x10,
    UNIT_FLAG_MONSTER = 0x20,
    UNIT_FLAG_FEMALE = 0x40,
    UNIT_FLAG_MALE = 0x80
} unit_flags_e;

/* abilities */
/* Equipped or ENTD-selected reaction/support/movement ability IDs. */
typedef struct unit_abilities {
    u16 reaction;
    u16 support;
    u16 movement;
} unit_abilities_t;
typedef char unit_abilities_size_must_be_6[(sizeof(unit_abilities_t) == 6) ? 1 : -1];

/* Active reaction abilities occupy four MSB-first bytes in unit_stats_t. */
typedef enum unit_reaction_set_1 {
    UNIT_REACTION_SET_1_BRAVE_UP = 0x01,
    UNIT_REACTION_SET_1_REGENERATOR = 0x02,
    UNIT_REACTION_SET_1_DRAGON_SPIRIT = 0x04,
    UNIT_REACTION_SET_1_CAUTION = 0x08,
    UNIT_REACTION_SET_1_SUNKEN_STATE = 0x10,
    UNIT_REACTION_SET_1_SPEED_SAVE = 0x20,
    UNIT_REACTION_SET_1_MA_SAVE = 0x40,
    UNIT_REACTION_SET_1_PA_SAVE = 0x80
} unit_reaction_set_1_e;

typedef enum unit_reaction_set_2 {
    UNIT_REACTION_SET_2_COUNTER_FLOOD = 0x01,
    UNIT_REACTION_SET_2_COUNTER_TACKLE = 0x02,
    UNIT_REACTION_SET_2_COUNTER_MAGIC = 0x04,
    UNIT_REACTION_SET_2_MEATBONE_SLASH = 0x08,
    UNIT_REACTION_SET_2_CRITICAL_QUICK = 0x10,
    UNIT_REACTION_SET_2_MP_RESTORE = 0x20,
    UNIT_REACTION_SET_2_HP_RESTORE = 0x40,
    UNIT_REACTION_SET_2_FAITH_UP = 0x80
} unit_reaction_set_2_e;

typedef enum unit_reaction_set_3 {
    UNIT_REACTION_SET_3_MP_SWITCH = 0x01,
    UNIT_REACTION_SET_3_DISTRIBUTE = 0x02,
    /* 0x04 is present in the bitset but remains unidentified. */
    UNIT_REACTION_SET_3_COUNTER = 0x08,
    UNIT_REACTION_SET_3_AUTO_POTION = 0x10,
    UNIT_REACTION_SET_3_REFLECT = 0x20,
    UNIT_REACTION_SET_3_GILGAME_HEART = 0x40,
    UNIT_REACTION_SET_3_ABSORB_USED_MP = 0x80
} unit_reaction_set_3_e;

typedef enum unit_reaction_set_4 {
    UNIT_REACTION_SET_4_HAMEDO = 0x01,
    UNIT_REACTION_SET_4_ARROW_GUARD = 0x02,
    UNIT_REACTION_SET_4_BLADE_GRASP = 0x04,
    UNIT_REACTION_SET_4_CATCH = 0x08,
    UNIT_REACTION_SET_4_ABANDON = 0x10,
    UNIT_REACTION_SET_4_FINGER_GUARD = 0x20,
    UNIT_REACTION_SET_4_WEAPON_GUARD = 0x40,
    UNIT_REACTION_SET_4_DAMAGE_SPLIT = 0x80
} unit_reaction_set_4_e;

/* Active support abilities occupy four MSB-first bitsets in unit_stats_t
 * at +0x08f..+0x092. WORLD copies them to formation_unit_t at +0x124;
 * these masks name the shared byte domain, not ability IDs. */
typedef enum unit_support_set_1 {
    UNIT_SUPPORT_SET_1_EQUIP_GUN = 0x01,
    UNIT_SUPPORT_SET_1_EQUIP_AXE = 0x02,
    UNIT_SUPPORT_SET_1_EQUIP_SPEAR = 0x04,
    UNIT_SUPPORT_SET_1_EQUIP_CROSSBOW = 0x08,
    UNIT_SUPPORT_SET_1_EQUIP_KATANA = 0x10,
    UNIT_SUPPORT_SET_1_EQUIP_SWORD = 0x20,
    UNIT_SUPPORT_SET_1_EQUIP_SHIELD = 0x40,
    UNIT_SUPPORT_SET_1_EQUIP_ARMOR = 0x80
} unit_support_set_1_e;

typedef enum unit_support_set_2 {
    UNIT_SUPPORT_SET_2_CONCENTRATE = 0x01,
    UNIT_SUPPORT_SET_2_MAGIC_DEFENSE_UP = 0x02,
    UNIT_SUPPORT_SET_2_MAGIC_ATTACK_UP = 0x04,
    UNIT_SUPPORT_SET_2_DEFENSE_UP = 0x08,
    UNIT_SUPPORT_SET_2_ATTACK_UP = 0x10,
    UNIT_SUPPORT_SET_2_GAINED_EXP_UP = 0x20,
    UNIT_SUPPORT_SET_2_GAINED_JP_UP = 0x40,
    UNIT_SUPPORT_SET_2_HALF_MP = 0x80
} unit_support_set_2_e;

typedef enum unit_support_set_3 {
    UNIT_SUPPORT_SET_3_TWO_SWORDS = 0x01,
    UNIT_SUPPORT_SET_3_TWO_HANDS = 0x02,
    UNIT_SUPPORT_SET_3_MAINTENANCE = 0x04,
    UNIT_SUPPORT_SET_3_THROW_ITEM = 0x08,
    UNIT_SUPPORT_SET_3_MONSTER_TALK = 0x10,
    UNIT_SUPPORT_SET_3_MARTIAL_ARTS = 0x20,
    UNIT_SUPPORT_SET_3_SECRET_HUNT = 0x40,
    UNIT_SUPPORT_SET_3_TRAIN = 0x80
} unit_support_set_3_e;

typedef enum unit_support_set_4 {
    UNIT_SUPPORT_SET_4_NON_CHARGE = 0x04,
    UNIT_SUPPORT_SET_4_SHORT_CHARGE = 0x08,
    UNIT_SUPPORT_SET_4_EQUIP_CHANGE = 0x20,
    UNIT_SUPPORT_SET_4_DEFEND = 0x40,
    UNIT_SUPPORT_SET_4_MONSTER_SKILL = 0x80
} unit_support_set_4_e;

/* Active movement abilities occupy three MSB-first bytes in unit_stats_t.
 * MAIN reads these when calculating move, jump and Float behavior. */
typedef enum unit_movement_set_1 {
    UNIT_MOVEMENT_SET_1_MOVE_HP_UP = 0x01,
    UNIT_MOVEMENT_SET_1_IGNORE_HEIGHT = 0x02,
    UNIT_MOVEMENT_SET_1_JUMP_PLUS_3 = 0x04,
    UNIT_MOVEMENT_SET_1_JUMP_PLUS_2 = 0x08,
    UNIT_MOVEMENT_SET_1_JUMP_PLUS_1 = 0x10,
    UNIT_MOVEMENT_SET_1_MOVE_PLUS_3 = 0x20,
    UNIT_MOVEMENT_SET_1_MOVE_PLUS_2 = 0x40,
    UNIT_MOVEMENT_SET_1_MOVE_PLUS_1 = 0x80
} unit_movement_set_1_e;

typedef enum unit_movement_set_2 {
    UNIT_MOVEMENT_SET_2_ANY_GROUND = 0x01,
    UNIT_MOVEMENT_SET_2_ANY_WEATHER = 0x02,
    UNIT_MOVEMENT_SET_2_TELEPORT_2 = 0x04,
    UNIT_MOVEMENT_SET_2_TELEPORT = 0x08,
    UNIT_MOVEMENT_SET_2_CANNOT_ENTER_WATER = 0x10,
    UNIT_MOVEMENT_SET_2_MOVE_GET_JP = 0x20,
    UNIT_MOVEMENT_SET_2_MOVE_GET_EXP = 0x40,
    UNIT_MOVEMENT_SET_2_MOVE_MP_UP = 0x80
} unit_movement_set_2_e;

typedef enum unit_movement_set_3 {
    UNIT_MOVEMENT_SET_3_MOVE_FIND_ITEM = 0x01,
    UNIT_MOVEMENT_SET_3_SILENT_WALK = 0x02,
    UNIT_MOVEMENT_SET_3_FLY = 0x04,
    UNIT_MOVEMENT_SET_3_FLOAT = 0x08,
    UNIT_MOVEMENT_SET_3_MOVE_UNDERWATER = 0x10,
    UNIT_MOVEMENT_SET_3_MOVE_ON_LAVA = 0x20,
    UNIT_MOVEMENT_SET_3_WALK_ON_WATER = 0x40, /* ability 0x1f7; wades one level deep */
    UNIT_MOVEMENT_SET_3_MOVE_IN_WATER = 0x80  /* ability 0x1f6; stands on the water surface */
} unit_movement_set_3_e;

/* equipment */
/* The four bytes of unit_stats_t.equipment_categories: one bit per equippable
 * item type, MSB first (EQUIPPABLE_ITEM_TYPE_BYTE_MASK). Only the bits the code
 * tests are named. */
typedef enum unit_equipment_categories_1 {
    UNIT_EQUIPMENT_SET_1_AXE = 0x02,
    UNIT_EQUIPMENT_SET_1_KATANA = 0x04,
    UNIT_EQUIPMENT_SET_1_SWORD = 0x10
} unit_equipment_categories_1_e;

typedef enum unit_equipment_categories_2 {
    UNIT_EQUIPMENT_SET_2_SPEAR = 0x01,
    UNIT_EQUIPMENT_SET_2_CROSSBOW = 0x10,
    UNIT_EQUIPMENT_SET_2_GUN = 0x20
} unit_equipment_categories_2_e;

typedef enum unit_equipment_categories_3 {
    UNIT_EQUIPMENT_SET_3_ARMOR = 0x01,
    UNIT_EQUIPMENT_SET_3_RIBBON = 0x02,
    UNIT_EQUIPMENT_SET_3_HELM = 0x08,
    UNIT_EQUIPMENT_SET_3_SHIELD = 0x10,
    UNIT_EQUIPMENT_SET_3_BAG = 0x40
} unit_equipment_categories_3_e;

typedef enum unit_equipment_categories_4 { UNIT_EQUIPMENT_SET_4_PERFUME = 0x01 } unit_equipment_categories_4_e;

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
    UNIT_EQUIPMENT_SLOT_COUNT = 7
} unit_equipment_slot_e;

s32 unit_is_two_handing_weapon(const s16* slots, s32 two_hands_support);

/* birthday and zodiac */
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
    ZODIAC_SIGN_SERPENTARIUS = 12
} zodiac_sign_e;

enum { ZODIAC_SIGN_ORDINARY_COUNT = 12, ZODIAC_SIGN_COUNT = 13 };

/* Packed birthday and zodiac at unit offset 0x008. Field updates preserve
 * the three reserved bits between the day-of-year and zodiac. */
typedef struct unit_birthday {
    u16 birthday : 9;
    u16 _unused_01 : 3; /* reserved bits 9..11 */
    u16 zodiac : 4;
} unit_birthday_t;
typedef char unit_birthday_size_must_be_2[(sizeof(unit_birthday_t) == 2) ? 1 : -1];

/* stats */
/* Character progression; excess EXP is discarded on a level-up. */
enum { UNIT_LEVEL_MAX = 99, UNIT_LEVEL_UP_EXPERIENCE = 100, UNIT_LEVEL_EXPERIENCE_MAX = UNIT_LEVEL_UP_EXPERIENCE - 1 };

/* Calculated stat caps; eggs use the full u16 range for HP and MP. */
enum { UNIT_HP_MP_MAX = 999, UNIT_ATTACK_STAT_MAX = 99, UNIT_SPEED_MAX = 50, UNIT_BRAVE_FAITH_MAX = 100 };

/* Five 24-bit raw values serialized in HP, MP, Speed, PA, MA order. */
typedef enum unit_raw_stat_index {
    UNIT_RAW_STAT_HP = 0,
    UNIT_RAW_STAT_MP = 1,
    UNIT_RAW_STAT_SPEED = 2,
    UNIT_RAW_STAT_PHYSICAL_ATTACK = 3,
    UNIT_RAW_STAT_MAGIC_ATTACK = 4,
    UNIT_RAW_STAT_COUNT = 5
} unit_raw_stat_index_e;

enum {
    UNIT_RAW_STAT_SERIALIZED_BYTES = 3,
    UNIT_RAW_STAT_DATA_BYTE_COUNT = UNIT_RAW_STAT_COUNT * UNIT_RAW_STAT_SERIALIZED_BYTES
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

/* Physical attack, magic attack and speed, in the order units and item
 * attribute records store them. */
typedef enum unit_attribute_index {
    UNIT_ATTRIBUTE_PHYSICAL_ATTACK = 0,
    UNIT_ATTRIBUTE_MAGIC_ATTACK = 1,
    UNIT_ATTRIBUTE_SPEED = 2,
    UNIT_ATTRIBUTE_COUNT = 3
} unit_attribute_index_e;

/* CT points needed for a unit turn; speed supplies points each clocktick. */
enum { UNIT_TURN_CT_THRESHOLD = 100 };

/* unit_stats_t.equipment_stats (Battle Stats 0x3c-0x46), filled by
 * unit_set_equipment_stats: each hand's weapon power and weapon evade
 * (which battle counts only under Weapon Guard), then the physical evades of
 * the accessory, each hand's shield and the class, then the magic evades of
 * the accessory and each hand's shield. RIGHT_SHIELD comes from
 * UNIT_EQUIPMENT_SLOT_RIGHT_HAND_SHIELD. */
typedef enum unit_equipment_stat_index {
    UNIT_EQUIPMENT_STAT_RIGHT_WEAPON_POWER = 0,
    UNIT_EQUIPMENT_STAT_LEFT_WEAPON_POWER = 1,
    UNIT_EQUIPMENT_STAT_RIGHT_WEAPON_EVADE = 2,
    UNIT_EQUIPMENT_STAT_LEFT_WEAPON_EVADE = 3,
    UNIT_EQUIPMENT_STAT_ACCESSORY_PHYSICAL_EVADE = 4,
    UNIT_EQUIPMENT_STAT_RIGHT_SHIELD_PHYSICAL_EVADE = 5,
    UNIT_EQUIPMENT_STAT_LEFT_SHIELD_PHYSICAL_EVADE = 6,
    UNIT_EQUIPMENT_STAT_CLASS_PHYSICAL_EVADE = 7,
    UNIT_EQUIPMENT_STAT_ACCESSORY_MAGIC_EVADE = 8,
    UNIT_EQUIPMENT_STAT_RIGHT_SHIELD_MAGIC_EVADE = 9,
    UNIT_EQUIPMENT_STAT_LEFT_SHIELD_MAGIC_EVADE = 10,
    UNIT_EQUIPMENT_STAT_COUNT = 11
} unit_equipment_stat_index_e;

/* position */
/* Suppress random spell quotes for the matching primary or secondary skillset. */
typedef enum unit_spell_quote_flags {
    UNIT_SPELL_QUOTE_SUPPRESS_SECONDARY = 0x01,
    UNIT_SPELL_QUOTE_SUPPRESS_PRIMARY = 0x02
} unit_spell_quote_flags_e;

/* Runtime Y, facing and tile-layer fields. ENTD entries use the narrower
 * facing field of entd_unit_position_t; initialization copies each group. */
typedef struct unit_position {
    u16 y : 8;
    u16 facing : 4;
    u16 spell_quote_skillsets : 2; /* unit_spell_quote_flags_e */
    u16 stepping_stone : 1;
    u16 higher_elevation : 1;
} unit_position_t;
typedef char unit_position_size_must_be_2[(sizeof(unit_position_t) == 2) ? 1 : -1];

/* deployment */
/* unit_stats_t.formation_index values that name no party roster slot. */
typedef enum unit_formation_index {
    UNIT_FORMATION_INDEX_GENERATED_UNIT = 254,
    UNIT_FORMATION_INDEX_NONE = 255
} unit_formation_index_e;

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

extern unit_deployment_coordinates_t* g_unit_deployed_coords;

/* teams and control */
/* Team bits select one of four independently tracked battle teams. */
enum { UNIT_TEAM_COUNT = 4, UNIT_TEAM_SHIFT = 4, UNIT_TEAM_INDEX_MASK = 0x03 };

/* Battle Stats 0x05 and unit runtime record 0x13d share this byte. */
typedef enum unit_team_flags {
    UNIT_TEAM_MASK = 0x30,
    UNIT_TEAM_OR_PLAYER_CONTROL_MASK = 0x38,
    UNIT_TEAM_BLUE = 0x00,
    UNIT_TEAM_RED = 0x10,
    UNIT_TEAM_GREEN = 0x20,
    UNIT_TEAM_LIGHT_BLUE = 0x30,
    UNIT_TEAM_IMPORTANCE_MASK = 0x03, /* 0..3; 3 decides the battle outcome and cannot be knocked back */
    UNIT_TEAM_FLAG_IMMORTAL = 0x04,
    UNIT_TEAM_FLAG_PLAYER_CONTROLLED = 0x08,
    /* ENTD presence, read when the units are loaded: neither bit leaves the
     * unit out, RANDOMLY_PRESENT alone leaves it out half the time, and both
     * load its graphics for a later AddUnit. */
    UNIT_TEAM_FLAG_RANDOMLY_PRESENT = 0x40,
    UNIT_TEAM_FLAG_ALWAYS_PRESENT = 0x80,
    UNIT_TEAM_PRESENCE_MASK = 0xc0
} unit_team_flags_e;

/* unit_stats_t.auto_battle_setting and the AI copies of it. The BATTLE and
 * WORLD auto-battle menus store it and the AI reads it. */
typedef enum ai_autobattle_mode {
    AI_AUTOBATTLE_MANUAL = 0, /* the player gives the orders */
    AI_AUTOBATTLE_FIGHT_FOR_LIFE = 12,
    AI_AUTOBATTLE_PROTECT_ALLIES = 14,
    AI_AUTOBATTLE_FADING_LIFE = 16,
    AI_AUTOBATTLE_RETREAT = 17
} ai_autobattle_mode_e;

/* actions */
/* The 20-byte action command shared by unit stats, AI candidates and menus.
 * Targeting reads coordinates as signed or unsigned 16-bit values. */
typedef struct action_command {
    u8 actor_id;               /* 0x00 */
    u8 skillset_id;            /* 0x01 */
    s16 ability_id;            /* 0x02 */
    s16 calculator_type;       /* 0x04 */
    s16 calculator_multiplier; /* 0x06 */
    u8 item_id;                /* 0x08 */
    u8 _unused_09;             /* 0x09 */
    u8 targeting_type;         /* 0x0a; target_type_e */
    u8 target_id;              /* 0x0b */
    SVECTOR target_position;   /* 0x0c; tile x, elevation, y; padding is unread */
} action_command_t;
typedef char action_command_size_must_be_20[(sizeof(action_command_t) == 20) ? 1 : -1];

/* action_data_t.ct_change values that are not an amount. Any other value is a gain when
 * bit 7 is set and a loss otherwise. */
enum {
    ACTION_CT_CHANGE_ZERO = 0x7f, /* CT set to 0 */
    ACTION_CT_CHANGE_QUICK = 0xff /* CT filled */
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
     * reaction_try_arrow_guard, reaction_try_catch,
     * formula_force_attack_miss). */
    s16 attack_accuracy; /* 0x2a */
} action_data_t;
typedef char action_data_size_must_be_44[(sizeof(action_data_t) == 44) ? 1 : -1];

/* action_data_t.miss_display_flags, loaded from the ability's AI flags. */
typedef enum action_miss_display_flags {
    ACTION_MISS_DISPLAY_TEAM_MASK = 0x03,      /* the AI target-allies / target-enemies pair */
    ACTION_MISS_DISPLAY_ALLIES_ONLY = 0x01,    /* the team bits of an ability that targets allies only */
    ACTION_MISS_DISPLAY_DODGE_ANIMATION = 0x80 /* the ability's evade-with-motion bit */
} action_miss_display_flags_e;

/* battle stats */
/* The battle simulation owns 21 unit records. Battle-slot search routines use
 * the first value beyond that array as their no-unit sentinel. */
enum {
    UNIT_RUNTIME_SLOT_COUNT = 16, /* simultaneously present units, including event ghosts */
    UNIT_SLOT_COUNT = 21,
    UNIT_ID_NONE = UNIT_SLOT_COUNT
};

/* Battle Stats 0x182 links the two simulation records in a mounted pair. */
typedef enum unit_mount_info_flags {
    UNIT_MOUNT_INFO_PARTNER_ID_MASK = 0x1f,
    UNIT_MOUNT_INFO_FLAG_MOUNT = 0x40,
    UNIT_MOUNT_INFO_FLAG_RIDER = 0x80,
    UNIT_MOUNT_INFO_ROLE_MASK = 0xc0
} unit_mount_info_flags_e;

/* Lifecycle states stored in Battle Stats 0x183. */
typedef enum unit_existence_state {
    UNIT_EXISTENCE_OFF_FIELD = 0, /* defined but not placed; AddUnit can use the slot */
    UNIT_EXISTENCE_ACTIVE = 1,
    UNIT_EXISTENCE_PENDING_REMOVAL = 2,
    UNIT_EXISTENCE_DISABLED = 128,
    UNIT_EXISTENCE_NONE = 255 /* empty slot */
} unit_existence_state_e;

/* unit_stats_t.equipped_flags: equipment that some abilities require. */
typedef enum unit_equipped_flags {
    UNIT_EQUIPPED_FLAG_MATERIA_BLADE = 0x04,
    UNIT_EQUIPPED_FLAG_SWORD = 0x08
} unit_equipped_flags_e;

/* active_battle_id marks absence independently of the persistent battle_id. */
enum { UNIT_ACTIVE_BATTLE_ID_NONE = 0xff };

/* Seven encounter AI bytes copied together into the unit record. */
enum { UNIT_AI_SETUP_BYTE_COUNT = 7 };
typedef union unit_ai_setup {
    struct {
        u8 target_x;           /* 0x00 */
        u8 target_y;           /* 0x01 */
        u8 flags;              /* 0x02; ai_decision_flags_e; high bit selects the stay-near tile's upper level */
        u8 target_unit_id;     /* 0x03 */
        u8 targeting_flags_1;  /* 0x04; initial ai_unit_decision_t targeting flags */
        u8 targeting_flags_2;  /* 0x05; ai_targeting_flags_2_e */
        u8 initial_enemy_flag; /* 0x06 */
    } fields;
    u8 bytes[UNIT_AI_SETUP_BYTE_COUNT];
} unit_ai_setup_t;
typedef char unit_ai_setup_size_must_be_7[(sizeof(unit_ai_setup_t) == UNIT_AI_SETUP_BYTE_COUNT) ? 1 : -1];

/* Current and calculated unit attributes, also built for WORLD menus. BATTLE
 * keeps UNIT_SLOT_COUNT of them in g_unit_stats; a live
 * unit_t points to its stats through the member at 0x134. */
typedef struct unit_stats {
    u8 character_identity;                   /* 0x000 */
    u8 active_battle_id;                     /* 0x001; active battle slot, or 0xff; not an ENTD definition index */
    u8 formation_index;                      /* 0x002 */
    u8 job_id;                               /* 0x003 */
    u8 sprite_palette;                       /* 0x004 */
    u8 team_flags;                           /* 0x005 */
    u8 unit_flags;                           /* 0x006; mixed sex, monster, guest, and formation flags */
    u8 death_counter;                        /* 0x007 */
    unit_birthday_t birthday;                /* 0x008; zodiac is the high nibble */
    u16 innate_abilities[4];                 /* 0x00a */
    u8 primary_skillset;                     /* 0x012 */
    u8 secondary_skillset;                   /* 0x013 */
    unit_abilities_t abilities;              /* 0x014; reaction, support, movement */
    u8 equipment[UNIT_EQUIPMENT_SLOT_COUNT]; /* 0x01a; unit_equipment_slot_e */
    u8 experience;                           /* 0x021 */
    u8 level;                                /* 0x022 */
    u8 original_brave;                       /* 0x023 */
    u8 brave;                                /* 0x024 */
    u8 original_faith;                       /* 0x025 */
    u8 faith;                                /* 0x026 */
    u8 transparent_removal_flag;             /* 0x027 */
    u16 hp;                                  /* 0x028 */
    u16 max_hp;                              /* 0x02a */
    u16 mp;                                  /* 0x02c */
    u16 max_mp;                              /* 0x02e */
    u8 base_attributes[3];                   /* 0x030; unit_attribute_index_e */
    u8 equipment_attributes[3];              /* 0x033; unit_attribute_index_e */
    u8 attributes[3];                        /* 0x036; unit_attribute_index_e */
    u8 ct;                                   /* 0x039 */
    u8 move;                                 /* 0x03a */
    u8 jump;                                 /* 0x03b */
    u8 equipment_stats[UNIT_EQUIPMENT_STAT_COUNT];       /* 0x03c; unit_equipment_stat_index_e */
    u8 x;                                                /* 0x047 */
    unit_position_t position;                            /* 0x048 */
    u8 equipment_categories[4];                          /* 0x04a */
    status_sets_t status_sets;                           /* 0x04e; innate / immunity / current */
    u8 status_ct[STATUS_TIMED_COUNT];                    /* 0x05d */
    u8 elemental_affinity[5];                            /* 0x06d; elemental_affinity_index_e */
    unit_raw_stats_t raw_stats;                          /* 0x072 */
    job_stat_growth_t stat_growths[UNIT_RAW_STAT_COUNT]; /* 0x081; job_data_t.stat_growths */
    /* Active R/S/M flags: one contiguous 88-bit set indexed by ability ID. */
    struct {
        u8 reaction[4];        /* 0x00; unit_reaction_set_N_e */
        u8 support[4];         /* 0x04; unit_support_set_N_e */
        u8 movement[3];        /* 0x08; unit_movement_set_N_e */
    } ability_flags;           /* 0x08b */
    unit_career_data_t career; /* 0x096; unlocked jobs, learned abilities, levels and JP */
    /* unit_store_names copies 16/16/8/8 bytes to these four
     * destinations. All names use the game's encoding. */
    u8 name[UNIT_NAME_SIZE];         /* 0x12c */
    u8 job_name[16];                 /* 0x13c */
    u8 primary_skillset_name[8];     /* 0x14c */
    u8 secondary_skillset_name[8];   /* 0x154 */
    u8 ko_count;                     /* 0x15c */
    u8 charged_ability_ct;           /* 0x15d; clockticks until the charged action resolves; 0xff when not charging */
    u8 monster_graphic;              /* 0x15e; copied from job data and consumed by WORLD formation graphics */
    u8 spritesheet_id;               /* 0x15f */
    u8 job_portrait_palette;         /* 0x160 */
    u8 unit_id;                      /* 0x161 */
    u8 base_job_skillset;            /* 0x162 */
    u8 war_trophy;                   /* 0x163 */
    u8 bonus_money_modifier;         /* 0x164 */
    unit_ai_setup_t ai;              /* 0x165 */
    unit_name_id_t quote_name_id;    /* 0x16c */
    action_command_t action_command; /* 0x16e..0x181 */
    u8 mount_info;                   /* 0x182 */
    u8 existence;                    /* 0x183; unit_existence_state_e */
    u8 equipped_flags;               /* 0x184; unit_equipped_flags_e */
    u8 _unused_185;
    u8 has_turn;            /* 0x186 */
    u8 movement_taken;      /* 0x187 */
    u8 action_taken;        /* 0x188 */
    u8 ability_outcome;     /* 0x189 */
    u8 battle_id;           /* 0x18a; index in g_unit_stats */
    u8 ability_ct;          /* 0x18b */
    action_data_t action;   /* 0x18c through 0x1b7 */
    u8 auto_battle_setting; /* 0x1b8; ai_autobattle_mode_e */
    u8 auto_battle_target;  /* 0x1b9 */
    u8 initial_team_flags;  /* 0x1ba */
    u8 inflicted_status[5]; /* 0x1bb */
} unit_stats_t;
typedef char unit_stats_size_must_be_448[(sizeof(unit_stats_t) == 448) ? 1 : -1];

/* unit_stats_t.ability_outcome. */
typedef enum unit_ability_outcome {
    UNIT_ABILITY_OUTCOME_TARGETED = 0x01,     /* target of a primary action */
    UNIT_ABILITY_OUTCOME_AFFECTED = 0x02,     /* that action changed the unit */
    UNIT_ABILITY_OUTCOME_LEARN_CHECKED = 0x04 /* the learn-on-hit pass consumed AFFECTED */
} unit_ability_outcome_e;

/* unit heights (BATTLE) */
/* Seven result bytes; assigning elevation preserves the unassigned high bit. */
typedef struct unit_height_data {
    u8 x;
    u8 y;
    u8 elevation : 7;
    u8 _unused_02 : 1; /* not read or assigned */
    u8 unit_flags;
    u8 unit_height;
    u8 walking_height;
    u8 total_height;
} unit_height_data_t;
typedef char unit_height_data_size_must_be_7[sizeof(unit_height_data_t) == 7 ? 1 : -1];

/* unit display (BATTLE) */
/* Masks of the motion flags in the 32-bit view of unit_t.movement (move_destination_t),
 * whose high byte is runtime-unit +0x83. */
typedef enum unit_motion_flags {
    UNIT_MOTION_FLAG_SUPPRESS_FACING_UPDATE = 0x01000000,
    UNIT_MOTION_FLAG_SUPPRESS_SFX_AND_LANDING_EFFECTS = 0x02000000,
    UNIT_MOTION_FLAG_SUPPRESS_PALETTE_UPDATE = 0x04000000,
    UNIT_MOTION_FLAG_ALTERNATE_SHORT_HOP = 0x08000000,
    UNIT_MOTION_FLAG_FLOAT = 0x10000000,
    UNIT_MOTION_FLAG_FORCE_ANIMATION_UPDATE = 0x20000000 /* set with each stored animation request */
} unit_motion_flags_e;

/* action state (BATTLE) */
/* Tile coordinates stored in teleport paths and relocation reservations. */
typedef struct move_tile_coordinates {
    u8 x;
    u8 y;
    u8 elevation;
} move_tile_coordinates_t;
typedef char move_tile_coordinates_size_must_be_3[(sizeof(move_tile_coordinates_t) == 3) ? 1 : -1];

/* unit graphics queue */
/* One queued unit: where it stands, how it faces and which graphics it uses. */
typedef struct unit_graphics_load_descriptor {
    u8 map_x;
    u8 map_y;
    u8 map_level;
    u8 _padding_03; /* aligns facing */
    u16 facing;     /* a quarter turn per unit */
    s16 spritesheet_id;
    u16 palette_id;
    u16 runtime_id;
    unit_stats_t* stats;
    u32 flags; /* nonzero: the unit starts undrawn */
} unit_graphics_load_descriptor_t;
typedef char unit_graphics_load_descriptor_size_must_be_20[(sizeof(unit_graphics_load_descriptor_t) == 20) ? 1 : -1];

/* Resident unit-graphics queue, filled and consumed by the BATTLE loader. */
typedef struct unit_graphics_load_queue {
    s32 count;                                                            /* 0x00 */
    unit_graphics_load_descriptor_t descriptors[UNIT_RUNTIME_SLOT_COUNT]; /* 0x04 */
} unit_graphics_load_queue_t;
typedef char unit_graphics_load_queue_size_must_be_324[(sizeof(unit_graphics_load_queue_t) == 324) ? 1 : -1];

extern unit_graphics_load_queue_t g_unit_graphics_load_queue;

/* unit generation */
/* Base-stat and equipment generation profiles; these are not job ids or gender flags. */
typedef enum unit_generation_type {
    UNIT_TYPE_MALE = 0,
    UNIT_TYPE_FEMALE = 1,
    UNIT_TYPE_RAMZA = 2,
    UNIT_TYPE_MONSTER = 3,
    UNIT_TYPE_COUNT = 4
} unit_generation_type_e;

/* stat building and growth */
/* unit_init_from_encounter kinds. */
typedef enum unit_entd_init_kind {
    UNIT_ENTD_INIT_NEW = 0, /* reset the unit and build it from its ENTD entry */
    /* Rebuild a transformed unit's stats for its new job in place: no reset, no ENTD data and no
     * ENTD equipment. */
    UNIT_ENTD_INIT_TRANSFORM = 0x82
} unit_entd_init_kind_e;

s32 unit_init_from_encounter(unit_stats_t* unit, const struct entd_unit* entd, u32 use_world_text, s32 kind);
s32 unit_init_from_party_slot(unit_stats_t* unit, s32 slot, s32 use_world_text);
void unit_init_job_levels(const u16* unit_job_jp, u8* unit_job_levels);
void unit_init_status_and_rewards(unit_stats_t* unit, s32 clear_rewards);
void unit_init_position_and_rewards(unit_stats_t* unit, const struct entd_unit* entd);
void unit_learn_job_abilities(unit_stats_t* unit, s32 job_id, const struct entd_unit* entd);
void unit_reset_battle_state(unit_stats_t* unit);
void unit_refresh_stats_and_statuses(unit_stats_t* unit);
void unit_update_formation_stats(unit_stats_t* unit);
void unit_set_equipment_attributes(unit_stats_t* unit, s32 level_up_check);
void unit_set_equipment_stats(unit_stats_t* unit);
void unit_set_equippable_items(unit_stats_t* unit);
s32 unit_calculate_random_equipment(
    const unit_stats_t* unit, u8 item_type_flag, u8 weapon_flag_mask, u8 required_item_type);
void unit_calculate_move_jump(unit_stats_t* unit, s32 keep_hp_mp);
u8 unit_calculate_palette_spritesheet(unit_stats_t* unit, u8* palette);
u16 unit_calculate_rsm(const unit_stats_t* unit, u16 ability_id, s32 flags, const struct entd_unit* entd);
void unit_enable_rsm_flags(unit_stats_t* unit);
void unit_copy_last_ability_ct(unit_stats_t* unit);
s32 unit_check_level_up(unit_stats_t* unit);
void unit_apply_level_growth(unit_stats_t* unit, s32 level_down);
void unit_increase_casualty_counters(const unit_stats_t* unit);

/* menus and panels */

extern s16 g_unit_selected_index; /* the unit the unit-list screens have selected */

#endif
