#ifndef BATTLE_GFX_RESOURCE_H
#define BATTLE_GFX_RESOURCE_H

/* Battle sprite resources: the SHP, SEQ and SPR file layouts, spritesheet ids, the frame and
 * sequence tables, the VRAM slots that hold sheets and EVTCHR images, and the loader that fills them. */

#include "fft/battle_unit.h"
#include "fft/gfx.h"
#include "fft/main.h"
#include "fft/main_file.h"
#include "fft/unit.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* file layouts */
/* Serialized SHP/SEQ values are little-endian byte sequences. These file
 * views have byte alignment; their decoded RAM tables remain separate. */

/* A unit SHP frame set: primary at file +8; an optional submerged set is
 * addressed by the file header. Only the first 0xd0 offsets are installed. */
typedef struct battle_gfx_packed_unit_shp_frame_set {
    u8 frame_offsets[0x100][4]; /* 0x000; signed 32-bit values, -1 resolves to offset zero */
    u8 frame_data_size[2];      /* 0x400; unsigned 16-bit value */
    u8 frame_data[0];           /* 0x402; variable tail */
} battle_gfx_packed_unit_shp_frame_set_t;
typedef char
    battle_gfx_packed_unit_shp_frame_set_size_must_be_1026[sizeof(battle_gfx_packed_unit_shp_frame_set_t) == 1026 ? 1
                                                                                                                  : -1];

typedef struct battle_gfx_packed_unit_shp_record {
    u8 submerged_frame_set_offset[4]; /* 0x000; 8 means both native tables share the primary set */
    u8 first_attack_frame[2];         /* 0x004; unsigned 16-bit value */
    u8 first_sp2_frame[2];            /* 0x006; unsigned 16-bit value */
    battle_gfx_packed_unit_shp_frame_set_t primary_frame_set; /* 0x008 */
} battle_gfx_packed_unit_shp_record_t;
typedef char
    battle_gfx_packed_unit_shp_record_size_must_be_1034[sizeof(battle_gfx_packed_unit_shp_record_t) == 1034 ? 1 : -1];

/* WEP/EFF SHP has a different header and twice as many frame offsets. */
typedef struct battle_gfx_packed_weapon_shp_record {
    u8 _unused_000[4];          /* 0x000; loader skips this 32-bit value */
    u8 first_frame[0x20][2];    /* 0x004; unsigned zero-frame indices by weapon type */
    u8 frame_offsets[0x200][4]; /* 0x044; signed 32-bit values, -1 resolves to offset zero */
    u8 frame_data_size[2];      /* 0x844; unsigned 16-bit value */
    u8 frame_data[0];           /* 0x846; variable tail */
} battle_gfx_packed_weapon_shp_record_t;
typedef char battle_gfx_packed_weapon_shp_record_size_must_be_2118[sizeof(battle_gfx_packed_weapon_shp_record_t) == 2118
        ? 1
        : -1];

/* WEP/EFF SEQ stores 256 offsets followed by a variable sequence payload. */
typedef struct battle_gfx_packed_offset_table {
    u8 _unused_000[4];  /* 0x000; WEP/EFF loader skips the 32-bit header value */
    u8 entry[0x100][4]; /* 0x004; signed sequence offsets, -1 resolves to zero */
    u8 payload_size[2]; /* 0x404; unsigned 16-bit value */
    u8 payload[0];      /* 0x406; variable sequence data */
} battle_gfx_packed_offset_table_t;
typedef char
    battle_gfx_packed_offset_table_size_must_be_1030[sizeof(battle_gfx_packed_offset_table_t) == 1030 ? 1 : -1];

/* OTHER.SPR: the palette helper copies only the first 21 of 32 palettes. */
typedef struct battle_gfx_other_sprite_archive {
    palette_t palettes[32]; /* 0x0000 */
    u8 pixels[256 * 128];   /* 0x0400; uncompressed 256x256, 4bpp */
} battle_gfx_other_sprite_archive_t;
typedef char
    battle_gfx_other_sprite_archive_size_must_be_33792[sizeof(battle_gfx_other_sprite_archive_t) == 33792 ? 1 : -1];

/* WEP.SPR contains three CLUT/image pairs: weapons, trails and particles. */
typedef struct battle_gfx_weapon_sprite_archive {
    clut_t weapon_clut;           /* 0x00000 */
    u8 weapon_pixels[256][128];   /* 0x00200; 4bpp */
    clut_t trail_clut;            /* 0x08200 */
    u8 trail_pixels[256][128];    /* 0x08400; 4bpp */
    clut_t particle_clut;         /* 0x10400 */
    u8 particle_pixels[144][128]; /* 0x10600; 4bpp */
} battle_gfx_weapon_sprite_archive_t;

