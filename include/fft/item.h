#ifndef ITEM_H
#define ITEM_H

/* Items: their types, ids and records, the party's inventory, and the item lists, sort orders, stat previews, icons and
 * rearrange mode of the item screens that EQUIP and WORLD share. */

#include "fft/gfx.h"
#include "fft/job.h"
#include "fft/menu.h"
#include "fft/status.h"
#include "fft/unit.h"
#include "fft/unit_panel.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* item types */
/* Item categories stored in item_data_t.type and in the four-byte job/unit
 * equippable-type set. A name is the type word that ends the help text of the
 * type's items ("Ninja sword", "Hammer", "Dictionary", "Spear", "Stick",
 * "Clothes", "Gauntlet", "Mantle", "Women's ribbon"; INSTRUMENT is "Mus inst"
 * there) or, for the two thrown types, the name of the Throw ability that
 * g_ability_throw_item_types maps to it. */
typedef enum item_type {
    ITEM_TYPE_UNARMED = 0,
    ITEM_TYPE_KNIFE = 1,
    ITEM_TYPE_NINJA_SWORD = 2,
    ITEM_TYPE_SWORD = 3,
    ITEM_TYPE_KNIGHT_SWORD = 4,
    ITEM_TYPE_KATANA = 5,
    ITEM_TYPE_AXE = 6,
    ITEM_TYPE_ROD = 7,
    ITEM_TYPE_STAFF = 8,
    ITEM_TYPE_HAMMER = 9,
    ITEM_TYPE_GUN = 10,
    ITEM_TYPE_CROSSBOW = 11,
    ITEM_TYPE_BOW = 12,
    ITEM_TYPE_INSTRUMENT = 13,
    ITEM_TYPE_DICTIONARY = 14,
    ITEM_TYPE_SPEAR = 15,
    ITEM_TYPE_STICK = 16,
    ITEM_TYPE_BAG = 17,
    ITEM_TYPE_CLOTH = 18,
    ITEM_TYPE_SHIELD = 19,
    ITEM_TYPE_HELMET = 20,
    ITEM_TYPE_HAT = 21,
    ITEM_TYPE_RIBBON = 22,
    ITEM_TYPE_ARMOR = 23,
    ITEM_TYPE_CLOTHES = 24,
    ITEM_TYPE_ROBE = 25,
    ITEM_TYPE_SHOES = 26,
    ITEM_TYPE_GAUNTLET = 27,
    ITEM_TYPE_RING = 28,
    ITEM_TYPE_ARMLET = 29,
    ITEM_TYPE_MANTLE = 30,
    ITEM_TYPE_PERFUME = 31,
    ITEM_TYPE_SHURIKEN = 32,
    ITEM_TYPE_BALL = 33,
    ITEM_TYPE_CONSUMABLE = 34,
    ITEM_TYPE_COUNT = 35
} item_type_e;

/* Bits of item_data_t.type_flags. */
typedef enum item_type_flag {
    ITEM_TYPE_FLAG_UNREMOVABLE = 0x01,
    ITEM_TYPE_FLAG_RARE = 0x02,
    ITEM_TYPE_FLAG_UNKNOWN_04 = 0x04,
    ITEM_TYPE_FLAG_ACCESSORY = 0x08,
    ITEM_TYPE_FLAG_BODY_ARMOR = 0x10,
    ITEM_TYPE_FLAG_HEADGEAR = 0x20,
    ITEM_TYPE_FLAG_SHIELD = 0x40,
    ITEM_TYPE_FLAG_WEAPON = 0x80
} item_type_flag_e;

/* The four equippable-type bytes use MSB-first bit order for item types
 * 0x00..0x1f (job_data_t.equipment_categories). */
#define EQUIPPABLE_ITEM_TYPE_BYTE_INDEX(type) ((type) >> 3)
#define EQUIPPABLE_ITEM_TYPE_BYTE_MASK(type)  (0x80 >> ((type) & 7))

/* item ids */
enum { ITEM_ID_MASK = 0xff, ITEM_ID_COUNT = 256, ITEM_TYPE_FILTER_ANY = 255 };

/* Item ids index the 256 rows of g_item_primary_data. A name is the
 * item's name in the game's text; the second Javelin and the second
 * Escutcheon, which repeat a name and are never sold, end in _RARE. Item 0 is
 * Nothing, the unarmed item record. Rows 0xfe and 0xff otherwise contain zero
 * data but retain ITEM_TYPE_FLAG_RARE; ENTD uses them for Random
 * (ENTD_VALUE_RANDOM) and None, while the random selector also uses 0xff as an
 * Any-type filter. */
