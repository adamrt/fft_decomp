#ifndef SKILLSET_H
#define SKILLSET_H

/* Skillsets: their ids and record sizes, and BATTLE's loaders that list a unit's entries for each skillset of the
 * action menu (Item, Throw, Math Skill, Elemental, Monster Skill, Draw Out, Charge). */

#include "fft/unit.h"
#include "psx/types.h"

/* skillset records */
/* Skillset ids: the rows of g_ability_human_skillset_abilities and, from
 * SKILLSET_ID_MONSTER_FIRST, of g_ability_monster_skillset_abilities. A
 * name is the skillset's name in the game's text. Where rows share a name, a
 * suffix names the job whose record has the row (job_data_t.skillset) or, for
 * a row no job has, the unit the ENTD gives it to; a second row for one
 * character is told apart by an ability it lists. Monster rows have no name
 * text and take their job's name. A row with no name text has an enumerator
 * only where the code mentions it. */
typedef enum skillset_id {
    SKILLSET_ID_ATTACK = 1,
    SKILLSET_ID_DEFEND = 2,
    SKILLSET_ID_EQUIP_CHANGE = 3,
    SKILLSET_ID_BLANK_04 = 4,
    SKILLSET_ID_BASIC_SKILL = 5,
    SKILLSET_ID_ITEM = 6,
    SKILLSET_ID_BATTLE_SKILL = 7,
    SKILLSET_ID_CHARGE = 8,
    SKILLSET_ID_PUNCH_ART = 9,
    SKILLSET_ID_WHITE_MAGIC = 10,
    SKILLSET_ID_BLACK_MAGIC = 11,
    SKILLSET_ID_TIME_MAGIC = 12,
    SKILLSET_ID_SUMMON_MAGIC = 13,
    SKILLSET_ID_STEAL = 14,
    SKILLSET_ID_TALK_SKILL = 15,
    SKILLSET_ID_YIN_YANG_MAGIC = 16,
    SKILLSET_ID_ELEMENTAL = 17,
    SKILLSET_ID_JUMP = 18,
    SKILLSET_ID_DRAW_OUT = 19,
    SKILLSET_ID_THROW = 20,
    SKILLSET_ID_MATH_SKILL = 21,
    SKILLSET_ID_SING = 22,
    SKILLSET_ID_DANCE = 23,
    SKILLSET_ID_MIMIC = 24,
    SKILLSET_ID_GUTS_RAMZA_CHAPTER_1 = 25,
    SKILLSET_ID_GUTS_RAMZA_CHAPTER_2_3 = 26,
    SKILLSET_ID_GUTS_RAMZA_CHAPTER_4 = 27,
    SKILLSET_ID_GUTS_DELITA = 28,
    SKILLSET_ID_HOLY_SWORD_DELITA = 29,
    SKILLSET_ID_MIGHTY_SWORD_DELITA = 30,
    SKILLSET_ID_BASIC_SKILL_ALGUS = 31,
    SKILLSET_ID_DARK_SWORD_GAFGARION_GUEST = 32,
    SKILLSET_ID_HOLY_SWORD_AGRIAS_GUEST = 33,
    SKILLSET_ID_HOLY_SWORD_WIEGRAF_CHAPTER_1 = 34,
    SKILLSET_ID_MAGIC = 35, /* JOB_ID_DELITAS_SIS_TETA */
    SKILLSET_ID_HOLY_MAGIC_OVELIA = 36,
    SKILLSET_ID_SNIPE_MUSTADIO_GUEST = 37,
    SKILLSET_ID_SNIPE_MUSTADIO_JOIN = 38,
    SKILLSET_ID_DARK_SWORD_GAFGARION_ENEMY = 39,
    SKILLSET_ID_HOLY_SWORD_AGRIAS_JOIN = 40,
    SKILLSET_ID_LIMIT = 41, /* JOB_ID_SOLDIER_CLOUD */
    SKILLSET_ID_WHITE_AID = 42,
    SKILLSET_ID_DRAGON = 43,
    SKILLSET_ID_BREATH = 44,
    SKILLSET_ID_TRUTH_RAFA_GUEST = 45,
    SKILLSET_ID_UN_TRUTH = 46, /* both Malak jobs */
    SKILLSET_ID_STARRY_HEAVEN = 47,
    SKILLSET_ID_HOLY_SWORD_WIEGRAF_CHAPTER_3 = 48,
    SKILLSET_ID_HOLY_MAGIC_ALMA = 49,
    SKILLSET_ID_TRUTH_RAFA_JOIN = 50,
    SKILLSET_ID_BATTLE_SKILL_IZLUDE = 51,
    SKILLSET_ID_JUMP_IZLUDE = 52, /* ENTD second skillset */
    SKILLSET_ID_PUNCH_SKILL = 53, /* Wiegraf's ENTD second skillset */
    SKILLSET_ID_USE_HAND_CELIA = 54,
    SKILLSET_ID_USE_HAND_LEDE = 55,
    SKILLSET_ID_THROW_CELIA = 56,                       /* ENTD second skillset */
    SKILLSET_ID_THROW_LEDE = 57,                        /* ENTD second skillset */
    SKILLSET_ID_HOLY_SWORD_WIEGRAF_LIGHTNING_STAB = 58, /* ENTD: Wiegraf, beside PUNCH_SKILL; Lightning Stab alone */
    SKILLSET_ID_SWORD_SPIRIT_ELMDOR = 59,
    SKILLSET_ID_MIGHTY_SWORD_ROFEL = 60,
    SKILLSET_ID_ALL_MAGIC_ROFEL = 61,                  /* ENTD second skillset */
    SKILLSET_ID_SWORD_SPIRIT_ELMDOR_CHIRIJIRADEN = 62, /* ENTD: Elmdor, beside BLOOD_SUCK; two more swords */
    SKILLSET_ID_BLOOD_SUCK = 63,                       /* ENTD second skillset of Elmdor and the vampire Zalbag */
    SKILLSET_ID_MIGHTY_SWORD_VORMAV = 64,
    SKILLSET_ID_ALL_MAGIC_VORMAV = 65, /* ENTD second skillset */
    SKILLSET_ID_MIGHTY_SWORD_MELIADOUL_ENEMY = 66,
    SKILLSET_ID_MIGHTY_SWORD_MELIADOUL_JOIN = 67,
    SKILLSET_ID_SNIPE_BALK = 68,
    SKILLSET_ID_MAGIC_SWORD = 69,
    SKILLSET_ID_SWORD_SKILL_DYCEDARG = 70,
    SKILLSET_ID_ALL_MAGIC_DYCEDARG = 71, /* ENTD second skillset */
    SKILLSET_ID_ALL_MAGIC_KLETIAN = 72,
    SKILLSET_ID_PHANTOM = 73, /* Kletian's ENTD second skillset */
    SKILLSET_ID_ALL_SWORDSKILL = 74,
    SKILLSET_ID_DESTROY_SWORD = 75,          /* both Zalbag jobs */
    SKILLSET_ID_HOLY_MAGIC_ALMA_ULTIMA = 76, /* the two _KO Alma jobs; adds Ultima */
    SKILLSET_ID_FEAR_VELIUS = 103,
    SKILLSET_ID_WARLOCK_SUMMON = 104, /* Velius's ENTD second skillset */
    SKILLSET_ID_FEAR_ZALERA = 107,
    SKILLSET_ID_JA_MAGIC = 108, /* Zalera's ENTD second skillset */
    SKILLSET_ID_FEAR_HASHMALUM = 111,
    SKILLSET_ID_DIMENSION_MAGIC = 112, /* "Dimension Magc" in the name text */
    SKILLSET_ID_FEAR_QUEKLAIN = 115,
    SKILLSET_ID_IMPURE = 116, /* Queklain's ENTD second skillset */
    SKILLSET_ID_FEAR_ADRAMELK = 119,
    SKILLSET_ID_ALL_MAGIC_ADRAMELK = 120, /* ENTD second skillset */
    SKILLSET_ID_ULTIMATE_MAGIC = 123,     /* ENTD: Holy Angel Altima, whose job has the empty 0x59 */
    SKILLSET_ID_CHAOS = 124,              /* ENTD second skillset of Holy Angel Altima */
    SKILLSET_ID_COMPLETE_MAGIC = 125,     /* JOB_ID_ARCH_ANGEL_ALTIMA */
    SKILLSET_ID_SATURATION = 126,         /* ENTD second skillset of Arch Angel Altima */
    SKILLSET_ID_SWORD_SKILL_UNDEAD = 155,
    SKILLSET_ID_CHARGE_UNDEAD = 156,
    SKILLSET_ID_BLACK_MAGIC_UNDEAD = 157,
    SKILLSET_ID_TIME_MAGIC_UNDEAD = 158,
    SKILLSET_ID_YIN_YANG_MAGIC_UNDEAD = 159,
    SKILLSET_ID_SUMMON_MAGIC_UNDEAD = 160,
    SKILLSET_ID_ITEM_NO_RSM = 161,           /* the ITEM abilities with no R/S/M */
    SKILLSET_ID_WHITE_MAGIC_NO_RSM = 162,    /* ENTD first skillset of JOB_ID_PRIEST_36 */
    SKILLSET_ID_BLACK_MAGIC_NO_RSM = 163,    /* ENTD first skillset of JOB_ID_WIZARD_37; four spells */
    SKILLSET_ID_YIN_YANG_MAGIC_NO_RSM = 164, /* ENTD first skillset of JOB_ID_ORACLE_38 */
    SKILLSET_ID_BLOOD_SUCK_STATUS = 167,     /* no name text; AI: a unit under the Blood Suck status */
    SKILLSET_ID_FROG_ATTACK = 169,           /* no name text, empty; AI: ABILITY_ID_FROG_ATTACK */
    SKILLSET_ID_BYBLOS = 170,
    SKILLSET_ID_WORK = 171,
    SKILLSET_ID_BIO = 172,
    SKILLSET_ID_DARK_CLOUD = 173,
    SKILLSET_ID_DARK_MAGIC = 174,
    SKILLSET_ID_NIGHT_MAGIC = 175,
    SKILLSET_ID_MONSTER_CHOCOBO = 176,
    SKILLSET_ID_MONSTER_BLACK_CHOCOBO = 177,
    SKILLSET_ID_MONSTER_RED_CHOCOBO = 178,
    SKILLSET_ID_MONSTER_GOBLIN = 179,
    SKILLSET_ID_MONSTER_BLACK_GOBLIN = 180,
    SKILLSET_ID_MONSTER_GOBBLEDEGUCK = 181,
    SKILLSET_ID_MONSTER_BOMB = 182,
    SKILLSET_ID_MONSTER_GRENADE = 183,
    SKILLSET_ID_MONSTER_EXPLOSIVE = 184,
    SKILLSET_ID_MONSTER_RED_PANTHER = 185,
    SKILLSET_ID_MONSTER_CUAR = 186,
    SKILLSET_ID_MONSTER_VAMPIRE = 187,
    SKILLSET_ID_MONSTER_PISCO_DEMON = 188,
    SKILLSET_ID_MONSTER_SQUIDLARKIN = 189,
    SKILLSET_ID_MONSTER_MINDFLARE = 190,
    SKILLSET_ID_MONSTER_SKELETON = 191,
    SKILLSET_ID_MONSTER_BONE_SNATCH = 192,
    SKILLSET_ID_MONSTER_LIVING_BONE = 193,
    SKILLSET_ID_MONSTER_GHOUL = 194,
    SKILLSET_ID_MONSTER_GUST = 195,
    SKILLSET_ID_MONSTER_REVNANT = 196,
    SKILLSET_ID_MONSTER_FLOTIBALL = 197,
    SKILLSET_ID_MONSTER_AHRIMAN = 198,
    SKILLSET_ID_MONSTER_PLAGUE = 199,
    SKILLSET_ID_MONSTER_JURAVIS = 200,
    SKILLSET_ID_MONSTER_STEEL_HAWK = 201,
    SKILLSET_ID_MONSTER_COCATORIS = 202,
    SKILLSET_ID_MONSTER_URIBO = 203,
    SKILLSET_ID_MONSTER_PORKY = 204,
    SKILLSET_ID_MONSTER_WILDBOW = 205,
    SKILLSET_ID_MONSTER_WOODMAN = 206,
    SKILLSET_ID_MONSTER_TRENT = 207,
    SKILLSET_ID_MONSTER_TAIJU = 208,
    SKILLSET_ID_MONSTER_BULL_DEMON = 209,
    SKILLSET_ID_MONSTER_MINITAURUS = 210,
    SKILLSET_ID_MONSTER_SACRED = 211,
    SKILLSET_ID_MONSTER_MORBOL = 212,
    SKILLSET_ID_MONSTER_OCHU = 213,
    SKILLSET_ID_MONSTER_GREAT_MORBOL = 214,
    SKILLSET_ID_MONSTER_BEHEMOTH = 215,
    SKILLSET_ID_MONSTER_KING_BEHEMOTH = 216,
    SKILLSET_ID_MONSTER_DARK_BEHEMOTH = 217,
    SKILLSET_ID_MONSTER_DRAGON = 218,
    SKILLSET_ID_MONSTER_BLUE_DRAGON = 219,
    SKILLSET_ID_MONSTER_RED_DRAGON = 220,
    SKILLSET_ID_MONSTER_HYUDRA = 221,
    SKILLSET_ID_MONSTER_HYDRA = 222,
    SKILLSET_ID_MONSTER_TIAMAT = 223,
    SKILLSET_ID_NONE = 255 /* empty ENTD skillset; ends a skillset or AI ability list */
} skillset_id_e;