/* spritesheets */
/* Spritesheet ids index g_battle_gfx_spritesheet_data and
 * g_battle_gfx_spritesheet_files. An id below 0x80 is the character_identity_e
 * value it is loaded for (unit_get_spritesheet_palette returns those
 * unchanged) and has that identity's name; a numbered name loads the file of
 * the sheet with the plain name. From 0x60 the sheets are the generic jobs',
 * male then female: the generic selectors resolve to
 * 0x60 + (job - JOB_ID_SQUIRE) * 2, plus one for a female unit, with Dancer
 * and Mime moved down one job. Then comes one sheet per monster family, named
 * by the first job whose record has it (job_data_t.spritesheet_id), and the
 * four status sheets, which have no file of their own. */
typedef enum battle_spritesheet_id {
    BATTLE_SPRITESHEET_ID_RAMZA_CHAPTER_1 = 1,
    BATTLE_SPRITESHEET_ID_RAMZA_CHAPTER_2_3 = 2,
    BATTLE_SPRITESHEET_ID_RAMZA_CHAPTER_4 = 3,
    BATTLE_SPRITESHEET_ID_DELITA_CHAPTER_1 = 4,
    BATTLE_SPRITESHEET_ID_DELITA_CHAPTER_2_3 = 5,
    BATTLE_SPRITESHEET_ID_DELITA_CHAPTER_4 = 6,
    BATTLE_SPRITESHEET_ID_ALGUS = 7,
    BATTLE_SPRITESHEET_ID_ZALBAG = 8,
    BATTLE_SPRITESHEET_ID_DYCEDARG = 9,
    BATTLE_SPRITESHEET_ID_LARG = 10,
    BATTLE_SPRITESHEET_ID_GOLTANA = 11,
    BATTLE_SPRITESHEET_ID_OVELIA = 12,
    BATTLE_SPRITESHEET_ID_ORLANDU = 13,
    BATTLE_SPRITESHEET_ID_FUNERAL = 14,
    BATTLE_SPRITESHEET_ID_REIS_HUMAN = 15,
    BATTLE_SPRITESHEET_ID_ZALMO = 16,
    BATTLE_SPRITESHEET_ID_GAFGARION_ENEMY = 17,
    BATTLE_SPRITESHEET_ID_MALAK_12 = 18,
    BATTLE_SPRITESHEET_ID_SIMON = 19,
    BATTLE_SPRITESHEET_ID_ALMA_BATTLE = 20,
    BATTLE_SPRITESHEET_ID_OLAN = 21,
    BATTLE_SPRITESHEET_ID_MUSTADIO_JOIN = 22,
    BATTLE_SPRITESHEET_ID_GAFGARION_GUEST = 23,
    BATTLE_SPRITESHEET_ID_DRACLAU = 24,
    BATTLE_SPRITESHEET_ID_RAFA_GUEST = 25,
    BATTLE_SPRITESHEET_ID_MALAK = 26,
    BATTLE_SPRITESHEET_ID_ELMDOR = 27,
    BATTLE_SPRITESHEET_ID_TETA = 28,
    BATTLE_SPRITESHEET_ID_BARINTEN = 29,
    BATTLE_SPRITESHEET_ID_AGRIAS_JOIN = 30,
    BATTLE_SPRITESHEET_ID_BEOWULF = 31,
    BATTLE_SPRITESHEET_ID_WIEGRAF_CHAPTER_1 = 32,
    BATTLE_SPRITESHEET_ID_BALMAFULA = 33,
    BATTLE_SPRITESHEET_ID_MUSTADIO_GUEST = 34,
    BATTLE_SPRITESHEET_ID_RUDVICH = 35,
    BATTLE_SPRITESHEET_ID_VORMAV = 36,
    BATTLE_SPRITESHEET_ID_ROFEL = 37,
    BATTLE_SPRITESHEET_ID_IZLUDE = 38,
    BATTLE_SPRITESHEET_ID_KLETIAN = 39,
    BATTLE_SPRITESHEET_ID_WIEGRAF_CHAPTER_2_3 = 40,
    BATTLE_SPRITESHEET_ID_RAFA_JOIN = 41,
    BATTLE_SPRITESHEET_ID_MELIADOUL_JOIN = 42,
    BATTLE_SPRITESHEET_ID_BALK = 43,
    BATTLE_SPRITESHEET_ID_ALMA_2C = 44,
    BATTLE_SPRITESHEET_ID_CELIA = 45,
    BATTLE_SPRITESHEET_ID_LEDE = 46,
    BATTLE_SPRITESHEET_ID_MELIADOUL_ENEMY = 47,
    BATTLE_SPRITESHEET_ID_ALMA_EVENTS = 48,
    BATTLE_SPRITESHEET_ID_AJORA = 49,
    BATTLE_SPRITESHEET_ID_CLOUD = 50,
    BATTLE_SPRITESHEET_ID_ZALBAG_ZOMBIE = 51,
    BATTLE_SPRITESHEET_ID_AGRIAS_GUEST = 52,
    BATTLE_SPRITESHEET_ID_FEMALE_CHEMIST_35 = 53,
    BATTLE_SPRITESHEET_ID_FEMALE_PRIEST_36 = 54,
    BATTLE_SPRITESHEET_ID_MALE_WIZARD_37 = 55,
    BATTLE_SPRITESHEET_ID_MALE_ORACLE_38 = 56,
    BATTLE_SPRITESHEET_ID_MALE_SQUIRE_39 = 57,
    BATTLE_SPRITESHEET_ID_CELIA_3A = 58,
    BATTLE_SPRITESHEET_ID_LEDE_3B = 59,
    BATTLE_SPRITESHEET_ID_VELIUS = 60,
    BATTLE_SPRITESHEET_ID_MALE_KNIGHT_3D = 61,
    BATTLE_SPRITESHEET_ID_ZALERA = 62,
    BATTLE_SPRITESHEET_ID_MALE_ARCHER_3F = 63,
    BATTLE_SPRITESHEET_ID_HASHMALUM = 64,
    BATTLE_SPRITESHEET_ID_ALTIMA_FIRST_FORM = 65,
    BATTLE_SPRITESHEET_ID_MALE_WIZARD_42 = 66,
    BATTLE_SPRITESHEET_ID_QUEKLAIN = 67,
    BATTLE_SPRITESHEET_ID_FEMALE_TIME_MAGE_44 = 68,
    BATTLE_SPRITESHEET_ID_ADRAMELK = 69,
    BATTLE_SPRITESHEET_ID_MALE_ORACLE_46 = 70,
    BATTLE_SPRITESHEET_ID_FEMALE_SUMMONER_47 = 71,
    BATTLE_SPRITESHEET_ID_REIS_DRAGON = 72,
    BATTLE_SPRITESHEET_ID_ALTIMA_SECOND_FORM = 73,
    BATTLE_SPRITESHEET_ID_BOY = 74,
    BATTLE_SPRITESHEET_ID_GIRL = 75,
    BATTLE_SPRITESHEET_ID_YOUNG_MAN = 76,
    BATTLE_SPRITESHEET_ID_YOUNG_WOMAN = 77,
    BATTLE_SPRITESHEET_ID_MIDDLE_AGED_MAN = 78,
    BATTLE_SPRITESHEET_ID_MIDDLE_AGED_WOMAN = 79,
    BATTLE_SPRITESHEET_ID_OLD_MAN = 80,
    BATTLE_SPRITESHEET_ID_OLD_WOMAN = 81,
    BATTLE_SPRITESHEET_ID_OLD_FUNERAL_MAN = 82,
    BATTLE_SPRITESHEET_ID_OLD_FUNERAL_WOMAN = 83,
    BATTLE_SPRITESHEET_ID_FUNERAL_MAN = 84,
    BATTLE_SPRITESHEET_ID_FUNERAL_WOMAN = 85,
    BATTLE_SPRITESHEET_ID_FUNERAL_PRIEST = 86,
    BATTLE_SPRITESHEET_ID_MALE_SQUIRE_57 = 87,
    BATTLE_SPRITESHEET_ID_MALE_SQUIRE_58 = 88,
    BATTLE_SPRITESHEET_ID_MALE_SQUIRE_59 = 89,
    BATTLE_SPRITESHEET_ID_MALE_SQUIRE_5A = 90,
    BATTLE_SPRITESHEET_ID_MALE_SQUIRE_5B = 91,
    BATTLE_SPRITESHEET_ID_MALE_SQUIRE_5C = 92,
    BATTLE_SPRITESHEET_ID_DAMI = 93,    /* named by its file, DAMI.SPR; no ENTD unit has it */
    BATTLE_SPRITESHEET_ID_KASANEK = 94, /* named by its file, KASANEK.SPR; no ENTD unit has it */
    BATTLE_SPRITESHEET_ID_KASANEM = 95, /* named by its file, KASANEM.SPR; no ENTD unit has it */
    BATTLE_SPRITESHEET_ID_MALE_SQUIRE = 96,
    BATTLE_SPRITESHEET_ID_FEMALE_SQUIRE = 97,
    BATTLE_SPRITESHEET_ID_MALE_CHEMIST = 98,
    BATTLE_SPRITESHEET_ID_FEMALE_CHEMIST = 99,
    BATTLE_SPRITESHEET_ID_MALE_KNIGHT = 100,
    BATTLE_SPRITESHEET_ID_FEMALE_KNIGHT = 101,
    BATTLE_SPRITESHEET_ID_MALE_ARCHER = 102,
    BATTLE_SPRITESHEET_ID_FEMALE_ARCHER = 103,
    BATTLE_SPRITESHEET_ID_MALE_MONK = 104,
    BATTLE_SPRITESHEET_ID_FEMALE_MONK = 105,
    BATTLE_SPRITESHEET_ID_MALE_PRIEST = 106,
    BATTLE_SPRITESHEET_ID_FEMALE_PRIEST = 107,
    BATTLE_SPRITESHEET_ID_MALE_WIZARD = 108,
    BATTLE_SPRITESHEET_ID_FEMALE_WIZARD = 109,
    BATTLE_SPRITESHEET_ID_MALE_TIME_MAGE = 110,
    BATTLE_SPRITESHEET_ID_FEMALE_TIME_MAGE = 111,
    BATTLE_SPRITESHEET_ID_MALE_SUMMONER = 112,
    BATTLE_SPRITESHEET_ID_FEMALE_SUMMONER = 113,
    BATTLE_SPRITESHEET_ID_MALE_THIEF = 114,
    BATTLE_SPRITESHEET_ID_FEMALE_THIEF = 115,
    BATTLE_SPRITESHEET_ID_MALE_MEDIATOR = 116,
    BATTLE_SPRITESHEET_ID_FEMALE_MEDIATOR = 117,
    BATTLE_SPRITESHEET_ID_MALE_ORACLE = 118,
    BATTLE_SPRITESHEET_ID_FEMALE_ORACLE = 119,
    BATTLE_SPRITESHEET_ID_MALE_GEOMANCER = 120,
    BATTLE_SPRITESHEET_ID_FEMALE_GEOMANCER = 121,
    BATTLE_SPRITESHEET_ID_MALE_LANCER = 122,
    BATTLE_SPRITESHEET_ID_FEMALE_LANCER = 123,
    BATTLE_SPRITESHEET_ID_MALE_SAMURAI = 124,
    BATTLE_SPRITESHEET_ID_FEMALE_SAMURAI = 125,
    BATTLE_SPRITESHEET_ID_MALE_NINJA = 126,
    BATTLE_SPRITESHEET_ID_FEMALE_NINJA = 127,
    BATTLE_SPRITESHEET_ID_MALE_CALCULATOR = 128,
    BATTLE_SPRITESHEET_ID_FEMALE_CALCULATOR = 129,
    BATTLE_SPRITESHEET_ID_BARD = 130,
    BATTLE_SPRITESHEET_ID_DANCER = 131,
    BATTLE_SPRITESHEET_ID_MALE_MIME = 132,
    BATTLE_SPRITESHEET_ID_FEMALE_MIME = 133,
    BATTLE_SPRITESHEET_ID_CHOCOBO = 134,
    BATTLE_SPRITESHEET_ID_GOBLIN = 135,
    BATTLE_SPRITESHEET_ID_BOMB = 136,
    BATTLE_SPRITESHEET_ID_RED_PANTHER = 137,
    BATTLE_SPRITESHEET_ID_PISCO_DEMON = 138,
    BATTLE_SPRITESHEET_ID_SKELETON = 139,
    BATTLE_SPRITESHEET_ID_GHOUL = 140,
    BATTLE_SPRITESHEET_ID_FLOTIBALL = 141,
    BATTLE_SPRITESHEET_ID_JURAVIS = 142,
    BATTLE_SPRITESHEET_ID_URIBO = 143,
    BATTLE_SPRITESHEET_ID_WOODMAN = 144,
    BATTLE_SPRITESHEET_ID_BULL_DEMON = 145,
    BATTLE_SPRITESHEET_ID_MORBOL = 146,
    BATTLE_SPRITESHEET_ID_BEHEMOTH = 147,
    BATTLE_SPRITESHEET_ID_DRAGON = 148, /* also Reis as a Holy Dragon */
    BATTLE_SPRITESHEET_ID_HYUDRA = 149,
    BATTLE_SPRITESHEET_ID_BYBLOS = 150, /* and Apanda */
    BATTLE_SPRITESHEET_ID_SERPENTARIUS = 151,
    BATTLE_SPRITESHEET_ID_DRAGON_98 = 152,     /* loads the Dragon file again; no job has it */
    BATTLE_SPRITESHEET_ID_ARCHAIC_DEMON = 153, /* and Ultima Demon */
    BATTLE_SPRITESHEET_ID_STEEL_GIANT = 154,
    BATTLE_SPRITESHEET_ID_CRYSTAL = 155,
    BATTLE_SPRITESHEET_ID_CHICKEN = 156,
    BATTLE_SPRITESHEET_ID_FROG = 157,
    BATTLE_SPRITESHEET_ID_TREASURE = 158,
} battle_spritesheet_id_e;