typedef enum item_id {
    ITEM_ID_NOTHING = 0,
    /* ITEM_TYPE_KNIFE */
    ITEM_ID_DAGGER = 1,
    ITEM_ID_MYTHRIL_KNIFE = 2,
    ITEM_ID_BLIND_KNIFE = 3,
    ITEM_ID_MAGE_MASHER = 4,
    ITEM_ID_PLATINA_DAGGER = 5,
    ITEM_ID_MAIN_GAUCHE = 6,
    ITEM_ID_ORICHALCUM = 7,
    ITEM_ID_ASSASSIN_DAGGER = 8,
    ITEM_ID_AIR_KNIFE = 9,
    ITEM_ID_ZORLIN_SHAPE = 10,
    /* ITEM_TYPE_NINJA_SWORD */
    ITEM_ID_HIDDEN_KNIFE = 11,
    ITEM_ID_NINJA_KNIFE = 12,
    ITEM_ID_SHORT_EDGE = 13,
    ITEM_ID_NINJA_EDGE = 14,
    ITEM_ID_SPELL_EDGE = 15,
    ITEM_ID_SASUKE_KNIFE = 16,
    ITEM_ID_IGA_KNIFE = 17,
    ITEM_ID_KOGA_KNIFE = 18,
    /* ITEM_TYPE_SWORD */
    ITEM_ID_BROAD_SWORD = 19,
    ITEM_ID_LONG_SWORD = 20,
    ITEM_ID_IRON_SWORD = 21,
    ITEM_ID_MYTHRIL_SWORD = 22,
    ITEM_ID_BLOOD_SWORD = 23,
    ITEM_ID_CORAL_SWORD = 24,
    ITEM_ID_ANCIENT_SWORD = 25,
    ITEM_ID_SLEEP_SWORD = 26,
    ITEM_ID_PLATINUM_SWORD = 27,
    ITEM_ID_DIAMOND_SWORD = 28,
    ITEM_ID_ICE_BRAND = 29,
    ITEM_ID_RUNE_BLADE = 30,
    ITEM_ID_NAGRAROCK = 31,
    ITEM_ID_MATERIA_BLADE = 32,
    /* ITEM_TYPE_KNIGHT_SWORD */
    ITEM_ID_DEFENDER = 33,
    ITEM_ID_SAVE_THE_QUEEN = 34,
    ITEM_ID_EXCALIBUR = 35,
    ITEM_ID_RAGNAROK = 36,
    ITEM_ID_CHAOS_BLADE = 37,
    /* ITEM_TYPE_KATANA */
    ITEM_ID_ASURA_KNIFE = 38,
    ITEM_ID_KOUTETSU_KNIFE = 39,
    ITEM_ID_BIZEN_BOAT = 40,
    ITEM_ID_MURASAME = 41,
    ITEM_ID_HEAVENS_CLOUD = 42,
    ITEM_ID_KIYOMORI = 43,
    ITEM_ID_MURAMASA = 44,
    ITEM_ID_KIKUICHIMOJI = 45,
    ITEM_ID_MASAMUNE = 46,
    ITEM_ID_CHIRIJIRADEN = 47,
    /* ITEM_TYPE_AXE */
    ITEM_ID_BATTLE_AXE = 48,
    ITEM_ID_GIANT_AXE = 49,
    ITEM_ID_SLASHER = 50,
    /* ITEM_TYPE_ROD */
    ITEM_ID_ROD = 51,
    ITEM_ID_THUNDER_ROD = 52,
    ITEM_ID_FLAME_ROD = 53,
    ITEM_ID_ICE_ROD = 54,
    ITEM_ID_POISON_ROD = 55,
    ITEM_ID_WIZARD_ROD = 56,
    ITEM_ID_DRAGON_ROD = 57,
    ITEM_ID_FAITH_ROD = 58,
    /* ITEM_TYPE_STAFF */
    ITEM_ID_OAK_STAFF = 59,
    ITEM_ID_WHITE_STAFF = 60,
    ITEM_ID_HEALING_STAFF = 61,
    ITEM_ID_RAINBOW_STAFF = 62,
    ITEM_ID_WIZARD_STAFF = 63,
    ITEM_ID_GOLD_STAFF = 64,
    ITEM_ID_MACE_OF_ZEUS = 65,
    ITEM_ID_SAGE_STAFF = 66,
    /* ITEM_TYPE_HAMMER */
    ITEM_ID_FLAIL = 67,
    ITEM_ID_FLAME_WHIP = 68,
    ITEM_ID_MORNING_STAR = 69,
    ITEM_ID_SCORPION_TAIL = 70,
    /* ITEM_TYPE_GUN */
    ITEM_ID_ROMANDA_GUN = 71,
    ITEM_ID_MYTHRIL_GUN = 72,
    ITEM_ID_STONE_GUN = 73,
    ITEM_ID_BLAZE_GUN = 74,
    ITEM_ID_GLACIER_GUN = 75,
    ITEM_ID_BLAST_GUN = 76,
    /* ITEM_TYPE_CROSSBOW */
    ITEM_ID_BOW_GUN = 77,
    ITEM_ID_NIGHT_KILLER = 78,
    ITEM_ID_CROSS_BOW = 79,
    ITEM_ID_POISON_BOW = 80,
    ITEM_ID_HUNTING_BOW = 81,
    ITEM_ID_GASTRAFITIS = 82,
    /* ITEM_TYPE_BOW */
    ITEM_ID_LONG_BOW = 83,
    ITEM_ID_SILVER_BOW = 84,
    ITEM_ID_ICE_BOW = 85,
    ITEM_ID_LIGHTNING_BOW = 86,
    ITEM_ID_WINDSLASH_BOW = 87,
    ITEM_ID_MYTHRIL_BOW = 88,
    ITEM_ID_ULTIMUS_BOW = 89,
    ITEM_ID_YOICHI_BOW = 90,
    ITEM_ID_PERSEUS_BOW = 91,
    /* ITEM_TYPE_INSTRUMENT */
    ITEM_ID_RAMIA_HARP = 92,
    ITEM_ID_BLOODY_STRINGS = 93,
    ITEM_ID_FAIRY_HARP = 94,
    /* ITEM_TYPE_DICTIONARY */
    ITEM_ID_BATTLE_DICT = 95,
    ITEM_ID_MONSTER_DICT = 96,
    ITEM_ID_PAPYRUS_PLATE = 97,
    ITEM_ID_MADLEMGEN = 98,
    /* ITEM_TYPE_SPEAR */
    ITEM_ID_JAVELIN = 99,
    ITEM_ID_SPEAR = 100,
    ITEM_ID_MYTHRIL_SPEAR = 101,
    ITEM_ID_PARTISAN = 102,
    ITEM_ID_OBERISK = 103,
    ITEM_ID_HOLY_LANCE = 104,
    ITEM_ID_DRAGON_WHISKER = 105,
    ITEM_ID_JAVELIN_RARE = 106, /* the second "Javelin" of the name text, never sold */
    /* ITEM_TYPE_STICK */
    ITEM_ID_CYPRESS_ROD = 107,
    ITEM_ID_BATTLE_BAMBOO = 108,
    ITEM_ID_MUSK_ROD = 109,
    ITEM_ID_IRON_FAN = 110,
    ITEM_ID_GOKUU_ROD = 111,
    ITEM_ID_IVORY_ROD = 112,
    ITEM_ID_OCTAGON_ROD = 113,
    ITEM_ID_WHALE_WHISKER = 114,
    /* ITEM_TYPE_BAG */
    ITEM_ID_C_BAG = 115,
    ITEM_ID_FS_BAG = 116,
    ITEM_ID_P_BAG = 117,
    ITEM_ID_H_BAG = 118,
    /* ITEM_TYPE_CLOTH */
    ITEM_ID_PERSIA = 119,
    ITEM_ID_CASHMERE = 120,
    ITEM_ID_RYOZAN_SILK = 121,
    /* ITEM_TYPE_SHURIKEN */
    ITEM_ID_SHURIKEN = 122,
    ITEM_ID_MAGIC_SHURIKEN = 123,
    ITEM_ID_YAGYU_DARKNESS = 124,
    /* ITEM_TYPE_BALL */
    ITEM_ID_FIRE_BALL = 125,
    ITEM_ID_WATER_BALL = 126,
    ITEM_ID_LIGHTNING_BALL = 127,
    /* ITEM_TYPE_SHIELD */
    ITEM_ID_ESCUTCHEON = 128,
    ITEM_ID_BUCKLER = 129,
    ITEM_ID_BRONZE_SHIELD = 130,
    ITEM_ID_ROUND_SHIELD = 131,
    ITEM_ID_MYTHRIL_SHIELD = 132,
    ITEM_ID_GOLD_SHIELD = 133,
    ITEM_ID_ICE_SHIELD = 134,
    ITEM_ID_FLAME_SHIELD = 135,
    ITEM_ID_AEGIS_SHIELD = 136,
    ITEM_ID_DIAMOND_SHIELD = 137,
    ITEM_ID_PLATINA_SHIELD = 138,
    ITEM_ID_CRYSTAL_SHIELD = 139,
    ITEM_ID_GENJI_SHIELD = 140,
    ITEM_ID_KAISER_PLATE = 141,
    ITEM_ID_VENETIAN_SHIELD = 142,
    ITEM_ID_ESCUTCHEON_RARE = 143, /* the second "Escutcheon" of the name text, never sold */
    /* ITEM_TYPE_HELMET */
    ITEM_ID_LEATHER_HELMET = 144,
    ITEM_ID_BRONZE_HELMET = 145,
    ITEM_ID_IRON_HELMET = 146,
    ITEM_ID_BARBUTA = 147,
    ITEM_ID_MYTHRIL_HELMET = 148,
    ITEM_ID_GOLD_HELMET = 149,
    ITEM_ID_CROSS_HELMET = 150,
    ITEM_ID_DIAMOND_HELMET = 151,
    ITEM_ID_PLATINA_HELMET = 152,
    ITEM_ID_CIRCLET = 153,
    ITEM_ID_CRYSTAL_HELMET = 154,
    ITEM_ID_GENJI_HELMET = 155,
    ITEM_ID_GRAND_HELMET = 156,
    /* ITEM_TYPE_HAT */
    ITEM_ID_LEATHER_HAT = 157,
    ITEM_ID_FEATHER_HAT = 158,
    ITEM_ID_RED_HOOD = 159,
    ITEM_ID_HEADGEAR = 160,
    ITEM_ID_TRIANGLE_HAT = 161,
    ITEM_ID_GREEN_BERET = 162,
    ITEM_ID_TWIST_HEADBAND = 163,
    ITEM_ID_HOLY_MITER = 164,
    ITEM_ID_BLACK_HOOD = 165,
    ITEM_ID_GOLDEN_HAIRPIN = 166,
    ITEM_ID_FLASH_HAT = 167,
    ITEM_ID_THIEF_HAT = 168,
    /* ITEM_TYPE_RIBBON */
    ITEM_ID_CACHUSHA = 169,
    ITEM_ID_BARETTE = 170,
    ITEM_ID_RIBBON = 171,
    /* ITEM_TYPE_ARMOR */
    ITEM_ID_LEATHER_ARMOR = 172,
    ITEM_ID_LINEN_CUIRASS = 173,
    ITEM_ID_BRONZE_ARMOR = 174,
    ITEM_ID_CHAIN_MAIL = 175,
    ITEM_ID_MYTHRIL_ARMOR = 176,
    ITEM_ID_PLATE_MAIL = 177,
    ITEM_ID_GOLD_ARMOR = 178,
    ITEM_ID_DIAMOND_ARMOR = 179,
    ITEM_ID_PLATINA_ARMOR = 180,
    ITEM_ID_CARABINI_MAIL = 181,
    ITEM_ID_CRYSTAL_MAIL = 182,
    ITEM_ID_GENJI_ARMOR = 183,
    ITEM_ID_REFLECT_MAIL = 184,
    ITEM_ID_MAXIMILLIAN = 185,
    /* ITEM_TYPE_CLOTHES */
    ITEM_ID_CLOTHES = 186,
    ITEM_ID_LEATHER_OUTFIT = 187,
    ITEM_ID_LEATHER_VEST = 188,
    ITEM_ID_CHAIN_VEST = 189,
    ITEM_ID_MYTHRIL_VEST = 190,
    ITEM_ID_ADAMAN_VEST = 191,
    ITEM_ID_WIZARD_OUTFIT = 192,
    ITEM_ID_BRIGANDINE = 193,
    ITEM_ID_JUDO_OUTFIT = 194,
    ITEM_ID_POWER_SLEEVE = 195,
    ITEM_ID_EARTH_CLOTHES = 196,
    ITEM_ID_SECRET_CLOTHES = 197,
    ITEM_ID_BLACK_COSTUME = 198,
    ITEM_ID_RUBBER_COSTUME = 199,
    /* ITEM_TYPE_ROBE */
    ITEM_ID_LINEN_ROBE = 200,
    ITEM_ID_SILK_ROBE = 201,
    ITEM_ID_WIZARD_ROBE = 202,
    ITEM_ID_CHAMELEON_ROBE = 203,
    ITEM_ID_WHITE_ROBE = 204,
    ITEM_ID_BLACK_ROBE = 205,
    ITEM_ID_LIGHT_ROBE = 206,
    ITEM_ID_ROBE_OF_LORDS = 207,
    /* ITEM_TYPE_SHOES */
    ITEM_ID_BATTLE_BOOTS = 208,
    ITEM_ID_SPIKE_SHOES = 209,
    ITEM_ID_GERMINAS_BOOTS = 210,
    ITEM_ID_RUBBER_SHOES = 211,
    ITEM_ID_FEATHER_BOOTS = 212,
    ITEM_ID_SPRINT_SHOES = 213,
    ITEM_ID_RED_SHOES = 214,
    /* ITEM_TYPE_GAUNTLET */
    ITEM_ID_POWER_WRIST = 215,
    ITEM_ID_GENJI_GAUNTLET = 216,
    ITEM_ID_MAGIC_GAUNTLET = 217,
    ITEM_ID_BRACER = 218,
    /* ITEM_TYPE_RING */
    ITEM_ID_REFLECT_RING = 219,
    ITEM_ID_DEFENSE_RING = 220,
    ITEM_ID_MAGIC_RING = 221,
    ITEM_ID_CURSED_RING = 222,
    ITEM_ID_ANGEL_RING = 223,
    /* ITEM_TYPE_ARMLET */
    ITEM_ID_DIAMOND_ARMLET = 224,
    ITEM_ID_JADE_ARMLET = 225,
    ITEM_ID_108_GEMS = 226,
    ITEM_ID_N_KAI_ARMLET = 227,
    ITEM_ID_DEFENSE_ARMLET = 228,
    /* ITEM_TYPE_MANTLE */
    ITEM_ID_SMALL_MANTLE = 229,
    ITEM_ID_LEATHER_MANTLE = 230,
    ITEM_ID_WIZARD_MANTLE = 231,
    ITEM_ID_ELF_MANTLE = 232,
    ITEM_ID_DRACULA_MANTLE = 233,
    ITEM_ID_FEATHER_MANTLE = 234,
    ITEM_ID_VANISH_MANTLE = 235,
    /* ITEM_TYPE_PERFUME */
    ITEM_ID_CHANTAGE = 236,
    ITEM_ID_CHERCHE = 237,
    ITEM_ID_SETIEMSON = 238,
    ITEM_ID_SALTY_RAGE = 239,
    /* ITEM_TYPE_CONSUMABLE */
    ITEM_ID_POTION = 240,
    ITEM_ID_HI_POTION = 241,
    ITEM_ID_X_POTION = 242,
    ITEM_ID_ETHER = 243,
    ITEM_ID_HI_ETHER = 244,
    ITEM_ID_ELIXIR = 245,
    ITEM_ID_ANTIDOTE = 246,
    ITEM_ID_EYE_DROP = 247,
    ITEM_ID_ECHO_GRASS = 248,
    ITEM_ID_MAIDENS_KISS = 249,
    ITEM_ID_SOFT = 250,
    ITEM_ID_HOLY_WATER = 251,
    ITEM_ID_REMEDY = 252,
    ITEM_ID_PHOENIX_DOWN = 253,
    ITEM_ID_NONE = 255
} item_id_e;

