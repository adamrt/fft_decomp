#ifndef DATA_ITEM_H
#define DATA_ITEM_H

/* Item data: item ids, types and the item, weapon, shield, armor, accessory and attribute records. */

#include "fft/data_status.h"
#include "psx/types.h"

/* item types */
/* Item categories stored in item_data_t.type and in the four-byte job/unit
 * equippable-type set. A name is the type word that ends the help text of the
 * type's items ("Ninja sword", "Hammer", "Dictionary", "Spear", "Stick",
 * "Clothes", "Gauntlet", "Mantle", "Women's ribbon"; INSTRUMENT is "Mus inst"
 * there) or, for the two thrown types, the name of the Throw ability that
 * g_main_throw_ability_item_types maps to it. */
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
    ITEM_TYPE_COUNT = 35,
} item_type_e;

/* item_data_t.type_flags. */
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

/* The four equippable-type bytes use MSB-first bit order for item types
 * 0x00..0x1f (job_data_t.equipment_categories). */
#define EQUIPPABLE_ITEM_TYPE_BYTE_INDEX(type) ((type) >> 3)

#define EQUIPPABLE_ITEM_TYPE_BYTE_MASK(type) (0x80 >> ((type) & 7))

/* item ids */
enum {
    ITEM_ID_MASK = 0xff,
    ITEM_ID_COUNT = 256,
    ITEM_TYPE_FILTER_ANY = 255,
};

/* Item ids index the 256 rows of g_main_item_primary_data. A name is the
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
    ITEM_ID_NONE = 255,
} item_id_e;

/* Item table and equipment-category boundaries. */
enum {
    ITEM_ID_THROWABLE_FIRST = ITEM_ID_SHURIKEN,
    ITEM_ID_SHIELD_FIRST = ITEM_ID_ESCUTCHEON,
    ITEM_ID_HEADGEAR_FIRST = ITEM_ID_LEATHER_HELMET,
    ITEM_ID_BODY_ARMOR_FIRST = ITEM_ID_LEATHER_ARMOR,
    ITEM_ID_ACCESSORY_FIRST = ITEM_ID_BATTLE_BOOTS,
    ITEM_ID_CONSUMABLE_FIRST = ITEM_ID_POTION,
    ITEM_ID_END = 254, /* one past the last item; ENTD's Random and None follow */
};

/* world_unit_t.equipment 16-bit values carry the item id in the low ten
 * bits and flags above (bit 14 is tested by world_shop_buy_from_fitting_room
 * and the job wheel); the WORLD item tables are indexed by the masked id
 * (world_item_get_price, item_get_type). The range boundaries follow the
 * retail item table: throwable weapons start with Shuriken, followed by
 * shields, headgear, body armor, accessories, and consumables. */
enum {
    ITEM_ENTRY_ID_MASK = 0x3ff,
    ITEM_ENTRY_UNEQUIPPABLE = 0x4000,   /* item list entries: listed, but the unit cannot equip the item */
    ITEM_LIST_END = 0xff,               /* the item id that ends an item list */
    WORLD_EQUIPMENT_SHOP_ITEM = 0x8000, /* equipment 16-bit values: an unpaid shop item tried on in the fitting room */
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

/* records */
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

/* weapon_data_t.flags. */
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

/* Weapon record selected by item_data_t.secondary_data_id. Offsets are fixed
 * by main_unit_set_equipment_stats and main_unit_calculate_entd_equipment. */
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

/* Three-byte item ability records, g_main_item_secondary_data. Item abilities
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

/* Common and rare Fur Shop items obtained by poaching one monster job. */
typedef struct item_poach {
    u8 common_item;
    u8 rare_item;
} item_poach_t;
typedef char item_poach_size_must_be_2[(sizeof(item_poach_t) == 2) ? 1 : -1];

typedef struct armor_data {
    u8 hp_bonus; /* 0x00; added to unit_stats_t.max_hp */
    u8 mp_bonus; /* 0x01; added to unit_stats_t.max_mp */
} armor_data_t;

/* Physical attack, magic attack and speed, in the order units and item
 * attribute records store them. */
typedef enum unit_attribute_index {
    UNIT_ATTRIBUTE_PHYSICAL_ATTACK = 0,
    UNIT_ATTRIBUTE_MAGIC_ATTACK = 1,
    UNIT_ATTRIBUTE_SPEED = 2,
    UNIT_ATTRIBUTE_COUNT = 3,
} unit_attribute_index_e;

/* 25-byte item Attribute record (g_main_item_attributes). MAIN applies these
 * bonuses to battle units; WORLD's item_collect_stat_deltas reads the same
 * first five bytes for equipment previews. */
typedef struct item_attribute {
    u8 attributes[UNIT_ATTRIBUTE_COUNT]; /* 0x00; unit_attribute_index_e */
    u8 move;                             /* 0x03 */
    u8 jump;                             /* 0x04 */
    initial_status_sets_t status_sets;   /* 0x05 */
    u8 elemental_affinity[5];            /* 0x14; elemental_affinity_index_e */
} item_attribute_t;
typedef char item_attribute_size_must_be_25[(sizeof(item_attribute_t) == 25) ? 1 : -1];

/* Flat byte view of item attributes used by the status-icon renderers. */
typedef struct main_item_attribute_flat {
    u8 attributes[UNIT_ATTRIBUTE_COUNT];
    u8 move;
    u8 jump;
    u8 innate[5];
    u8 immunity[5];
    u8 starting[5];
    u8 elemental_affinity[5];
} main_item_attribute_flat_t;

/* g_main_item_type_order_tables: seven independent signed-byte order lists.
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

#endif