enum {
    BATTLE_SPRITESHEET_ID_RAMZA_END = 4,
    BATTLE_SPRITESHEET_ID_JOB_FIRST = 96, /* the generic job sheets start here; the monsters' follow */
    BATTLE_SPRITESHEET_ID_COUNT = 159,
};

/* Four-byte lookup record for the 0x9f ordinary unit spritesheets. */
typedef struct battle_gfx_spritesheet_data {
    u8 shp_id;         /* battle_shp_type_e */
    u8 seq_id;         /* battle_seq_type_e */
    u8 flying_flag;    /* nonzero: the sheet idles with the BATTLE_UNIT_ANIMATION_IDLE_FLYING animations */
    u8 graphic_height; /* height of the standing sprite in pixels */
} battle_gfx_spritesheet_data_t;
typedef char battle_gfx_spritesheet_data_size_must_be_4[(sizeof(battle_gfx_spritesheet_data_t) == 4) ? 1 : -1];

/* battle_gfx_spritesheet_data_t.shp_id: index of the sheet's frame table file in
 * g_battle_gfx_sprite_shp_files (BATTLE/TYPE1.SHP ...). Id 5 has no file. */
typedef enum battle_shp_type {
    BATTLE_SHP_TYPE1 = 0,
    BATTLE_SHP_TYPE2 = 1,
    BATTLE_SHP_CYOKO = 2,
    BATTLE_SHP_MON = 3,
    BATTLE_SHP_OTHER = 4,
    BATTLE_SHP_ARUTE = 6,
    BATTLE_SHP_KANZEN = 7,
} battle_shp_type_e;