/* Item table and equipment-category boundaries, in the retail item table's order: throwable weapons start with
 * Shuriken, followed by shields, headgear, body armor, accessories, and consumables. */
enum {
    ITEM_ID_THROWABLE_FIRST = ITEM_ID_SHURIKEN,
    ITEM_ID_SHIELD_FIRST = ITEM_ID_ESCUTCHEON,
    ITEM_ID_HEADGEAR_FIRST = ITEM_ID_LEATHER_HELMET,
    ITEM_ID_BODY_ARMOR_FIRST = ITEM_ID_LEATHER_ARMOR,
    ITEM_ID_ACCESSORY_FIRST = ITEM_ID_BATTLE_BOOTS,
    ITEM_ID_CONSUMABLE_FIRST = ITEM_ID_POTION,
    ITEM_ID_END = 254 /* one past the last item; ENTD's Random and None follow */
};

/* formation_unit_t.equipment 16-bit values carry the item id in the low ten bits and flags above (bit 14 is tested
 * by shop_fitting_buy and the job wheel); the WORLD item tables are indexed by the masked id (shop_get_item_price,
 * item_get_type). */
enum {
    ITEM_ENTRY_ID_MASK = 0x3ff,
    ITEM_ENTRY_UNEQUIPPABLE = 0x4000, /* item list entries: listed, but the unit cannot equip the item */
    ITEM_LIST_END = 0xff,             /* the item id that ends an item list */
    SHOP_FITTING_ITEM = 0x8000,       /* equipment 16-bit values: an unpaid shop item tried on in the fitting room */
    /* The equipment values of a WORLD stat detail: the top two bits pick the name's text palette, and
     * HIGHLIGHT, the same bit as SHOP_ITEM, draws it in the highlight palette. */
    FORMATION_PALETTE_MASK = 0xc000,
    FORMATION_HIGHLIGHT = 0x8000
};

