#ifndef SPRITE_H
#define SPRITE_H

/* Unit sprites: sprite sheets, EVTCHR slots, palettes, shadows and afterimages. */

#include "fft/gfx.h"
#include "fft/unit.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

struct animation_state;

/* Unit SPR file: 16 palettes, two uncompressed 4bpp image blocks, then
 * a variable-length compressed attack-image stream. */
typedef struct sprite_unit_file {
    clut_t clut;                          /* 0x0000; 16 palettes of 16 colors */
    u8 pixels[256][128];                  /* 0x0200; 256 x 256 pixels, two per byte */
    u8 portrait_and_body_pixels[32][128]; /* 0x8200; 256 x 32 pixels, two per byte */
    u8 compressed_attack_image[0];        /* 0x9200; variable byte tail */
} sprite_unit_file_t;

typedef char sprite_unit_file_size_must_be_37376[sizeof(sprite_unit_file_t) == 37376 ? 1 : -1];

/* Packed unit SEQ file. Values and offsets are little-endian byte
 * sequences, so this view needs no native 16-bit or 32-bit alignment. */
typedef struct sprite_packed_seq_record {
    u8 first_attack_sequence[2];   /* 0x000; unsigned little-endian 16-bit value */
    u8 first_sp2_sequence[2];      /* 0x002; unsigned little-endian 16-bit value */
    u8 sequence_offsets[0x100][4]; /* 0x004; signed little-endian 32-bit values, -1 means offset zero */
    u8 sequence_data_size[2];      /* 0x404; unsigned little-endian 16-bit value */
    u8 sequence_data[0];           /* 0x406; variable byte tail */
} sprite_packed_seq_record_t;

typedef char sprite_packed_seq_record_size_must_be_1030[sizeof(sprite_packed_seq_record_t) == 1030 ? 1 : -1];

/* SHP frames */
/* One part of an SHP frame: a rectangle of the sheet and where it is drawn. */
typedef struct sprite_frame_part {
    s8 x_shift; /* pixels from the sprite's origin to the part's left and top edges */
    s8 y_shift;
    u16 attributes; /* SPRITE_SHP_PART_* masks */
} sprite_frame_part_t;

/* Encoded fields of sprite_frame_part_t.attributes. */
enum {
    SPRITE_SHP_PART_TILE_MASK = 0x03ff, /* the picture's place in the sheet in 8-pixel tiles: bits 0-4 u, 5-9 v */
    SPRITE_SHP_PART_SIZE_MASK = 0x3c00, /* bits 10-13: index of g_sprite_part_sizes */
    SPRITE_SHP_PART_FLIP_MASK = 0xc000, /* shifted down 13: SPRITE_DISPLAY_HORIZONTAL_FLIP and _VERTICAL_FLIP */
};

/* The part size that a unit sheet draws as two 48x24 halves, the upper one nearer the camera. */
enum {
    SPRITE_SHP_PART_SIZE_48X48 = 14,
};

/* One frame of an SHP file: a 2-byte header followed by 4-byte parts
 * (sprite_frame_load_unit). */
typedef struct sprite_frame {
    /* Bits 0-2 are the part count minus one, bits 3-7 index the rotation
     * table g_sprite_y_rotations. */
    u8 part_count_and_rotation;
    u8 flags; /* the SPRITE_DISPLAY_SEMI_TRANSPARENT and _BLEND_MASK bits of every part */
    sprite_frame_part_t parts[1];
} sprite_frame_t;

/* Unit SHP frame tables filled by sprite_unpack_unit_shp_data:
 * SHP header bytes 4..5 and 6..7, then two 0xd0-entry frame pointer tables,
 * each entry a sprite_frame_t. The second table draws the same
 * frames without the legs, for a unit standing in water or riding
 * (sprite_frame_select_unit_shp); it repeats the first when the file
 * carries a single frame set (header 8). */
typedef struct sprite_unit_shp_frame_tables {
    s32 first_attack_frame;                 /* 0x000; frames from here up are drawn from the sheet's attack image */
    s32 first_sp2_frame;                    /* 0x004; set from the file and never read */
    sprite_frame_t* frames[0xd0];           /* 0x008 */
    sprite_frame_t* submerged_frames[0xd0]; /* 0x348 */
} sprite_unit_shp_frame_tables_t;

typedef char sprite_unit_shp_frame_tables_size_must_be_1672[sizeof(sprite_unit_shp_frame_tables_t) == 1672 ? 1 : -1];

/* WEP/EFF SHP data: the first frame of each weapon type, then the frame
 * pointers from 0x40. */
