#ifndef SPRITE_SHEET_H
#define SPRITE_SHEET_H

/* BATTLE spritesheets: the SHP, SEQ and SPR file layouts, spritesheet ids, the frame and sequence tables, item
 * graphics, the VRAM slots that hold sheets and EVTCHR images, and the loader that fills them. */

#include "fft/file.h"
#include "fft/gfx.h"
#include "fft/sprite.h"
#include "fft/unit.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* file layouts */
/* Serialized SHP/SEQ values are little-endian byte sequences. These file
 * views have byte alignment; their decoded RAM tables remain separate. */

/* A unit SHP frame set: primary at file +8; an optional submerged set is
 * addressed by the file header. Only the first 0xd0 offsets are installed. */
typedef struct sprite_packed_unit_shp_frame_set {
    u8 frame_offsets[0x100][4]; /* 0x000; signed 32-bit values, -1 resolves to offset zero */
    u8 frame_data_size[2];      /* 0x400; unsigned 16-bit value */
    u8 frame_data[0];           /* 0x402; variable tail */
} sprite_packed_unit_shp_frame_set_t;
typedef char
    sprite_packed_unit_shp_frame_set_size_must_be_1026[sizeof(sprite_packed_unit_shp_frame_set_t) == 1026 ? 1 : -1];

typedef struct sprite_packed_unit_shp_record {
    u8 submerged_frame_set_offset[4];                     /* 0x000; 8 means both native tables share the primary set */
    u8 first_attack_frame[2];                             /* 0x004; unsigned 16-bit value */
    u8 first_sp2_frame[2];                                /* 0x006; unsigned 16-bit value */
    sprite_packed_unit_shp_frame_set_t primary_frame_set; /* 0x008 */
} sprite_packed_unit_shp_record_t;
typedef char sprite_packed_unit_shp_record_size_must_be_1034[sizeof(sprite_packed_unit_shp_record_t) == 1034 ? 1 : -1];

/* WEP/EFF SHP has a different header and twice as many frame offsets. */
typedef struct sprite_packed_weapon_shp_record {
    u8 _unused_000[4];          /* 0x000; loader skips this 32-bit value */
    u8 first_frame[0x20][2];    /* 0x004; unsigned zero-frame indices by weapon type */
    u8 frame_offsets[0x200][4]; /* 0x044; signed 32-bit values, -1 resolves to offset zero */
    u8 frame_data_size[2];      /* 0x844; unsigned 16-bit value */
    u8 frame_data[0];           /* 0x846; variable tail */
} sprite_packed_weapon_shp_record_t;
typedef char
    sprite_packed_weapon_shp_record_size_must_be_2118[sizeof(sprite_packed_weapon_shp_record_t) == 2118 ? 1 : -1];

/* WEP/EFF SEQ stores 256 offsets followed by a variable sequence payload. */
typedef struct sprite_packed_offset_table {
    u8 _unused_000[4];  /* 0x000; WEP/EFF loader skips the 32-bit header value */
    u8 entry[0x100][4]; /* 0x004; signed sequence offsets, -1 resolves to zero */
    u8 payload_size[2]; /* 0x404; unsigned 16-bit value */
    u8 payload[0];      /* 0x406; variable sequence data */
} sprite_packed_offset_table_t;
typedef char sprite_packed_offset_table_size_must_be_1030[sizeof(sprite_packed_offset_table_t) == 1030 ? 1 : -1];

/* OTHER.SPR: the palette helper copies only the first 21 of 32 palettes. */
typedef struct sprite_other_archive {
    palette_t palettes[32]; /* 0x0000 */
    u8 pixels[256 * 128];   /* 0x0400; uncompressed 256x256, 4bpp */
} sprite_other_archive_t;
typedef char sprite_other_archive_size_must_be_33792[sizeof(sprite_other_archive_t) == 33792 ? 1 : -1];

/* WEP.SPR contains three CLUT/image pairs: weapons, trails and particles. */
typedef struct sprite_weapon_archive {
    clut_t weapon_clut;           /* 0x00000 */
    u8 weapon_pixels[256][128];   /* 0x00200; 4bpp */
    clut_t trail_clut;            /* 0x08200 */
    u8 trail_pixels[256][128];    /* 0x08400; 4bpp */
    clut_t particle_clut;         /* 0x10400 */
    u8 particle_pixels[144][128]; /* 0x10600; 4bpp */
} sprite_weapon_archive_t;