/* item records */
/* Primary item record, one per item id. */
typedef struct item_data {
    u8 palette;
    u8 sprite_id;
    u8 required_level;
    u8 type_flags;
    u8 secondary_data_id;
    u8 type;
    u8 _unused_06;
    u8 attributes;
    u16 price;
    u8 shop_availability;
    u8 _padding_0b; /* tail padding to 2-byte alignment */
} item_data_t;
typedef char item_data_size_must_be_12[(sizeof(item_data_t) == 12) ? 1 : -1];

/* Bits of weapon_data_t.flags. */
typedef enum weapon_flag {
    WEAPON_FLAG_FORCED_TWO_HANDS = 0x01,
    WEAPON_FLAG_THROWABLE = 0x02,
    WEAPON_FLAG_TWO_HANDS_COMPATIBLE = 0x04,
    WEAPON_FLAG_TWO_SWORDS_COMPATIBLE = 0x08,
    WEAPON_FLAG_ARC = 0x10,
    WEAPON_FLAG_DIRECT = 0x20,
    WEAPON_FLAG_LUNGING = 0x40,
    WEAPON_FLAG_STRIKING = 0x80
} weapon_flag_e;

/* Weapon record selected by item_data_t.secondary_data_id. Offsets are fixed
 * by unit_set_equipment_stats and unit_calculate_encounter_equipment. */