typedef struct sprite_weapon_shp {
    u16 first_frame[0x20];         /* 0x000; indexed by weapon type */
    sprite_frame_t* frames[0x200]; /* 0x040 */
} sprite_weapon_shp_t;

typedef char sprite_weapon_shp_size_must_be_2112[sizeof(sprite_weapon_shp_t) == 2112 ? 1 : -1];

/* Common header of an unpacked unit SEQ table. The pointer tail has 0x100
 * entries in a native table and 0x40 in an EVTCHR slot. */
typedef struct sprite_unit_seq_table {
    s32 first_attack_sequence; /* 0x00; sequences from here use the attack image */
    s32 first_sp2_sequence;    /* 0x04; sequences from here use the SP2 file */
    u8* sequences[0];          /* 0x08; variable pointer tail */
} sprite_unit_seq_table_t;

typedef char sprite_unit_seq_table_size_must_be_8[sizeof(sprite_unit_seq_table_t) == 8 ? 1 : -1];

/* Native table storage: five resident tables and the separate boss table.
 * Their sequences live in the shared sequence-data buffer. */
typedef struct sprite_native_unit_seq_table {
    sprite_unit_seq_table_t header; /* 0x000 */
    u8* sequences[0x100];           /* 0x008 */
} sprite_native_unit_seq_table_t;

typedef char sprite_native_unit_seq_table_size_must_be_1032[sizeof(sprite_native_unit_seq_table_t) == 1032 ? 1 : -1];

/* An EVTCHR slot's fixed SEQ storage. Its 32-bit header fields are unset and unread;
 * its 0x40 pointers refer into the slot's own 0x400 bytes. */
typedef struct sprite_evtchr_seq {
    sprite_unit_seq_table_t header; /* 0x000 */
    u8* sequences[0x40];            /* 0x008 */
    u8 sequence_data[0x400];        /* 0x108 */
} sprite_evtchr_seq_t;

typedef char sprite_evtchr_seq_size_must_be_1288[sizeof(sprite_evtchr_seq_t) == 1288 ? 1 : -1];

/* Native WEP/EFF storage pairs the SHP frame table with its SEQ pointers. */
typedef struct sprite_wep_eff_graphics {
    sprite_weapon_shp_t shp; /* 0x000 */
    u8* seq[0x100];          /* 0x840; sequence command streams */
} sprite_wep_eff_graphics_t;

typedef char sprite_wep_eff_graphics_size_must_be_3136[sizeof(sprite_wep_eff_graphics_t) == 3136 ? 1 : -1];

/* How sprite_display_construct_polygon_data draws a sprite part. The
 * unit's sprite_display_flags, its horizontal_flip_flag, the flags a
 * sequence toggles and each part's own are XORed together. */
typedef enum sprite_display_flags {
    SPRITE_DISPLAY_SEMI_TRANSPARENT = 0x01,
    SPRITE_DISPLAY_HORIZONTAL_FLIP = 0x02,
    SPRITE_DISPLAY_VERTICAL_FLIP = 0x04,
    /* A part's flags only: queued nearer the camera than the rest by its pitch and zoom. */
    SPRITE_DISPLAY_NEARER = 0x80,
} sprite_display_flags_e;

/* Two-bit selector in sprite flags and tpage, already shifted to bits 5-6. */
typedef enum sprite_display_blend_mode {
    SPRITE_DISPLAY_BLEND_AVERAGE = 0x00,
    SPRITE_DISPLAY_BLEND_ADD = 0x20,
    SPRITE_DISPLAY_BLEND_SUBTRACT = 0x40,
    SPRITE_DISPLAY_BLEND_ADD_QUARTER = 0x60,
} sprite_display_blend_mode_e;

enum { SPRITE_DISPLAY_BLEND_MASK = 0x60 };

/* One part of a sprite display: a rectangle of the display's texture page,
 * in pixels, and its place relative to the sprite's origin. */
typedef struct sprite_display_part {
    s8 x_shift;
    s8 y_shift;
    u8 width;
    u8 height;
    u8 u;
    u8 v;
    u8 flags; /* sprite_display_flags_e and sprite_display_blend_mode_e */
} sprite_display_part_t;

typedef char sprite_display_part_size_must_be_7[(sizeof(sprite_display_part_t) == 7) ? 1 : -1];

/* A sprite display: texture page, CLUT, color and the parts cut from it.
 * Renderer APIs use this common header and variable part tail; the fixed
 * storage records below describe each slot inside a 0x440-byte runtime record. */