/* spritesheets */
/* Spritesheet ids index g_sprite_sheet_data and
 * g_sprite_sheet_files. An id below 0x80 is the character_identity_e
 * value it is loaded for (unit_get_spritesheet_palette returns those
 * unchanged) and has that identity's name; a numbered name loads the file of
 * the sheet with the plain name. From 0x60 the sheets are the generic jobs',
 * male then female: the generic selectors resolve to
 * 0x60 + (job - JOB_ID_SQUIRE) * 2, plus one for a female unit, with Dancer
 * and Mime moved down one job. Then comes one sheet per monster family, named
 * by the first job whose record has it (job_data_t.spritesheet_id), and the
 * four status sheets, which have no file of their own. */
typedef enum sprite_sheet_id {
    SPRITE_SHEET_ID_RAMZA_CHAPTER_1 = 1,
    SPRITE_SHEET_ID_RAMZA_CHAPTER_2_3 = 2,
    SPRITE_SHEET_ID_RAMZA_CHAPTER_4 = 3,
    SPRITE_SHEET_ID_DELITA_CHAPTER_1 = 4,
    SPRITE_SHEET_ID_DELITA_CHAPTER_2_3 = 5,
    SPRITE_SHEET_ID_DELITA_CHAPTER_4 = 6,
    SPRITE_SHEET_ID_ALGUS = 7,
    SPRITE_SHEET_ID_ZALBAG = 8,
    SPRITE_SHEET_ID_DYCEDARG = 9,
    SPRITE_SHEET_ID_LARG = 10,
    SPRITE_SHEET_ID_GOLTANA = 11,
    SPRITE_SHEET_ID_OVELIA = 12,
    SPRITE_SHEET_ID_ORLANDU = 13,
    SPRITE_SHEET_ID_FUNERAL = 14,
    SPRITE_SHEET_ID_REIS_HUMAN = 15,
    SPRITE_SHEET_ID_ZALMO = 16,
    SPRITE_SHEET_ID_GAFGARION_ENEMY = 17,
    SPRITE_SHEET_ID_MALAK_12 = 18,
    SPRITE_SHEET_ID_SIMON = 19,
    SPRITE_SHEET_ID_ALMA_BATTLE = 20,
    SPRITE_SHEET_ID_OLAN = 21,
    SPRITE_SHEET_ID_MUSTADIO_JOIN = 22,
    SPRITE_SHEET_ID_GAFGARION_GUEST = 23,
    SPRITE_SHEET_ID_DRACLAU = 24,
    SPRITE_SHEET_ID_RAFA_GUEST = 25,
    SPRITE_SHEET_ID_MALAK = 26,
    SPRITE_SHEET_ID_ELMDOR = 27,
    SPRITE_SHEET_ID_TETA = 28,
    SPRITE_SHEET_ID_BARINTEN = 29,
    SPRITE_SHEET_ID_AGRIAS_JOIN = 30,
    SPRITE_SHEET_ID_BEOWULF = 31,
    SPRITE_SHEET_ID_WIEGRAF_CHAPTER_1 = 32,
    SPRITE_SHEET_ID_BALMAFULA = 33,
    SPRITE_SHEET_ID_MUSTADIO_GUEST = 34,
    SPRITE_SHEET_ID_RUDVICH = 35,
    SPRITE_SHEET_ID_VORMAV = 36,
    SPRITE_SHEET_ID_ROFEL = 37,
    SPRITE_SHEET_ID_IZLUDE = 38,
    SPRITE_SHEET_ID_KLETIAN = 39,
    SPRITE_SHEET_ID_WIEGRAF_CHAPTER_2_3 = 40,
    SPRITE_SHEET_ID_RAFA_JOIN = 41,
    SPRITE_SHEET_ID_MELIADOUL_JOIN = 42,
    SPRITE_SHEET_ID_BALK = 43,
    SPRITE_SHEET_ID_ALMA_2C = 44,
    SPRITE_SHEET_ID_CELIA = 45,
    SPRITE_SHEET_ID_LEDE = 46,
    SPRITE_SHEET_ID_MELIADOUL_ENEMY = 47,
    SPRITE_SHEET_ID_ALMA_EVENTS = 48,
    SPRITE_SHEET_ID_AJORA = 49,
    SPRITE_SHEET_ID_CLOUD = 50,
    SPRITE_SHEET_ID_ZALBAG_ZOMBIE = 51,
    SPRITE_SHEET_ID_AGRIAS_GUEST = 52,
    SPRITE_SHEET_ID_FEMALE_CHEMIST_35 = 53,
    SPRITE_SHEET_ID_FEMALE_PRIEST_36 = 54,
    SPRITE_SHEET_ID_MALE_WIZARD_37 = 55,
    SPRITE_SHEET_ID_MALE_ORACLE_38 = 56,
    SPRITE_SHEET_ID_MALE_SQUIRE_39 = 57,
    SPRITE_SHEET_ID_CELIA_3A = 58,
    SPRITE_SHEET_ID_LEDE_3B = 59,
    SPRITE_SHEET_ID_VELIUS = 60,
    SPRITE_SHEET_ID_MALE_KNIGHT_3D = 61,
    SPRITE_SHEET_ID_ZALERA = 62,
    SPRITE_SHEET_ID_MALE_ARCHER_3F = 63,
    SPRITE_SHEET_ID_HASHMALUM = 64,
    SPRITE_SHEET_ID_ALTIMA_FIRST_FORM = 65,
    SPRITE_SHEET_ID_MALE_WIZARD_42 = 66,
    SPRITE_SHEET_ID_QUEKLAIN = 67,
    SPRITE_SHEET_ID_FEMALE_TIME_MAGE_44 = 68,
    SPRITE_SHEET_ID_ADRAMELK = 69,
    SPRITE_SHEET_ID_MALE_ORACLE_46 = 70,
    SPRITE_SHEET_ID_FEMALE_SUMMONER_47 = 71,
    SPRITE_SHEET_ID_REIS_DRAGON = 72,
    SPRITE_SHEET_ID_ALTIMA_SECOND_FORM = 73,
    SPRITE_SHEET_ID_BOY = 74,
    SPRITE_SHEET_ID_GIRL = 75,
    SPRITE_SHEET_ID_YOUNG_MAN = 76,
    SPRITE_SHEET_ID_YOUNG_WOMAN = 77,
    SPRITE_SHEET_ID_MIDDLE_AGED_MAN = 78,
    SPRITE_SHEET_ID_MIDDLE_AGED_WOMAN = 79,
    SPRITE_SHEET_ID_OLD_MAN = 80,
    SPRITE_SHEET_ID_OLD_WOMAN = 81,
    SPRITE_SHEET_ID_OLD_FUNERAL_MAN = 82,
    SPRITE_SHEET_ID_OLD_FUNERAL_WOMAN = 83,
    SPRITE_SHEET_ID_FUNERAL_MAN = 84,
    SPRITE_SHEET_ID_FUNERAL_WOMAN = 85,
    SPRITE_SHEET_ID_FUNERAL_PRIEST = 86,
    SPRITE_SHEET_ID_MALE_SQUIRE_57 = 87,
    SPRITE_SHEET_ID_MALE_SQUIRE_58 = 88,
    SPRITE_SHEET_ID_MALE_SQUIRE_59 = 89,
    SPRITE_SHEET_ID_MALE_SQUIRE_5A = 90,
    SPRITE_SHEET_ID_MALE_SQUIRE_5B = 91,
    SPRITE_SHEET_ID_MALE_SQUIRE_5C = 92,
    SPRITE_SHEET_ID_DAMI = 93,    /* named by its file, DAMI.SPR; no ENTD unit has it */
    SPRITE_SHEET_ID_KASANEK = 94, /* named by its file, KASANEK.SPR; no ENTD unit has it */
    SPRITE_SHEET_ID_KASANEM = 95, /* named by its file, KASANEM.SPR; no ENTD unit has it */
    SPRITE_SHEET_ID_MALE_SQUIRE = 96,
    SPRITE_SHEET_ID_FEMALE_SQUIRE = 97,
    SPRITE_SHEET_ID_MALE_CHEMIST = 98,
    SPRITE_SHEET_ID_FEMALE_CHEMIST = 99,
    SPRITE_SHEET_ID_MALE_KNIGHT = 100,
    SPRITE_SHEET_ID_FEMALE_KNIGHT = 101,
    SPRITE_SHEET_ID_MALE_ARCHER = 102,
    SPRITE_SHEET_ID_FEMALE_ARCHER = 103,
    SPRITE_SHEET_ID_MALE_MONK = 104,
    SPRITE_SHEET_ID_FEMALE_MONK = 105,
    SPRITE_SHEET_ID_MALE_PRIEST = 106,
    SPRITE_SHEET_ID_FEMALE_PRIEST = 107,
    SPRITE_SHEET_ID_MALE_WIZARD = 108,
    SPRITE_SHEET_ID_FEMALE_WIZARD = 109,
    SPRITE_SHEET_ID_MALE_TIME_MAGE = 110,
    SPRITE_SHEET_ID_FEMALE_TIME_MAGE = 111,
    SPRITE_SHEET_ID_MALE_SUMMONER = 112,
    SPRITE_SHEET_ID_FEMALE_SUMMONER = 113,
    SPRITE_SHEET_ID_MALE_THIEF = 114,
    SPRITE_SHEET_ID_FEMALE_THIEF = 115,
    SPRITE_SHEET_ID_MALE_MEDIATOR = 116,
    SPRITE_SHEET_ID_FEMALE_MEDIATOR = 117,
    SPRITE_SHEET_ID_MALE_ORACLE = 118,
    SPRITE_SHEET_ID_FEMALE_ORACLE = 119,
    SPRITE_SHEET_ID_MALE_GEOMANCER = 120,
    SPRITE_SHEET_ID_FEMALE_GEOMANCER = 121,
    SPRITE_SHEET_ID_MALE_LANCER = 122,
    SPRITE_SHEET_ID_FEMALE_LANCER = 123,
    SPRITE_SHEET_ID_MALE_SAMURAI = 124,
    SPRITE_SHEET_ID_FEMALE_SAMURAI = 125,
    SPRITE_SHEET_ID_MALE_NINJA = 126,
    SPRITE_SHEET_ID_FEMALE_NINJA = 127,
    SPRITE_SHEET_ID_MALE_CALCULATOR = 128,
    SPRITE_SHEET_ID_FEMALE_CALCULATOR = 129,
    SPRITE_SHEET_ID_BARD = 130,
    SPRITE_SHEET_ID_DANCER = 131,
    SPRITE_SHEET_ID_MALE_MIME = 132,
    SPRITE_SHEET_ID_FEMALE_MIME = 133,
    SPRITE_SHEET_ID_CHOCOBO = 134,
    SPRITE_SHEET_ID_GOBLIN = 135,
    SPRITE_SHEET_ID_BOMB = 136,
    SPRITE_SHEET_ID_RED_PANTHER = 137,
    SPRITE_SHEET_ID_PISCO_DEMON = 138,
    SPRITE_SHEET_ID_SKELETON = 139,
    SPRITE_SHEET_ID_GHOUL = 140,
    SPRITE_SHEET_ID_FLOTIBALL = 141,
    SPRITE_SHEET_ID_JURAVIS = 142,
    SPRITE_SHEET_ID_URIBO = 143,
    SPRITE_SHEET_ID_WOODMAN = 144,
    SPRITE_SHEET_ID_BULL_DEMON = 145,
    SPRITE_SHEET_ID_MORBOL = 146,
    SPRITE_SHEET_ID_BEHEMOTH = 147,
    SPRITE_SHEET_ID_DRAGON = 148, /* also Reis as a Holy Dragon */
    SPRITE_SHEET_ID_HYUDRA = 149,
    SPRITE_SHEET_ID_BYBLOS = 150, /* and Apanda */
    SPRITE_SHEET_ID_SERPENTARIUS = 151,
    SPRITE_SHEET_ID_DRAGON_98 = 152,     /* loads the Dragon file again; no job has it */
    SPRITE_SHEET_ID_ARCHAIC_DEMON = 153, /* and Ultima Demon */
    SPRITE_SHEET_ID_STEEL_GIANT = 154,
    SPRITE_SHEET_ID_CRYSTAL = 155,
    SPRITE_SHEET_ID_CHICKEN = 156,
    SPRITE_SHEET_ID_FROG = 157,
    SPRITE_SHEET_ID_TREASURE = 158,
} sprite_sheet_id_e;

