#ifndef DATA_JOB_H
#define DATA_JOB_H

/* Job data: the job records and job ids. */

#include "fft/data_status.h"
#include "psx/types.h"

/* job records */
/* job_data_t.jump: the jump value and the stepping-stone flag. */
enum {
    JOB_JUMP_VALUE_MASK = 0x7f,
    JOB_JUMP_STEPPING_STONE = 0x80,
};

/* One pair for HP, MP, Speed, PA or MA. Unit growth uses the first byte;
 * deriving the battle stat uses the second. */
typedef struct job_stat_growth {
    u8 growth;
    u8 multiplier;
} job_stat_growth_t;
typedef char job_stat_growth_size_must_be_2[(sizeof(job_stat_growth_t) == 2) ? 1 : -1];

/*
 * 0x30-byte job record (g_job_data_pointer). The named fields are those copied
 * by unit_copy_job_data, unit_calculate_encounter_abilities and
 * unit_set_equipment_stats into unit_stats_t.
 */
typedef struct job_data {
    u8 skillset;                       /* 0x00 */
    u8 innate_abilities[8];            /* 0x01; four little-endian u16 ability ids, byte-copied */
    u8 equipment_categories[4];        /* 0x09 */
    job_stat_growth_t stat_growths[5]; /* 0x0d; HP, MP, Speed, PA, MA; copied to unit_stats_t 0x81 */
    u8 move;                           /* 0x17 */
    u8 jump;                           /* 0x18; JOB_JUMP_* */
    u8 class_evade;                    /* 0x19; unit_stats_t equipment-stat slot 0x43 */
    initial_status_sets_t status_sets; /* 0x1a */
    u8 elemental_affinity[4];          /* 0x29; affinity indices 0 through 3 */
    u8 spritesheet_id;                 /* 0x2d */
    u8 job_portrait_palette;           /* 0x2e */
    u8 monster_graphic;                /* 0x2f; copied to unit_stats_t 0x15e */
} job_data_t;
typedef char job_data_size_must_be_48[(sizeof(job_data_t) == 48) ? 1 : -1];

/* The job record the ATTACK and REQUIRE party portrait builders last looked
 * up; each overlay defines its own. */
extern job_data_t* g_formation_job_data;

/* job ids */
/* Job ids index the 160 records of g_job_data. A name is the job's name
 * in the game's text. Where records share a name, a suffix names the
 * character: the unit name the game's text has at the same index, which is
 * also the name the ENTD gives the job's units. Two records of one character
 * are told apart by the chapter, by the role the ENTD gives the unit (GUEST,
 * JOIN, ENEMY) or by the record's status sets (UNDEAD, INNATE_KO and
 * STARTING_KO are its innate and starting statuses). A numbered suffix marks a
 * record that repeats a name with nothing but its id to name it: 0x35-0x38
 * repeat four generic jobs for ENTD units that can equip every item type, and
 * 0x98 is an empty second Holy Dragon. Records without name text (0x00, 0x39-0x3b, 0x9b-0x9f) and
 * those the text calls "None" (0x8e, 0x8f, 0x92-0x95) have no enumerator. */
