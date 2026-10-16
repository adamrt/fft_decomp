#ifndef BATTLE_UNIT_SPRITE_H
#define BATTLE_UNIT_SPRITE_H

/* BATTLE unit sprites: SPR, SHP and SEQ sheet formats, sequence opcodes, sprite displays and animation blocks. */

#include "fft/gfx.h"
#include "psx/types.h"

/* sheet files */
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

/* SEQ sequences */
/* SEQ sequence bytes, as animation_run_unit_seq_script (unit sheets) and
 * animation_run_wep_eff_seq_script (weapon and effect sheets) read them.
 * A sequence is a run of two-byte frame instructions (an SHP frame, then the
 * frames to wait; a wait of 0 holds the frame for good) and of commands:
 * ANIMATION_SEQ_OP_PREFIX, an opcode and its operand bytes. A jump offset is a
 * signed byte counted from the command's own prefix byte, so -2 repeats the
 * frame instruction before it. UNKNOWN_XX commands only skip their operands. */
enum {
    ANIMATION_SEQ_OP_PREFIX = 0xff,
};

typedef enum animation_seq_opcode {
    /* 0xbe-0xd2 and 0xd4 are unit commands: a weapon or effect sequence that uses one is an error. */
    ANIMATION_SEQ_OP_STOP_POSITION_COPIES = 0xbe,
    ANIMATION_SEQ_OP_START_POSITION_COPIES = 0xbf, /* the trail of sprite_afterimage_draw */
    ANIMATION_SEQ_OP_WAIT_FOR_DISTORTION = 0xc0,   /* offset: jump while a distortion animation runs */
    ANIMATION_SEQ_OP_START_DISTORTION = 0xc1,      /* distortion animation id - 2, timer */
    ANIMATION_SEQ_OP_NOP = 0xc2,
    ANIMATION_SEQ_OP_HIDE_ITEM = 0xc3,
    ANIMATION_SEQ_OP_SET_ITEM_POSITION = 0xc4, /* x, y: signed offsets of the item sprite from the unit */
    ANIMATION_SEQ_OP_SHOW_ITEM = 0xc5,         /* draw the unit's item display (sprite_draw_item_display) */
    ANIMATION_SEQ_OP_WAIT_FOR_INPUT = 0xc6,    /* offset: jump while g_state_window_closed is 0 */
    ANIMATION_SEQ_OP_UNKNOWN_C7 = 0xc7,        /* two operands */
    ANIMATION_SEQ_OP_UNKNOWN_C8 = 0xc8,        /* two operands */
    ANIMATION_SEQ_OP_UNKNOWN_C9 = 0xc9,        /* two operands */
    ANIMATION_SEQ_OP_UNKNOWN_CA = 0xca,        /* one operand */
    ANIMATION_SEQ_OP_MOVE_UP_2 = 0xcb,         /* 0xcb-0xd2 shift the sprite by one or two units of seq_offset */
    ANIMATION_SEQ_OP_MOVE_UP_1 = 0xcc,
    ANIMATION_SEQ_OP_MOVE_BACKWARD_2 = 0xcd,
    ANIMATION_SEQ_OP_MOVE_BACKWARD_1 = 0xce,
    ANIMATION_SEQ_OP_MOVE_DOWN_2 = 0xcf,
    ANIMATION_SEQ_OP_MOVE_DOWN_1 = 0xd0,
    ANIMATION_SEQ_OP_MOVE_FORWARD_2 = 0xd1,
    ANIMATION_SEQ_OP_MOVE_FORWARD_1 = 0xd2,
    ANIMATION_SEQ_OP_WAIT_FOR_EFFECT_LOAD = 0xd3, /* offset: jump while g_state_animation_continue_check is 1 */
    ANIMATION_SEQ_OP_PLAY_WEAPON_SOUND = 0xd4,    /* 0 swing, 1 hit: action_play_weapon_sfx */
    ANIMATION_SEQ_OP_RESTART = 0xd5,              /* back to the sequence's first byte, counting the restart */
    ANIMATION_SEQ_OP_WAIT_FOR_EFFECT = 0xd6,      /* offset: jump while g_state_animation_continue_check is not 0 */
    ANIMATION_SEQ_OP_UNKNOWN_D7 = 0xd7,           /* one operand; the sequences of death and revival start with it */
    ANIMATION_SEQ_OP_SET_FRAME_BIAS = 0xd8,       /* signed; added to every later frame id */
    ANIMATION_SEQ_OP_LAUNCH_PROJECTILE = 0xd9, /* two unread operands: the thrown item, stone or arrow of the action */
    ANIMATION_SEQ_OP_UNDEFINED_DA = 0xda,      /* no handler: SYSTEM_ERROR_SEQ_OPCODE like any other value */
    ANIMATION_SEQ_OP_SET_WAIT_BIAS = 0xdb,     /* signed; added to every later wait */
    ANIMATION_SEQ_OP_RETURN = 0xdc,            /* resume the sequence ANIMATION_SEQ_OP_CALL_SEQUENCE left */
    ANIMATION_SEQ_OP_CALL_SEQUENCE = 0xdd,     /* sequence id: run it from its start; one level deep */
    ANIMATION_SEQ_OP_STRIKE_TARGETS = 0xde,    /* the blow of a weapon-like ability lands: its targets react */
    ANIMATION_SEQ_OP_CLEAR_ROTATION = 0xdf,
    ANIMATION_SEQ_OP_HIDE_SHADOW = 0xe0,
    ANIMATION_SEQ_OP_SHOW_SHADOW = 0xe1,
    ANIMATION_SEQ_OP_SET_LAYER_PRIORITY = 0xe2, /* row of g_animation_layer_priorities */
    ANIMATION_SEQ_OP_UNKNOWN_E3 = 0xe3,         /* one operand */
    ANIMATION_SEQ_OP_UNKNOWN_E4 = 0xe4,         /* one operand */
    ANIMATION_SEQ_OP_SET_ROTATION = 0xe5,       /* low byte, high byte: the display's rotation, ONE to the turn */
    ANIMATION_SEQ_OP_UNKNOWN_E6 = 0xe6,         /* two operands */
    ANIMATION_SEQ_OP_UNKNOWN_E7 = 0xe7,         /* two operands */
    ANIMATION_SEQ_OP_UNKNOWN_E8 = 0xe8,         /* two operands */
    ANIMATION_SEQ_OP_UNKNOWN_E9 = 0xe9,         /* one operand */
    ANIMATION_SEQ_OP_UNKNOWN_EA = 0xea,         /* two operands */
    ANIMATION_SEQ_OP_FLIP_VERTICAL = 0xeb,
    ANIMATION_SEQ_OP_FLIP_HORIZONTAL = 0xec,
    ANIMATION_SEQ_OP_UNKNOWN_ED = 0xed,   /* one operand */
    ANIMATION_SEQ_OP_MOVE_UNIT_FB = 0xee, /* signed: forward (+) or backward; 0xee-0xf0 and 0xfa move units only */
    ANIMATION_SEQ_OP_MOVE_UNIT_DU = 0xef, /* signed: down (+) or up */
    ANIMATION_SEQ_OP_MOVE_UNIT_RL = 0xf0, /* signed: right (+) or left */
    ANIMATION_SEQ_OP_UNKNOWN_F1 = 0xf1,   /* two operands */
    ANIMATION_SEQ_OP_START_BLOCK_SEQUENCE = 0xf2, /* sprite block 1-3, sequence id: show the weapon or effect sprite */
    ANIMATION_SEQ_OP_UNKNOWN_F3 = 0xf3,           /* one operand */
    ANIMATION_SEQ_OP_UNKNOWN_F4 = 0xf4,           /* one operand */
    ANIMATION_SEQ_OP_UNKNOWN_F5 = 0xf5,           /* one operand */
    ANIMATION_SEQ_OP_PLAY_SOUND = 0xf6,           /* sound_sfx_e */
    ANIMATION_SEQ_OP_UNKNOWN_F7 = 0xf7,           /* three operands */
    ANIMATION_SEQ_OP_UNKNOWN_F8 = 0xf8,           /* two operands */
    ANIMATION_SEQ_OP_MOVE_SCREEN_OFFSET = 0xf9,   /* signed x, y: added to the unit's screen_offset */
    ANIMATION_SEQ_OP_MOVE_UNIT = 0xfa,            /* right, down, forward: the operands of 0xf0, 0xef and 0xee */
    ANIMATION_SEQ_OP_UNKNOWN_FB = 0xfb,           /* two operands */
    ANIMATION_SEQ_OP_REPEAT = 0xfc,               /* offset, count: jump count times, then go on */
    ANIMATION_SEQ_OP_JUMP = 0xfd,                 /* offset */
    ANIMATION_SEQ_OP_END_ANIMATION = 0xfe,        /* stop; a weapon or effect sprite is hidden too */
    ANIMATION_SEQ_OP_PAUSE_ANIMATION
    = 0xff, /* stop; either one puts a unit in its g_battle_gfx_targeted_frame_* pose */
} animation_seq_opcode_e;