enum {
    SPRITE_SHEET_ID_RAMZA_END = 4,
    SPRITE_SHEET_ID_JOB_FIRST = 96, /* the generic job sheets start here; the monsters' follow */
    SPRITE_SHEET_ID_COUNT = 159,
};

/* Four-byte lookup record for the 0x9f ordinary unit spritesheets. */
typedef struct sprite_sheet_data {
    u8 shp_id;         /* sprite_shp_type_e */
    u8 seq_id;         /* sprite_seq_type_e */
    u8 flying_flag;    /* nonzero: the sheet idles with the ANIMATION_IDLE_FLYING animations */
    u8 graphic_height; /* height of the standing sprite in pixels */
} sprite_sheet_data_t;
typedef char sprite_sheet_data_size_must_be_4[(sizeof(sprite_sheet_data_t) == 4) ? 1 : -1];

/* sprite_sheet_data_t.shp_id: index of the sheet's frame table file in
 * g_sprite_shp_files (BATTLE/TYPE1.SHP ...). Id 5 has no file. */
typedef enum sprite_shp_type {
    SPRITE_SHP_TYPE1 = 0,
    SPRITE_SHP_TYPE2 = 1,
    SPRITE_SHP_CYOKO = 2,
    SPRITE_SHP_MON = 3,
    SPRITE_SHP_OTHER = 4,
    SPRITE_SHP_ARUTE = 6,
    SPRITE_SHP_KANZEN = 7,
} sprite_shp_type_e;