typedef struct weapon_data {
    u8 range;      /* 0x00 */
    u8 flags;      /* 0x01; weapon_flag_e */
    u8 formula;    /* 0x02; ability_formula_e */
    u8 _unused_03; /* 0x03 */
    u8 power;      /* 0x04; copied to unit_stats_t equipment-stat slots 0x3c/0x3d */
    u8 evade;      /* 0x05; copied to unit_stats_t 0x3e/0x3f */
    u8 element;    /* 0x06 */
    u8 proc_id;    /* 0x07; status-infliction or spell ID copied to the active ability */
} weapon_data_t;
typedef char weapon_data_size_must_be_8[(sizeof(weapon_data_t) == 8) ? 1 : -1];

/* Three-byte item ability records, g_item_secondary_data. Item abilities
 * select these records directly; formula handlers use z and ability setup uses
 * the status infliction id. */
typedef struct item_secondary_data {
    u8 formula;
    u8 z;
    u8 status_infliction_id;
} item_secondary_data_t;
typedef char item_secondary_data_size_must_be_3[(sizeof(item_secondary_data_t) == 3) ? 1 : -1];

/* Shield and accessory records share unsigned physical/magic evade percentages,
 * indexed by item_data_t.secondary_data_id. */
typedef struct item_evade {
    u8 physical_evade; /* 0x00 */
    u8 magic_evade;    /* 0x01 */
} item_evade_t;
typedef char item_evade_size_must_be_2[(sizeof(item_evade_t) == 2) ? 1 : -1];