/* Animation ids from 500 up are not doubled into a front and a back
 * sequence: 500 + n and 600 + n play sequence n of EVTCHR slot 0 and 1. */
enum {
    ANIMATION_SEQUENCE_EVTCHR_SLOT_0 = 500,
    ANIMATION_SEQUENCE_EVTCHR_SLOT_1 = 600,
};

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

/* sprite displays */
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

/* animation blocks */
/* graphic_type of a unit's sprite blocks: which sheet the block animates. */
typedef enum sprite_graphic_type {
    SPRITE_GRAPHIC_UNIT = 0,
    SPRITE_GRAPHIC_WEAPON = 1, /* WEP sheets */
    SPRITE_GRAPHIC_EFFECT = 2, /* EFF sheets */
} sprite_graphic_type_e;

/* Unit sprite animation state passed to the sequence and frame renderers.
 * Weapon/effect sprite blocks share its first 0x1c bytes, with distinct tails. */
typedef struct animation_state {
    u16 trigger;      /* 0x00; nonzero while the sprite is drawn */
    u16 graphic_type; /* 0x02; sprite_graphic_type_e */
    /* 0x04; index of the sequence in the SEQ table. A unit's is its
     * animation_e * 2, + 1 when it faces away from the camera;
     * from ANIMATION_SEQUENCE_EVTCHR_SLOT_0 up it is a sequence of an EVTCHR slot. */
    u16 sequence_id;
    u16 script_pos;        /* 0x06; offset of the next byte to read in the sequence */
    u16 frame;             /* 0x08; SHP frame on display */
    u16 wait;              /* 0x0a; frames until the sequence is read again; 0 when it has stopped */
    u16 restart_count;     /* 0x0c; ANIMATION_SEQ_OP_RESTART commands run since the sequence started */
    u16 saved_sequence_id; /* 0x0e; where ANIMATION_SEQ_OP_RETURN resumes */
    u16 saved_script_pos;  /* 0x10 */
    u16 wait_bias;         /* 0x12; ANIMATION_SEQ_OP_SET_WAIT_BIAS: added to each wait read */
    u16 frame_bias;        /* 0x14; ANIMATION_SEQ_OP_SET_FRAME_BIAS: added to each frame id read */
    u16 repeat_count;      /* 0x16; jumps the ANIMATION_SEQ_OP_REPEAT in progress has left; 0 when none is */
    u16 flags;             /* 0x18; the flip bits of sprite_display_flags_e that the sequence toggled */
    u8 _padding_1a[2];     /* 0x1a; aligns shp */
    sprite_unit_shp_frame_tables_t* shp;       /* 0x1c */
    sprite_unit_seq_table_t* seq;              /* 0x20 */
    sprite_unit_shp_frame_tables_t* saved_shp; /* 0x24; the sheet's own tables while a status sheet replaces them */
    sprite_unit_seq_table_t* saved_seq;        /* 0x28 */
    sprite_display_t* display;                 /* 0x2c */
} animation_state_t;
typedef char animation_state_size_must_be_48[sizeof(animation_state_t) == 48 ? 1 : -1];

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

#endif