/* sprite_sheet_data_t.seq_id: index of the sheet's animation file in
 * g_sprite_seq_files (BATTLE/TYPE1.SEQ ...). */
typedef enum sprite_seq_type {
    SPRITE_SEQ_TYPE1 = 0,
    SPRITE_SEQ_TYPE3 = 1,
    SPRITE_SEQ_CYOKO = 2,
    SPRITE_SEQ_MON = 3,
    SPRITE_SEQ_OTHER = 4,
    SPRITE_SEQ_RUKA = 5,
    SPRITE_SEQ_ARUTE = 6,
    SPRITE_SEQ_KANZEN = 7,
} sprite_seq_type_e;

extern sprite_sheet_data_t g_sprite_sheet_data[SPRITE_SHEET_ID_COUNT];

s32 sprite_sheet_is_loaded(s16 spritesheet_id);
u32 sprite_sheet_get_flying_flag(u32 spritesheet_id);
s32 sprite_get_unit_spritesheet_height_by_runtime_id(u32 runtime_id);
s32 sprite_get_unit_spritesheet_height_by_battle_id(u32 battle_id);

/* frame and sequence tables */
extern sprite_unit_shp_frame_tables_t g_sprite_unit_shp_frame_tables[5];
extern sprite_native_unit_seq_table_t g_sprite_unit_seq_tables[5];
extern sprite_unit_shp_frame_tables_t g_sprite_boss_shp_frame_tables;
extern sprite_native_unit_seq_table_t g_sprite_boss_seq_tables;
extern sprite_wep_eff_graphics_t g_sprite_weapon_graphics[2];
extern sprite_wep_eff_graphics_t g_sprite_effect_graphics[1];