/* battle_gfx_spritesheet_data_t.seq_id: index of the sheet's animation file in
 * g_battle_gfx_sprite_seq_files (BATTLE/TYPE1.SEQ ...). */
typedef enum battle_seq_type {
    BATTLE_SEQ_TYPE1 = 0,
    BATTLE_SEQ_TYPE3 = 1,
    BATTLE_SEQ_CYOKO = 2,
    BATTLE_SEQ_MON = 3,
    BATTLE_SEQ_OTHER = 4,
    BATTLE_SEQ_RUKA = 5,
    BATTLE_SEQ_ARUTE = 6,
    BATTLE_SEQ_KANZEN = 7,
} battle_seq_type_e;

extern battle_gfx_spritesheet_data_t g_battle_gfx_spritesheet_data[BATTLE_SPRITESHEET_ID_COUNT];

u32 battle_gfx_get_spritesheet_flying_flag(u32 spritesheet_id);
s32 battle_gfx_get_unit_spritesheet_height_by_runtime_id(u32 runtime_id);
s32 battle_gfx_get_unit_spritesheet_height_by_battle_id(u32 battle_id);
s32 battle_gfx_is_spritesheet_id_loaded(s16 spritesheet_id);

/* frame and sequence tables */
extern battle_gfx_native_unit_seq_table_t g_battle_gfx_boss_seq_tables;
extern battle_gfx_unit_shp_frame_tables_t g_battle_gfx_boss_shp_frame_tables;
extern battle_gfx_wep_eff_graphics_t g_battle_gfx_effect_graphics[1];
/* Sprite part width/height in 8-pixel tiles, indexed by SHP size bits. */
extern size32_t g_battle_gfx_part_sizes[];
/* By battle_shp_type_e: the frame a unit is left on when its sequence ends
 * or its attack image is taken away, seen from behind and from the front. */