typedef enum job_id {
    JOB_ID_SQUIRE_RAMZA_CHAPTER_1 = 1,
    JOB_ID_SQUIRE_RAMZA_CHAPTER_2_3 = 2,
    JOB_ID_SQUIRE_RAMZA_CHAPTER_4 = 3,
    JOB_ID_SQUIRE_DELITA = 4,
    JOB_ID_HOLY_KNIGHT_DELITA = 5,
    JOB_ID_ARC_KNIGHT_DELITA = 6,
    JOB_ID_SQUIRE_ALGUS = 7,
    JOB_ID_ARC_KNIGHT_ZALBAG = 8,
    JOB_ID_LUNE_KNIGHT_DYCEDARG = 9,
    JOB_ID_DUKE_LARG = 10,
    JOB_ID_DUKE_GOLTANA = 11,
    JOB_ID_PRINCESS_OVELIA = 12,
    JOB_ID_HOLY_SWORDSMAN_ORLANDU = 13,
    JOB_ID_HIGH_PRIEST_FUNERAL = 14,
    JOB_ID_DRAGONER_REIS = 15,
    JOB_ID_HOLY_PRIEST_ZALMO = 16,
    JOB_ID_DARK_KNIGHT_GAFGARION_ENEMY = 17,
    JOB_ID_HELL_KNIGHT_MALAK_INNATE_KO = 18,
    JOB_ID_BISHOP_SIMON = 19,
    JOB_ID_CLERIC_ALMA_STARTING_KO = 20,
    JOB_ID_ASTROLOGIST_OLAN = 21,
    JOB_ID_ENGINEER_MUSTADIO_JOIN = 22,
    JOB_ID_DARK_KNIGHT_GAFGARION_GUEST = 23,
    JOB_ID_CARDINAL_DRACLAU = 24,
    JOB_ID_HEAVEN_KNIGHT_RAFA_GUEST = 25,
    JOB_ID_HELL_KNIGHT_MALAK = 26,
    JOB_ID_ARC_KNIGHT_ELMDOR = 27,
    JOB_ID_DELITAS_SIS_TETA = 28,
    JOB_ID_ARC_DUKE_BARINTEN = 29,
    JOB_ID_HOLY_KNIGHT_AGRIAS_JOIN = 30,
    JOB_ID_TEMPLE_KNIGHT_BEOWULF = 31,
    JOB_ID_WHITE_KNIGHT_WIEGRAF_CHAPTER_1 = 32,
    JOB_ID_ARC_WITCH_BALMAFULA = 33,
    JOB_ID_ENGINEER_MUSTADIO_GUEST = 34,
    JOB_ID_BI_COUNT_RUDVICH = 35,
    JOB_ID_DIVINE_KNIGHT_VORMAV = 36,
    JOB_ID_DIVINE_KNIGHT_ROFEL = 37,
    JOB_ID_KNIGHT_BLADE_IZLUDE = 38,
    JOB_ID_SORCERER_KLETIAN = 39, /* "Sorceror" in the name text */
    JOB_ID_WHITE_KNIGHT_WIEGRAF_CHAPTER_3 = 40,
    JOB_ID_HEAVEN_KNIGHT_RAFA_JOIN = 41,
    JOB_ID_DIVINE_KNIGHT_MELIADOUL_JOIN = 42,
    JOB_ID_ENGINEER_BALK = 43,
    JOB_ID_CLERIC_ALMA_INNATE_KO = 44,
    JOB_ID_ASSASSIN_CELIA = 45,
    JOB_ID_ASSASSIN_LEDE = 46,
    JOB_ID_DIVINE_KNIGHT_MELIADOUL_ENEMY = 47,
    JOB_ID_CLERIC_ALMA = 48,
    JOB_ID_PHONY_SAINT_AJORA = 49,
    JOB_ID_SOLDIER_CLOUD = 50,
    JOB_ID_ARC_KNIGHT_ZALBAG_VAMPIRE = 51,
    JOB_ID_HOLY_KNIGHT_AGRIAS_GUEST = 52,
    JOB_ID_CHEMIST_35 = 53,
    JOB_ID_PRIEST_36 = 54,
    JOB_ID_WIZARD_37 = 55,
    JOB_ID_ORACLE_38 = 56,

    JOB_ID_WARLOCK_VELIUS = 60,
    JOB_ID_KNIGHT_UNDEAD = 61,
    JOB_ID_ANGEL_OF_DEATH_ZALERA = 62,
    JOB_ID_ARCHER_UNDEAD = 63,
    JOB_ID_REGULATOR_HASHMALUM = 64,
    JOB_ID_HOLY_ANGEL_ALTIMA = 65,
    JOB_ID_WIZARD_UNDEAD = 66,
    JOB_ID_IMPURE_KING_QUEKLAIN = 67,
    JOB_ID_TIME_MAGE_UNDEAD = 68,
    JOB_ID_GHOST_OF_FURY_ADRAMELK = 69,
    JOB_ID_ORACLE_UNDEAD = 70,
    JOB_ID_SUMMONER_UNDEAD = 71,
    JOB_ID_HOLY_DRAGON_REIS = 72,
    JOB_ID_ARCH_ANGEL_ALTIMA = 73,

    JOB_ID_SQUIRE = 74,
    JOB_ID_CHEMIST = 75,
    JOB_ID_KNIGHT = 76,
    JOB_ID_ARCHER = 77,
    JOB_ID_MONK = 78,
    JOB_ID_PRIEST = 79,
    JOB_ID_WIZARD = 80,
    JOB_ID_TIME_MAGE = 81,
    JOB_ID_SUMMONER = 82,
    JOB_ID_THIEF = 83,
    JOB_ID_MEDIATOR = 84,
    JOB_ID_ORACLE = 85,
    JOB_ID_GEOMANCER = 86,
    JOB_ID_LANCER = 87,
    JOB_ID_SAMURAI = 88,
    JOB_ID_NINJA = 89,
    JOB_ID_CALCULATOR = 90,
    JOB_ID_BARD = 91,
    JOB_ID_DANCER = 92,
    JOB_ID_MIME = 93,

    JOB_ID_CHOCOBO = 94,
    JOB_ID_BLACK_CHOCOBO = 95,
    JOB_ID_RED_CHOCOBO = 96,
    JOB_ID_GOBLIN = 97,
    JOB_ID_BLACK_GOBLIN = 98,
    JOB_ID_GOBBLEDEGUCK = 99,
    JOB_ID_BOMB = 100,
    JOB_ID_GRENADE = 101,
    JOB_ID_EXPLOSIVE = 102,
    JOB_ID_RED_PANTHER = 103,
    JOB_ID_CUAR = 104,
    JOB_ID_VAMPIRE = 105,
    JOB_ID_PISCO_DEMON = 106,
    JOB_ID_SQUIDLARKIN = 107,
    JOB_ID_MINDFLARE = 108,
    JOB_ID_SKELETON = 109,
    JOB_ID_BONE_SNATCH = 110,
    JOB_ID_LIVING_BONE = 111,
    JOB_ID_GHOUL = 112,
    JOB_ID_GUST = 113,
    JOB_ID_REVNANT = 114,
    JOB_ID_FLOTIBALL = 115,
    JOB_ID_AHRIMAN = 116,
    JOB_ID_PLAGUE = 117,
    JOB_ID_JURAVIS = 118,
    JOB_ID_STEEL_HAWK = 119,
    JOB_ID_COCATORIS = 120,
    JOB_ID_URIBO = 121,
    JOB_ID_PORKY = 122,
    JOB_ID_WILDBOW = 123,
    JOB_ID_WOODMAN = 124,
    JOB_ID_TRENT = 125,
    JOB_ID_TAIJU = 126,
    JOB_ID_BULL_DEMON = 127,
    JOB_ID_MINITAURUS = 128,
    JOB_ID_SACRED = 129,
    JOB_ID_MORBOL = 130,
    JOB_ID_OCHU = 131,
    JOB_ID_GREAT_MORBOL = 132,
    JOB_ID_BEHEMOTH = 133,
    JOB_ID_KING_BEHEMOTH = 134,
    JOB_ID_DARK_BEHEMOTH = 135,
    JOB_ID_DRAGON = 136,
    JOB_ID_BLUE_DRAGON = 137,
    JOB_ID_RED_DRAGON = 138,
    JOB_ID_HYUDRA = 139,
    JOB_ID_HYDRA = 140,
    JOB_ID_TIAMAT = 141,

    JOB_ID_BYBLOS = 144,
    JOB_ID_STEEL_GIANT = 145,
    JOB_ID_APANDA = 150,
    JOB_ID_SERPENTARIUS = 151,
    JOB_ID_HOLY_DRAGON_98 = 152,
    JOB_ID_ARCHAIC_DEMON = 153,
    JOB_ID_ULTIMA_DEMON = 154,
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

/* The spritesheet of a generic unit (sprite_sheet_id_e): (job - JOB_ID_GENERIC_FIRST) * 2 plus the
 * male or female base, the Mime base for a Mime, whose sheets sit one job lower; Bard and Dancer have
 * their own sheets. */
enum {
    JOB_SHEET_GENERIC_MALE_MIME_BASE = 0x5e,
    JOB_SHEET_GENERIC_FEMALE_MIME_BASE = 0x5f,
    JOB_SHEET_GENERIC_MALE_BASE = 0x60,
    JOB_SHEET_GENERIC_FEMALE_BASE = 0x61,
    JOB_SHEET_BARD = 0x82,
    JOB_SHEET_DANCER = 0x83,
};

#endif