/* Sprite part width/height in 8-pixel tiles, indexed by SHP size bits. */
extern size32_t g_sprite_part_sizes[];

sprite_unit_shp_frame_tables_t* sprite_sheet_get_unit_shp(u32 spritesheet_id);
sprite_unit_seq_table_t* sprite_sheet_get_unit_seq(u32 spritesheet_id);
sprite_weapon_shp_t* sprite_sheet_get_weapon_shp(u32 spritesheet_id);
u8** sprite_sheet_get_weapon_seq(u32 spritesheet_id);
sprite_weapon_shp_t* sprite_sheet_get_effect_shp(u32 unused_spritesheet_id);
u8** sprite_sheet_get_effect_seq(u32 unused_spritesheet_id);

/* item graphics */
/* Where a weapon or item graphic sits in WEP.SPR, per item graphic id. */
typedef struct sprite_item_graphic {
    u8 palettes;    /* 0x00; g_sprite_weapon_clut rows: high nibble for BATTLE_MAP_CLUT_ROW_ITEM_A, low for _B */
    u8 texture_row; /* 0x01; top of the graphic on its texture page, in steps of 8 pixels */
} sprite_item_graphic_t;

extern sprite_item_graphic_t g_sprite_item_graphic_data[0x92];

/* VRAM slots */
/* Texture pages stored in a sprite display record's tpage. */
enum {
    /* The images of g_sprite_evtchr_slots[0] and [1]. */
    SPRITE_TPAGE_EVTCHR_SLOT_0 = 4,
    SPRITE_TPAGE_EVTCHR_SLOT_1 = 5,
    /* The effect and the weapon sheets of WEP.SPR. */
    SPRITE_TPAGE_EFFECT = 8,
    SPRITE_TPAGE_WEAPON = 9,
    /* The sheets from SPRITE_SHEET_ID_CRYSTAL up. */
    SPRITE_TPAGE_OTHER = 11,
    /* Plus the unit's spritesheet slot. */
    SPRITE_TPAGE_SPRITESHEET_SLOT_0 = 0x14,
    SPRITE_TPAGE_ITEM = 0x1e,  /* ITEM.BIN */
    SPRITE_TPAGE_FRAME = 0x1f, /* FRAME.BIN */
};