extern u8 g_battle_gfx_targeted_frame_back[];
extern u8 g_battle_gfx_targeted_frame_front[];
extern battle_gfx_native_unit_seq_table_t g_battle_gfx_unit_seq_tables[5];
extern battle_gfx_unit_shp_frame_tables_t g_battle_gfx_unit_shp_frame_tables[5];
extern battle_gfx_wep_eff_graphics_t g_battle_gfx_weapon_graphics[2];

u8** battle_gfx_get_effect_seq_data(u32 unused_spritesheet_id);
battle_gfx_weapon_shp_t* battle_gfx_get_effect_shp_data(u32 unused_spritesheet_id);
battle_gfx_unit_seq_table_t* battle_gfx_get_unit_seq_by_spritesheet_id(u32 spritesheet_id);
battle_gfx_unit_shp_frame_tables_t* battle_gfx_get_unit_shp_by_spritesheet_id(u32 spritesheet_id);
u8** battle_gfx_get_weapon_seq_by_spritesheet_id(u32 spritesheet_id);
battle_gfx_weapon_shp_t* battle_gfx_get_weapon_shp_by_spritesheet_id(u32 spritesheet_id);

/* item graphics */
/* Where a weapon or item graphic sits in WEP.SPR, per item graphic id. */
typedef struct battle_gfx_item_graphic {
    u8 palettes;    /* 0x00; g_battle_gfx_weapon_clut rows: high nibble for MAP_CLUT_ROW_ITEM_A, low for _B */
    u8 texture_row; /* 0x01; top of the graphic on its texture page, in steps of 8 pixels */
} battle_gfx_item_graphic_t;