typedef struct sprite_display {
    rgb24_t color; /* 0x00 */
    u8 part_count; /* 0x03; parts in use */
    s16 tpage;     /* 0x04; SPRITE_TPAGE_*, with sprite_display_blend_mode_e */
    u16 clut;      /* 0x06 */
    u16 scale_x;   /* 0x08; ONE is neutral scale */
    u16 scale_y;   /* 0x0a; ONE is neutral scale */
    /* 0x0c; turns the sprite about its origin, ONE to the turn. Each frame
     * sets it from its header; ANIMATION_SEQ_OP_SET_ROTATION and _CLEAR_ROTATION
     * then replace it. */
    u16 rotation;
    sprite_display_part_t parts[0]; /* 0x0e; variable tail */
} sprite_display_t;

typedef char sprite_display_size_must_be_14[sizeof(sprite_display_t) == 14 ? 1 : -1];

typedef struct sprite_unit_display {
    sprite_display_t header;        /* 0x00 */
    sprite_display_part_t parts[8]; /* 0x0e */
} sprite_unit_display_t;

typedef char sprite_unit_display_size_must_be_70[sizeof(sprite_unit_display_t) == 70 ? 1 : -1];

typedef struct sprite_weapon_display {
    sprite_display_t header;        /* 0x00 */
    sprite_display_part_t parts[3]; /* 0x0e */
    u8 _padding_23;                 /* aligns the following display */
} sprite_weapon_display_t;

typedef char sprite_weapon_display_size_must_be_36[sizeof(sprite_weapon_display_t) == 36 ? 1 : -1];

typedef struct sprite_single_part_display {
    sprite_display_t header;        /* 0x00 */
    sprite_display_part_t parts[1]; /* 0x0e */
    u8 _padding_15;                 /* aligns the following display */
} sprite_single_part_display_t;

typedef char sprite_single_part_display_size_must_be_22[sizeof(sprite_single_part_display_t) == 22 ? 1 : -1];

/* Sprite lookup result filled in by sprite_get_unit_frame_and_vram_ids. */
typedef struct sprite_unit_query {
    s16 clut;             /* 0x00; the unit's CLUT of BATTLE_MAP_CLUT_ROW_UNITS */
    s16 tpage;            /* 0x02 */
    s16 graphic_height;   /* 0x04 */
    s16 graphic_y_offset; /* 0x06 */
} sprite_unit_query_t;

/* graphic_type of a unit's sprite blocks: which sheet the block animates. */
typedef enum sprite_graphic_type {
    SPRITE_GRAPHIC_UNIT = 0,
    SPRITE_GRAPHIC_WEAPON = 1, /* WEP sheets */
    SPRITE_GRAPHIC_EFFECT = 2, /* EFF sheets */
} sprite_graphic_type_e;

/* Weapon/effect sprite animation block, runtime unit data 0x208, 0x238 and
 * 0x268: block 0 plays the weapon sheet's sequences, blocks 1 and 2 the
 * effect sheet's. It shares the first 0x1c bytes of animation_state_t,
 * which has their meanings, but keeps its display pointer at 0x24
 * (animation_run_wep_eff_seq_script, sprite_draw_unit_layers,
 * unit_init). Its SEQ table has no header: seq[sequence_id]
 * is the sequence. */
typedef struct sprite_block {
    u16 trigger;               /* 0x00; nonzero while the sprite is drawn */
    u16 graphic_type;          /* 0x02; sprite_graphic_type_e */
    u16 sequence_id;           /* 0x04 */
    u16 script_pos;            /* 0x06 */
    u16 frame;                 /* 0x08; includes the weapon type's first frame */
    u16 wait;                  /* 0x0a */
    u16 restart_count;         /* 0x0c */
    u16 saved_sequence_id;     /* 0x0e */
    u16 saved_script_pos;      /* 0x10 */
    u16 wait_bias;             /* 0x12 */
    u16 frame_bias;            /* 0x14 */
    u16 repeat_count;          /* 0x16 */
    u16 flags;                 /* 0x18 */
    u8 _padding_1a[2];         /* 0x1a; aligns shp */
    sprite_weapon_shp_t* shp;  /* 0x1c */
    u8** seq;                  /* 0x20 */
    sprite_display_t* display; /* 0x24 */
    u8 _unused_28[8];          /* 0x28 */
} sprite_block_t;

typedef char sprite_block_size_must_be_48[(sizeof(sprite_block_t) == 48) ? 1 : -1];
extern s32 g_sprite_unit_graphics_load_complete;
void sprite_set_horizontal_flip_flag(u32 runtime_id);
void sprite_clear_horizontal_flip_flag(u32 runtime_id);

/* Cleared with the sprite counters at render init; nothing reads it. */
extern s16 g_sprite_unread_1;