/* The nine spritesheet VRAM slots (g_sprite_sheet_slots), 0x32d6 bytes each;
 * g_sprite_sheet_ids_by_vram_slot aliases the first record's byte 1. */
typedef struct sprite_sheet_slot {
    u8 in_use;                          /* 0x00; cleared when no live unit uses the slot */
    u8 spritesheet_id;                  /* 0x01 */
    u8 compressed_attack_image[0x30d4]; /* 0x02; image data copied from SPR data +0x9200 by
                                         * sprite_sheet_load_into_vram_slot */
    clut_t clut;                        /* 0x30d6; 16 palettes with the STP bit set */
} sprite_sheet_slot_t;
typedef char sprite_sheet_slot_size_must_be_13014[(sizeof(sprite_sheet_slot_t) == 13014) ? 1 : -1];

/* Each spritesheet slot's 64x32 portrait and lower-body block lies in the VRAM area at
 * (SPRITE_SHEET_STRIP_VRAM_X, SPRITE_SHEET_STRIP_VRAM_Y): column slot / 8, row
 * slot % 8. */
enum {
    SPRITE_SHEET_SLOT_COUNT = 9,
    SPRITE_SHEET_STRIP_VRAM_X = 0x340,
    SPRITE_SHEET_STRIP_VRAM_Y = 0x100,
    SPRITE_SHEET_STRIP_WIDTH = 64,
    SPRITE_SHEET_STRIP_HEIGHT = 32,
};

extern sprite_sheet_slot_t g_sprite_sheet_slots[SPRITE_SHEET_SLOT_COUNT];
extern u8 g_sprite_sheet_ids_by_vram_slot[];

s32 sprite_sheet_claim_slot(s16 spritesheet_id);
s32 sprite_sheet_get_slot_by_battle_id(u32 battle_id);

/* EVTCHR slots */
enum {
    SPRITE_SHP_FRAME_EVTCHR_FIRST = 210, /* frame 0 of an EVTCHR slot */
};

/* The SHP frames of an EVTCHR slot, laid out as sprite_unit_shp_frame_tables_t
 * so that frame ids from SPRITE_SHP_FRAME_EVTCHR_FIRST up index it
 * (sprite_frame_select_unit_shp). The file has 0x28 frames; their data
 * is stored twice, once for each frame table, with the same contents. */
typedef struct sprite_evtchr_shp {
    sprite_unit_shp_frame_tables_t tables; /* 0x000; 32-bit header fields remain unset and unread */
    u8 data[0x1e0];                        /* 0x688 */
    u8 submerged_data[0x1e0];              /* 0x868 */
} sprite_evtchr_shp_t;