extern battle_gfx_item_graphic_t g_battle_gfx_item_graphic_data[0x92];

/* VRAM slots */
/* Texture pages stored in a sprite display record's tpage. */
enum {
    /* The images of g_battle_gfx_vram_slots[0] and [1]. */
    BATTLE_GFX_TPAGE_EVTCHR_SLOT_0 = 4,
    BATTLE_GFX_TPAGE_EVTCHR_SLOT_1 = 5,
    /* The effect and the weapon sheets of WEP.SPR. */
    BATTLE_GFX_TPAGE_EFFECT = 8,
    BATTLE_GFX_TPAGE_WEAPON = 9,
    /* The sheets from BATTLE_SPRITESHEET_ID_CRYSTAL up. */
    BATTLE_GFX_TPAGE_OTHER = 11,
    /* Plus the unit's spritesheet slot. */
    BATTLE_GFX_TPAGE_SPRITESHEET_SLOT_0 = 0x14,
    BATTLE_GFX_TPAGE_ITEM = 0x1e,  /* ITEM.BIN */
    BATTLE_GFX_TPAGE_FRAME = 0x1f, /* FRAME.BIN */
};

enum {
    BATTLE_SHP_FRAME_EVTCHR_FIRST = 210, /* frame 0 of an EVTCHR slot */
};

/* The SHP frames of an EVTCHR slot, laid out as battle_gfx_unit_shp_frame_tables_t
 * so that frame ids from BATTLE_SHP_FRAME_EVTCHR_FIRST up index it
 * (battle_gfx_select_unit_shp_frame). The file has 0x28 frames; their data
 * is stored twice, once for each frame table, with the same contents. */
typedef struct battle_gfx_evtchr_shp {
    battle_gfx_unit_shp_frame_tables_t tables; /* 0x000; 32-bit header fields remain unset and unread */
    u8 data[0x1e0];                            /* 0x688 */
    u8 submerged_data[0x1e0];                  /* 0x868 */
} battle_gfx_evtchr_shp_t;

/*
 * The two EVTCHR slots: each owns a 256x200 texture page (image_rect) for the
 * pictures that do not fit a unit's spritesheet slot, and a buffer to build
 * them in. A page shows a unit's attack image (decompressed on demand by
 * battle_gfx_load_unit_into_evtchr_slot), a unit's SP2 file, or the image of
 * an EVTCHR.BIN block: the event instruction LoadEVTCHR unpacks a block into
 * seq, shp, cluts and image_data and SaveEVTCHR uploads the image. Every
 * target access uses a 0x7564-byte stride.
 */
typedef struct battle_gfx_vram_slot {
    u32 owner;              /* 0x0000; runtime id with owner flags, or battle_gfx_vram_slot_owner_marker_e marker */
    u32 image_buffer_state; /* 0x0004; upload countdown or battle_gfx_vram_slot_buffer_marker_e marker */
    u32 evtchr_load_state;  /* 0x0008; set to 0 free, 1 a unit's attack image, 3 an EVTCHR block; never read */
    RECT image_rect;        /* 0x000c */
    battle_gfx_evtchr_seq_t seq; /* 0x0014 */
    battle_gfx_evtchr_shp_t shp; /* 0x051c */
    clut_t clut;                 /* 0x0f64; filled by battle_gfx_set_evtchr_palette_by_runtime_id */
    u8 image_data[0x6400];       /* 0x1164 */
} battle_gfx_vram_slot_t;
typedef char battle_gfx_vram_slot_size_must_be_30052[(sizeof(battle_gfx_vram_slot_t) == 30052) ? 1 : -1];

/* Per-slot summary written by battle_gfx_get_vram_slot_summary. */
typedef struct battle_gfx_vram_slot_summary {
    u8 spritesheet_id; /* 0xff free, 0xfe saved, 0xfd no unit, else owner's sheet */
    u8 evtchr_state;   /* 0xff none, 0xfe saved, 0xfd loaded */
} battle_gfx_vram_slot_summary_t;

typedef enum battle_gfx_vram_slot_owner_marker {
    BATTLE_GFX_VRAM_SLOT_OWNER_EVTCHR = 0xfe, /* an EVTCHR image (battle_gfx_save_evtchr_slot) */
    BATTLE_GFX_VRAM_SLOT_OWNER_NONE = 0xff,
} battle_gfx_vram_slot_owner_marker_e;