enum { SKILLSET_ID_MONSTER_FIRST = 176, SKILLSET_ID_END = 224 };

/* Human skillsets contain 16 action slots followed by six reaction/support/movement
 * slots. Monster records contain four ids; the shared scratch list reserves 24. */
enum {
    HUMAN_SKILLSET_ACTION_SLOT_COUNT = 16,
    HUMAN_SKILLSET_ABILITY_COUNT = 22,
    MONSTER_SKILLSET_ABILITY_COUNT = 4,
    SKILLSET_ABILITY_LIST_COUNT = 24
};

/* Ability classes ability_skillset_list_build collects from a
 * skillset; the random R/S/M selector uses rsm_ability_filter_e instead. */
typedef enum skillset_ability_filter {
    SKILLSET_ABILITY_FILTER_ACTION = 0x01,
    SKILLSET_ABILITY_FILTER_REACTION = 0x02,
    SKILLSET_ABILITY_FILTER_MOVEMENT = 0x04,
    SKILLSET_ABILITY_FILTER_SUPPORT = 0x08,
    SKILLSET_ABILITY_FILTER_ALL = 0x0f
} skillset_ability_filter_e;

/* action menu lists */
s32 skillset_build_menu_list(s32 unit_id, u8* entries, u8* empty, u8* menu_types);
s32 skillset_get_unit_ability_data(
    s32 unit_id, u8 skillset, s16* ability_ids, u8* mp_out, u8* ct_out, s32 unused, u8* flags_out, u8* turns_out);
s32 skillset_load_item_abilities(s32 unit_id, s32 skillset, u8* out_item_ids);
s32 skillset_load_throw_abilities(s32 unit_id, s32 skillset, u8* out_item_ids);
s32 skillset_math_load_attributes(s32 unit_id, s32 skillset, s16* out_ability_ids);
s32 skillset_math_load_multiples(s32 unit_id, s32 skillset, s16* out_ability_ids);
s32 skillset_math_collect_calculator_abilities(s32 unit_id, u8 skillset, s16* out);
s32 skillset_monster_collect_abilities(s32 unit_id, u8 skillset, s16* out, s32 unused, u8* flags_out);
s32 skillset_monster_count_supporters(const unit_stats_t* unit);
s32 skillset_load_draw_out_abilities(s32 unit_id, s32 skillset, u8* out_item_ids);
s32 skillset_load_charge_ability_ids(s32 unit_id, u8 skillset_id, s16* ability_ids);
s32 skillset_load_charge_abilities(s32 unit_id, s32 skillset, s16* abilities, u8* ability_ct, u8* turns);

#endif