/*
 * The two EVTCHR slots: each owns a 256x200 texture page (image_rect) for the
 * pictures that do not fit a unit's spritesheet slot, and a buffer to build
 * them in. A page shows a unit's attack image (decompressed on demand by
 * sprite_evtchr_load_unit), a unit's SP2 file, or the image of
 * an EVTCHR.BIN block: the event instruction LoadEVTCHR unpacks a block into
 * seq, shp, cluts and image_data and SaveEVTCHR uploads the image. Every
 * target access uses a 0x7564-byte stride.
 */
typedef struct sprite_evtchr_slot {
    u32 owner;               /* 0x0000; runtime id with owner flags, or sprite_evtchr_slot_owner_marker_e marker */
    u32 image_buffer_state;  /* 0x0004; upload countdown or sprite_evtchr_slot_buffer_marker_e marker */
    u32 evtchr_load_state;   /* 0x0008; set to 0 free, 1 a unit's attack image, 3 an EVTCHR block; never read */
    RECT image_rect;         /* 0x000c */
    sprite_evtchr_seq_t seq; /* 0x0014 */
    sprite_evtchr_shp_t shp; /* 0x051c */
    clut_t clut;             /* 0x0f64; filled by sprite_evtchr_set_palette_by_runtime_id */
    u8 image_data[0x6400];   /* 0x1164 */
} sprite_evtchr_slot_t;
typedef char sprite_evtchr_slot_size_must_be_30052[(sizeof(sprite_evtchr_slot_t) == 30052) ? 1 : -1];

typedef enum sprite_evtchr_slot_owner_marker {
    SPRITE_EVTCHR_SLOT_OWNER_EVTCHR = 0xfe, /* an EVTCHR image (sprite_evtchr_save_slot) */
    SPRITE_EVTCHR_SLOT_OWNER_NONE = 0xff,
} sprite_evtchr_slot_owner_marker_e;

/* Otherwise owner is a runtime id combined with one of these image-kind bits. */
typedef enum sprite_evtchr_slot_owner_flags {
    SPRITE_EVTCHR_SLOT_OWNER_SP2 = 0x40,
    SPRITE_EVTCHR_SLOT_OWNER_ATTACK_IMAGE = 0x80,
} sprite_evtchr_slot_owner_flags_e;

enum { SPRITE_EVTCHR_SLOT_OWNER_ID_MASK = 0x1f };

typedef enum sprite_evtchr_slot_buffer_marker {
    SPRITE_EVTCHR_SLOT_BUFFER_FREE = 0,
    SPRITE_EVTCHR_SLOT_BUFFER_EVTCHR = 0xfe, /* an unpacked EVTCHR block, not yet saved to VRAM or cleared */
} sprite_evtchr_slot_buffer_marker_e;

/* Other positive buffer values are upload countdowns decremented each frame. */
enum { SPRITE_EVTCHR_SLOT_UPLOAD_FRAMES = 2 };

/* Per-slot summary written by sprite_evtchr_get_slot_summary. */
typedef struct sprite_evtchr_slot_summary {
    u8 spritesheet_id; /* 0xff free, 0xfe saved, 0xfd no unit, else owner's sheet */
    u8 evtchr_state;   /* 0xff none, 0xfe saved, 0xfd loaded */
} sprite_evtchr_slot_summary_t;

extern sprite_evtchr_slot_t g_sprite_evtchr_slots[];

s32 sprite_evtchr_init_slots(void);
s32 sprite_evtchr_unpack_to_slot(s32 index, const evtchr_file_record_t* file);
s32 sprite_evtchr_save_slot(s32 slot_index);
s32 sprite_evtchr_clear_loaded_slot(s32 slot_index);
s32 sprite_evtchr_clear_saved_slot(s32 slot_index);
s32 sprite_evtchr_release_reserved_slot(s32 slot_index);
s32 sprite_evtchr_get_slot_summary(sprite_evtchr_slot_summary_t* summary);
void sprite_evtchr_invalidate_sp2(unit_t* unit);