typedef struct armor_data {
    u8 hp_bonus; /* 0x00; added to unit_stats_t.max_hp */
    u8 mp_bonus; /* 0x01; added to unit_stats_t.max_mp */
} armor_data_t;

/* 25-byte item Attribute record (g_item_attributes). MAIN applies these
 * bonuses to battle units; WORLD's item_collect_stat_deltas reads the same
 * first five bytes for equipment previews. */
typedef struct item_attribute {
    u8 attributes[UNIT_ATTRIBUTE_COUNT]; /* 0x00; unit_attribute_index_e */
    u8 move;                             /* 0x03 */
    u8 jump;                             /* 0x04 */
    u8 innate[STATUS_BYTE_COUNT];        /* 0x05 */
    u8 immunity[STATUS_BYTE_COUNT];      /* 0x0a */
    u8 starting[STATUS_BYTE_COUNT];      /* 0x0f */
    u8 elemental_affinity[5];            /* 0x14; elemental_affinity_index_e */
} item_attribute_t;
typedef char item_attribute_size_must_be_25[(sizeof(item_attribute_t) == 25) ? 1 : -1];

/* Poach yields the rare item 31 times in 256. */
enum { ITEM_POACH_RARE_ITEM_CHANCE = 31 };

/* Common and rare Fur Shop items obtained by poaching one monster job. */
typedef struct item_poach {
    u8 common_item;
    u8 rare_item;
} item_poach_t;
typedef char item_poach_size_must_be_2[(sizeof(item_poach_t) == 2) ? 1 : -1];

extern item_data_t g_item_primary_data[ITEM_ID_COUNT];
extern item_secondary_data_t g_item_secondary_data[ITEM_ID_END - ITEM_ID_CONSUMABLE_FIRST];
extern weapon_data_t g_item_weapon_data[ITEM_ID_SHIELD_FIRST];
extern item_evade_t
    g_item_shield_data[ITEM_ID_HEADGEAR_FIRST - ITEM_ID_SHIELD_FIRST]; /* unit right/left shield evade slots */
extern armor_data_t g_item_helm_armor_secondary_data[ITEM_ID_ACCESSORY_FIRST - ITEM_ID_HEADGEAR_FIRST];
extern item_evade_t
    g_item_accessory_data[ITEM_ID_CONSUMABLE_FIRST - ITEM_ID_ACCESSORY_FIRST]; /* unit accessory evade slots */
extern item_attribute_t g_item_attributes[];
extern u8 g_item_ability_item_ids[14];
extern item_poach_t g_item_poach_table[JOB_ID_MONSTER_LAST - JOB_ID_MONSTER_FIRST + 1];

/* &g_item_primary_data[0].secondary_data_id viewed as a stride-12 byte
 * array. unit_set_equipment_stats reads the secondary id through its own
 * %lo(g_item_primary_data+4) address while the type_flags test uses a
 * record pointer; reading both through item_data_t folds them onto one base
 * register and misses the byte-exact match. */