/* Otherwise owner is a runtime id combined with one of these image-kind bits. */
typedef enum battle_gfx_vram_slot_owner_flags {
    BATTLE_GFX_VRAM_SLOT_OWNER_SP2 = 0x40,
    BATTLE_GFX_VRAM_SLOT_OWNER_ATTACK_IMAGE = 0x80,
} battle_gfx_vram_slot_owner_flags_e;

enum { BATTLE_GFX_VRAM_SLOT_OWNER_ID_MASK = 0x1f };

typedef enum battle_gfx_vram_slot_buffer_marker {
    BATTLE_GFX_VRAM_SLOT_BUFFER_FREE = 0,
    BATTLE_GFX_VRAM_SLOT_BUFFER_EVTCHR = 0xfe, /* an unpacked EVTCHR block, not yet saved to VRAM or cleared */
} battle_gfx_vram_slot_buffer_marker_e;

/* Other positive buffer values are upload countdowns decremented each frame. */
enum { BATTLE_GFX_VRAM_SLOT_UPLOAD_FRAMES = 2 };

/* The nine spritesheet VRAM slots (g_battle_gfx_spritesheet_slots), 0x32d6 bytes each;
 * g_battle_gfx_spritesheet_ids_by_vram_slot aliases the first record's byte 1. */
typedef struct battle_gfx_spritesheet_slot {
    u8 in_use;                          /* 0x00; cleared when no live unit uses the slot */
    u8 spritesheet_id;                  /* 0x01 */
    u8 compressed_attack_image[0x30d4]; /* 0x02; image data copied from SPR data +0x9200 by
                                         * battle_gfx_load_spritesheet_into_vram_slot */
    clut_t clut;                        /* 0x30d6; 16 palettes with the STP bit set */
} battle_gfx_spritesheet_slot_t;
typedef char battle_gfx_spritesheet_slot_size_must_be_13014[(sizeof(battle_gfx_spritesheet_slot_t) == 13014) ? 1 : -1];

/* Each spritesheet slot's 64x32 portrait and lower-body block lies in the VRAM area at
 * (BATTLE_GFX_SPRITESHEET_STRIP_VRAM_X, BATTLE_GFX_SPRITESHEET_STRIP_VRAM_Y): column slot / 8, row
 * slot % 8. */
enum {
    BATTLE_GFX_SPRITESHEET_SLOT_COUNT = 9,
    BATTLE_GFX_SPRITESHEET_STRIP_VRAM_X = 0x340,
    BATTLE_GFX_SPRITESHEET_STRIP_VRAM_Y = 0x100,
    BATTLE_GFX_SPRITESHEET_STRIP_WIDTH = 64,
    BATTLE_GFX_SPRITESHEET_STRIP_HEIGHT = 32,
};

extern u8 g_battle_gfx_spritesheet_ids_by_vram_slot[];
extern battle_gfx_spritesheet_slot_t g_battle_gfx_spritesheet_slots[BATTLE_GFX_SPRITESHEET_SLOT_COUNT];
extern battle_gfx_vram_slot_t g_battle_gfx_vram_slots[];

s32 battle_gfx_claim_spritesheet_slot(s16 spritesheet_id);
s32 battle_gfx_clear_loaded_evtchr_slot(s32 slot_index);
s32 battle_gfx_clear_saved_evtchr_slot(s32 slot_index);
s32 battle_gfx_get_spritesheet_vram_by_battle_id(u32 battle_id);
s32 battle_gfx_get_vram_slot_summary(battle_gfx_vram_slot_summary_t* summary);
s32 battle_gfx_init_evtchr_vram_slots(void);
void battle_gfx_invalidate_sp2_vram_slot(battle_unit_t* unit);
s32 battle_gfx_release_reserved_vram_slot(s32 slot_index);
s32 battle_gfx_save_evtchr_slot(s32 slot_index);
s32 battle_gfx_unpack_evtchr_file_to_vram_slot(s32 index, const evtchr_file_record_t* file);

/* loading */
/* g_battle_gfx_unit_graphics_load_state.phase: the phase of the file being loaded. */
typedef enum battle_gfx_unit_load_phase {
    BATTLE_GFX_UNIT_LOAD_PHASE_START_READ = 0,
    BATTLE_GFX_UNIT_LOAD_PHASE_WAIT = 1,
    BATTLE_GFX_UNIT_LOAD_PHASE_INSTALL = 2,
    BATTLE_GFX_UNIT_LOAD_PHASE_FINISH = 3,
} battle_gfx_unit_load_phase_e;

/* g_battle_gfx_unit_graphics_load_state.file_kind, the file kind, counts through one of two lists:
 * battle_gfx_step_queued_unit_graphics_load loads three files per queued unit and
 * battle_gfx_step_load_unit_shp_seq_data the shared sprite, weapon and effect files. */