/* Cleared with the sprite counters at render init; nothing reads it. */
extern s16 g_sprite_unread_2;

/* g_sprite_prim_pool is a ring of 256 POLY_FT4; g_sprite_prim_index is the index of
 * the last one taken, and g_sprite_prim_frame_start_index its value when the frame began. */
extern s16 g_sprite_prim_index;

extern s16 g_sprite_prim_frame_start_index;
extern POLY_FT4* g_sprite_prim_pool;
s16 sprite_take_prim(void);

void sprite_display_construct_polygon_data(
    sprite_display_t* display, s32 end, s32 start, SVECTOR* position, s16 angle, s16 mode, SVECTOR* scale, u32* ot);

sprite_display_t* sprite_display_init_item(s32 runtime_id);
sprite_display_t* sprite_display_init_unit(s32 runtime_id);
sprite_display_t* sprite_display_init_weapon(s32 unit_index, s32 display_type);
void sprite_display_release_unit_stub(sprite_display_t* unused_display);
void sprite_display_set_part_flip(sprite_display_t* display, s32 part_index, u32 flags);
void sprite_display_set_part_semi_transparent(sprite_display_t* display, s32 part_index, s32 enabled);

void sprite_display_store_data(
    sprite_display_t* display, s32 index, s16 x_shift, s16 y_shift, s16 u, s16 v, s16 width, s16 height, s16 flags);

/* The display rotations that the upper five bits of a frame's first byte select, ONE to the turn. */
extern u16 g_sprite_y_rotations[];

void sprite_sheet_apply_status_change(unit_t* unit, s32 tile_effect_level);
void sprite_afterimage_draw(unit_t* unit);
sprite_frame_t* sprite_get_unit_frame_and_vram_ids(u32 runtime_id, sprite_unit_query_t* out);
void sprite_draw_item_display(struct unit* unit);
void sprite_afterimage_init(struct unit* unit);
void sprite_frame_load_unit(unit_t* unit, struct animation_state* state, u16 frame, u16 sequence_id);
void sprite_afterimage_shift_and_fill(struct unit* unit);
void sprite_display_store_item(struct unit* unit, u32 item_id);
void sprite_update_transparency_flag(struct unit* unit);
void sprite_shadow_build_quad(unit_t* unit);
void sprite_shadow_draw(unit_t* unit, u32* ot);
s32 sprite_shadow_hide_by_runtime_id(u32 runtime_id);
s32 sprite_shadow_show_by_runtime_id(u32 runtime_id);
void sprite_project_all_units(void);

/* First 21 OTHER.SPR palettes copied together; the other three groups also
 * retain their independently loaded symbol aliases. */
typedef struct sprite_other_palettes {
    palette_t chicken[5];  /* 0x000 */
    palette_t frog[5];     /* 0x0a0 */
    palette_t crystal[6];  /* 0x140 */
    palette_t treasure[5]; /* 0x200 */
} sprite_other_palettes_t;

typedef char sprite_other_palettes_size_must_be_672[sizeof(sprite_other_palettes_t) == 672 ? 1 : -1];
extern palette_t g_sprite_crystal_palettes[6];
extern clut_t g_sprite_trail_clut;
extern palette_t g_sprite_frog_palettes[5];
extern clut_t g_sprite_weapon_clut; /* the first 0x200 bytes of WEP.SPR */
extern sprite_other_palettes_t g_sprite_other_palettes;
extern palette_t g_sprite_shadow_palette;
extern palette_t g_sprite_treasure_palettes[5];
void sprite_palette_reset_by_runtime_id(u32 runtime_id);
void sprite_palette_check_tile_status_mod(struct unit* unit, s32 tile_effect_level, s32 final_value);
void sprite_palette_tint_by_team(struct unit* unit, s32 amount);
void sprite_evtchr_set_palette_by_runtime_id(s32 runtime_id, s32 slot_index, s32 clut_index);
void sprite_palette_set_thrown_item(s32 item_id, unit_t* unit);
void sprite_palette_set_thrown_item_by_runtime_id(u32 item_id, u32 runtime_id);
void sprite_palette_apply_modulation_to_units(s32 mode, s32 amount, s32 runtime_id, s16 red, s16 green, s16 blue);
void sprite_palette_tint_all_by_team(void);
void sprite_palette_tint_unit(unit_t* unit, s32 level, s32 final_value);
void sprite_palette_restore_all(void);
extern MATRIX g_sprite_display_matrix;
sprite_display_t* sprite_display_init_numeric(s32 unit_index, s32 display_section);
sprite_display_t* sprite_display_init_status_bubble(s32 unit_index);
void sprite_update_status_bubbles_and_graphics(void);

#endif