extern u8 g_item_secondary_data_ids[];

item_data_t* item_get_data(s32 item_id);

/* inventory */
extern u8 g_item_quantities[ITEM_ID_COUNT];
extern u8 g_item_poached_quantities[ITEM_ID_COUNT];

/* The first 0x40 bytes are Move-Find-Item flags; the second half is not yet
 * identified. Both halves are initialized and serialized as one block. */
extern u8 g_item_location_flags[0x80];

void item_init_new_game_inventory(void);
s32 item_get_total_quantity(u8 item_id, s32 include_battle);
s32 item_give(unit_stats_t* unit, u8 item);
s32 item_decrement_party_quantity(unit_stats_t* unit, s32 item_id, s32 spend);
void item_build_inventory_totals(void);

/* EQUIP and WORLD each build their own bodies of the first two. */
s32 item_get_total_count(s16 item_id);
s32 item_get_equipped_count(s32 item_entry);
s32 item_adjust_inventory_count(s16 item_entry, s32 delta);

/* item lists */
/* Broad item categories shared by the WORLD and EQUIP menu classifiers.
 * Throwable weapons and consumables both use OTHER because neither maps to a
 * normal equipment slot category. */
typedef enum item_menu_category {
    ITEM_MENU_CATEGORY_WEAPON = 0,
    ITEM_MENU_CATEGORY_SHIELD = 1,
    ITEM_MENU_CATEGORY_HEADGEAR = 2,
    ITEM_MENU_CATEGORY_BODY_ARMOR = 3,
    ITEM_MENU_CATEGORY_ACCESSORY = 4,
    ITEM_MENU_CATEGORY_OTHER = 5
} item_menu_category_e;

/* The category argument of formation_item_build_list,
 * shop_build_stock_list and equipment_build_item_list. 0 to 4
 * select what fits that equipment slot. The item menus pass their tab + 1 for
 * the first four tabs and OTHER for the fifth; the best-fit builders pass
 * WEAPON or SHIELD for the left hand. */
typedef enum item_list_filter {
    ITEM_LIST_FILTER_HAND = 0, /* weapons and shields; 1, the left hand, selects the same */
    ITEM_LIST_FILTER_HEADGEAR = 2,
    ITEM_LIST_FILTER_BODY_ARMOR = 3,
    ITEM_LIST_FILTER_ACCESSORY = 4,
    ITEM_LIST_FILTER_WEAPON = 5,
    ITEM_LIST_FILTER_SHIELD = 6,
    ITEM_LIST_FILTER_OTHER = 7
} item_list_filter_e;

/* The item menus' tabs (g_menu_selected_slot there, g_shop_item_category): tabs 0 to 3 (hand, head,
 * body, accessory) list filter tab + 1, and this last tab lists ITEM_LIST_FILTER_OTHER. */
enum { ITEM_MENU_TAB_OTHER = 4 };

/* The last argument of the same functions: the equip check against the unit. */
typedef enum item_list_mode {
    ITEM_LIST_MODE_ALL = 0,                    /* no check */
    ITEM_LIST_MODE_MARK_UNEQUIPPABLE = 1,      /* 0x4000 on entries the unit cannot equip; taken as
                                                  EQUIPPABLE unless Show Unequippable Items is on */
    ITEM_LIST_MODE_EQUIPPABLE = 2,             /* entries the unit cannot equip are left out */
    ITEM_LIST_MODE_EQUIPPABLE_IN_INVENTORY = 3 /* formation_item_build_list only: as
                                                   EQUIPPABLE, counting no equipped copies */
} item_list_mode_e;

/* State of the item list screen (g_formation_item_menu_mode in the formation
 * screen, g_equipment_substate in EQUIP). Each state's handler returns the
 * next state. */
typedef enum menu_item_list_state {
    MENU_ITEM_LIST_STATE_CLOSE = -1,
    MENU_ITEM_LIST_STATE_BROWSE = 0,
    MENU_ITEM_LIST_STATE_REARRANGE = 1,
    MENU_ITEM_LIST_STATE_ACTION = 2, /* action strip for the selected item */
    MENU_ITEM_LIST_STATE_SORT_ORDER = 5,
    MENU_ITEM_LIST_STATE_EQUIPPED_BY = 6 /* WORLD only: the units equipping the selected item */
} menu_item_list_state_e;

extern s16 g_item_list_entries[];
extern u8 g_item_weapon_page_order[0x8a];
extern u8 g_item_helmet_page_order[0x1d];
extern u8 g_item_armor_page_order[0x25];
extern u8 g_item_accessory_page_order[0x21];
extern u8 g_item_page_order[0x15];