/* loading */
/* g_sprite_load_state.phase: the phase of the file being loaded. */
typedef enum sprite_load_phase {
    SPRITE_LOAD_PHASE_START_READ = 0,
    SPRITE_LOAD_PHASE_WAIT = 1,
    SPRITE_LOAD_PHASE_INSTALL = 2,
    SPRITE_LOAD_PHASE_FINISH = 3,
} sprite_load_phase_e;

/* g_sprite_load_state.file_kind, the file kind, counts through one of two lists:
 * sprite_load_step_queued loads three files per queued unit and
 * sprite_load_step_shp_seq_data the shared sprite, weapon and effect files. */
typedef enum sprite_load_queued_kind {
    SPRITE_LOAD_QUEUED_SHP = 0,
    SPRITE_LOAD_QUEUED_SEQ = 1,
    SPRITE_LOAD_QUEUED_SPR = 2,
} sprite_load_queued_kind_e;

typedef enum sprite_load_shared_kind {
    SPRITE_LOAD_SHARED_SPRITE_SHP = 0,
    SPRITE_LOAD_SHARED_SPRITE_SEQ = 1,
    SPRITE_LOAD_SHARED_WEAPON_SHP = 2,
    SPRITE_LOAD_SHARED_WEAPON_SEQ = 3,
    SPRITE_LOAD_SHARED_EFFECT_SHP = 4,
    SPRITE_LOAD_SHARED_EFFECT_SEQ = 5,
    SPRITE_LOAD_SHARED_WEAPON_SPR = 6,
    SPRITE_LOAD_SHARED_OTHER_SPR = 7,
} sprite_load_shared_kind_e;

/* Both load-kind lists start at zero. */
enum { SPRITE_LOAD_KIND_FIRST = 0 };

/* Shared by the resident-file and queued-unit loaders. The file kind selects
 * the buffer's format, and index selects a file or a queued descriptor. */
typedef struct sprite_load_state {
    s32 file_kind; /* 0x00; sprite_load_shared_kind_e or sprite_load_queued_kind_e */
    s32 phase;     /* 0x04; sprite_load_phase_e */
    s32 index;     /* 0x08 */
    void* buffer;  /* 0x0c; allocated until the FINISH phase */
} sprite_load_state_t;
typedef char sprite_load_state_size_must_be_16[sizeof(sprite_load_state_t) == 16 ? 1 : -1];

extern sprite_load_state_t g_sprite_load_state;
extern file_extent_t g_sprite_sp2_files[];
extern file_extent_t g_sprite_worker8_sp2_files[];
extern u32* g_sprite_sp2_data;
extern s32 g_sprite_last_loaded_shp_id;
extern s32 g_sprite_last_loaded_seq_id;
extern u8 g_sprite_shp_frame_data_buffer[];
extern u8 g_sprite_seq_data_buffer[];
extern u8 g_sprite_fallback_shp_frame_data[];
extern u8 g_sprite_fallback_seq_data[];

/* Byte-granular bump-allocation cursors: the consuming files do raw byte arithmetic on these, so the element type
 * must stay u8. */
extern u8* g_sprite_shp_frame_data_cursor;
extern u8* g_sprite_seq_data_cursor;

void sprite_load_reset_state(void);
void sprite_load_clear_state(s32 unused_map_id);
void sprite_queue_unit_graphics_load(s32 map_x, s32 map_y, s32 map_level, s16 facing, s16 spritesheet_id,
    s16 palette_id, s16 runtime_id, unit_stats_t* unit, u32 flags);
s32 sprite_load_unit_graphics_by_battle_id(s32, s32);

/* Drains the immediate loader steps (2); returns 0 when finished or 1 while work remains pending
 * (including an allocation retry). */
s32 sprite_poll_unit_graphics_load(void);
s32 sprite_load_step_shp_seq_data(void);

/* 0 finished, 1 pending or deferred, 2 immediate progress. */
s32 sprite_load_step_queued(void);
s32 sprite_open_sp2_file(unit_t* unit, u32* destination);
void sprite_decompress_attack_image(const u8* source, u8* destination);
s32 sprite_evtchr_load_unit_attack_image(s32 runtime_id, s32 slot);

extern s32 g_sprite_unit_graphics_load_complete;

#endif