typedef enum battle_gfx_queued_load_kind {
    BATTLE_GFX_QUEUED_LOAD_SHP = 0,
    BATTLE_GFX_QUEUED_LOAD_SEQ = 1,
    BATTLE_GFX_QUEUED_LOAD_SPR = 2,
} battle_gfx_queued_load_kind_e;

typedef enum battle_gfx_shared_load_kind {
    BATTLE_GFX_SHARED_LOAD_SPRITE_SHP = 0,
    BATTLE_GFX_SHARED_LOAD_SPRITE_SEQ = 1,
    BATTLE_GFX_SHARED_LOAD_WEAPON_SHP = 2,
    BATTLE_GFX_SHARED_LOAD_WEAPON_SEQ = 3,
    BATTLE_GFX_SHARED_LOAD_EFFECT_SHP = 4,
    BATTLE_GFX_SHARED_LOAD_EFFECT_SEQ = 5,
    BATTLE_GFX_SHARED_LOAD_WEAPON_SPR = 6,
    BATTLE_GFX_SHARED_LOAD_OTHER_SPR = 7,
} battle_gfx_shared_load_kind_e;

/* Both load-kind lists start at zero. */
enum { BATTLE_GFX_UNIT_LOAD_KIND_FIRST = 0 };

/* Shared by the resident-file and queued-unit loaders. The file kind selects
 * the buffer's format, and index selects a file or a queued descriptor. */
typedef struct battle_gfx_unit_graphics_load_state {
    s32 file_kind; /* 0x00; battle_gfx_shared_load_kind_e or battle_gfx_queued_load_kind_e */
    s32 phase;     /* 0x04; battle_gfx_unit_load_phase_e */
    s32 index;     /* 0x08 */
    void* buffer;  /* 0x0c; allocated until the FINISH phase */
} battle_gfx_unit_graphics_load_state_t;
typedef char
    battle_gfx_unit_graphics_load_state_size_must_be_16[sizeof(battle_gfx_unit_graphics_load_state_t) == 16 ? 1 : -1];

extern const u8* g_battle_gfx_compressed_data;
extern s32 g_battle_gfx_compressed_nibble_phase;
extern s32 g_battle_gfx_compressed_offset;
extern u8 g_battle_gfx_fallback_seq_data[];
extern u8 g_battle_gfx_fallback_shp_frame_data[];
extern s32 g_battle_gfx_last_loaded_seq_id;
extern s32 g_battle_gfx_last_loaded_shp_id;
/* Byte-granular bump-allocation cursors: the consuming files do raw byte
 * arithmetic on these, so the element type must stay u8. */
extern u8* g_battle_gfx_seq_data_cursor;
extern u8 g_battle_gfx_shp_frame_data_buffer[];
extern u8* g_battle_gfx_shp_frame_data_cursor;
extern u32* g_battle_gfx_sp2_data;
extern file_extent_t g_battle_gfx_sp2_files[];
extern file_extent_t g_battle_gfx_sprite_seq_files[];
extern file_extent_t g_battle_gfx_sprite_shp_files[];
extern file_extent_t g_battle_gfx_spritesheet_files[];
extern u8 g_battle_gfx_unit_animation_data[];
extern battle_gfx_unit_graphics_load_state_t g_battle_gfx_unit_graphics_load_state;
extern file_extent_t g_battle_gfx_worker8_sp2_files[];

void battle_gfx_queue_unit_graphics_load(s32 map_x, s32 map_y, s32 map_level, s16 facing, s16 spritesheet_id,
    s16 palette_id, s16 runtime_id, unit_stats_t* unit, u32 flags);
void battle_gfx_clear_unit_graphics_load_state(s32 unused_map_id);
void battle_gfx_decompress_attack_spritesheet(const u8* source, u8* destination);
void battle_gfx_init_deployment_and_reset_unit_graphics_state(void);
s32 battle_gfx_load_unit_attack_image_into_evtchr_slot(s32 runtime_id, s32 slot);
s32 battle_gfx_load_unit_graphics_by_battle_id(s32, s32);
s32 battle_gfx_open_sp2_file(battle_unit_t* unit, u32* destination);
/* Drains the immediate loader steps (2); returns 0 when finished or 1 while work remains pending
 * (including an allocation retry). */
s32 battle_gfx_poll_unit_graphics_load(void);
void battle_gfx_reset_unit_graphics_load_state(void);
s32 battle_gfx_step_load_unit_shp_seq_data(void);
/* 0 finished, 1 pending or deferred, 2 immediate progress. */
s32 battle_gfx_step_queued_unit_graphics_load(void);

#endif