/* The owned items of each list category (0 weapons and shields, then headgear, body armor, accessories and
 * other: the menu category minus one), byte lists ending in a terminator. */
extern u8* g_item_category_lists[];

/* The equipment slot or list category the item screens show (menu_equipment_slot_e). */
extern u8 g_menu_selected_slot;

s32 item_get_type(s32 item_id);

/* Throwable weapons and consumables share the fallback category. */
item_menu_category_e item_get_menu_category(s16 item_id);

/* Reconciles a category list with what the party owns: newly owned items go first. */
void item_rebuild_category_list(s32 category);
s32 item_sort_list_by_category_order(s32 category_index, s16* list);

/* Packs a -1 terminated s16 item list into the category's byte list. */
void item_store_category_list(s32 category_index, const s16* source);

/* sort orders */
/* A key in the list that item_sort_id_list (EQUIP and WORLD each build their own body) sorts by
 * (g_item_type_order_lists); -1 ends the list. TYPE sorts ascending, the others descending. */
typedef enum item_sort_key {
    ITEM_SORT_KEY_TYPE = 0,
    ITEM_SORT_KEY_PRICE = 1,
    ITEM_SORT_KEY_RANKING = 2, /* item_get_ranking_value */
    ITEM_SORT_KEY_PHYSICAL_EVADE = 3,
    ITEM_SORT_KEY_MAGIC_EVADE = 4,
    ITEM_SORT_KEY_HELD_COUNT = 5 /* inventory plus equipped copies */
} item_sort_key_e;

/* Independent sort-key lists in their resident storage layout. Unit List and
 * Formation have their own key domains; the five item categories use item_sort_key_e.
 * Each list ends in -1. The item rows save only their live lengths. */
typedef struct menu_sort_orders {
    s8 unit_list[12];
    s8 formation[8];
    s8 weapons_and_shields[8];
    s8 headgear[8];
    s8 body_armor[8];
    s8 accessories[8];
    s8 other_items[8];
} menu_sort_orders_t;

extern menu_sort_orders_t g_menu_sort_orders;

/* Views of the individual menu sort orders for the WORLD save-image
 * copies (save_build_image, save_restore_image),
 * whose code the member spelling changes. */
extern s8 g_formation_sort_order[8];
extern u8 g_weapons_and_shields_sort_order[7];
extern u8 g_headgear_sort_order[5];
extern u8 g_body_armor_sort_order[5];
extern u8 g_accessories_sort_order[7];
extern u8 g_other_items_sort_order[5];

/* Packed sort-key orders ending in -1. WORLD slot 0 points to Formation's
 * formation order (formation_sort_key_e); EQUIP slot 0 is null. Slots 1..5
 * in both programs point to the five item-category orders (item_sort_key_e). */
extern s8* g_item_type_order_lists[];

void item_init_order_tables(void);
s32 item_get_ranking_value(s32 item_id);
void item_sort_id_list(s32 mode, s16* list);

/* Store and load a category's sort key order as -1 terminated halfwords. */
void item_store_type_order(s32 list_index, const s16* source);
void item_load_type_order(s32 list_index, s16* destination);

/* stat previews */
extern struct unit_panel_record g_item_preview_stat_detail;

/* EQUIP and WORLD each build their own body. */
void item_collect_stat_deltas(s16 raw_item, unit_panel_summary_t* summary, unit_panel_record_t* detail, s32 alternate);

/* icons and the item panel */
/* The texture origin of each item type's 12x12 icon, read by item_get_type_icon_rect. */
extern uv8_t g_item_type_icon_origins[];

/* Status image rectangles, indexed by status id, that item_draw_status_list draws in the item detail panel's status
 * column. */
extern RECT g_unit_panel_status_display_image_rects[];

/* Item and item-type icons as menu_icon_source_t, for the icon row callbacks; item_get_type_icon_rect gives the
 * texture cell of an item type's 12x12 icon. */
void item_get_type_icon_rect(s32 item_type, menu_icon_source_t* output);
void item_build_icon_source(menu_icon_source_t* output, u32 item_id);
menu_icon_source_t* item_build_type_icon_source(s32 index);

/* A weapon's hand marks on the item panel: two-handed, or the Two Hands and Two Swords marks it allows. */
void item_draw_weapon_hand_icons(s16 weapon_id);

/* The item detail panel's status column. */
void item_draw_status_list(s16 item_id);

/* item action menu */
extern menu_entry_t g_item_action_menu;

/* rearranging */
/* The item screens' rearrange mode: pick an item up and move it within its category list. */
extern s8 g_item_rearrange_initialized;
extern s16 g_item_rearrange_picked_cursor;
extern s16 g_item_rearrange_preview_cursor;
extern u16 g_item_rearrange_picked_scroll;
extern u8 g_item_rearrange_anim_frame;

s32 item_run_rearrange_mode(void);

#endif
