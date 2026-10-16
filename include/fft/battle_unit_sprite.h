#ifndef BATTLE_UNIT_SPRITE_H
#define BATTLE_UNIT_SPRITE_H

/* BATTLE unit sprites: SPR, SHP and SEQ sheet formats, sequence opcodes, sprite displays and animation blocks. */

#include "fft/gfx.h"
#include "psx/types.h"

/* sheet files */
/* Unit SPR file: 16 palettes, two uncompressed 4bpp image blocks, then
 * a variable-length compressed attack-image stream. */
typedef struct battle_gfx_unit_sprite_file {
    clut_t clut;                          /* 0x0000; 16 palettes of 16 colors */
    u8 pixels[256][128];                  /* 0x0200; 256 x 256 pixels, two per byte */
    u8 portrait_and_body_pixels[32][128]; /* 0x8200; 256 x 32 pixels, two per byte */
    u8 compressed_attack_image[0];        /* 0x9200; variable byte tail */
} battle_gfx_unit_sprite_file_t;
typedef char battle_gfx_unit_sprite_file_size_must_be_37376[sizeof(battle_gfx_unit_sprite_file_t) == 37376 ? 1 : -1];

/* Packed unit SEQ file. Values and offsets are little-endian byte
 * sequences, so this view needs no native 16-bit or 32-bit alignment. */
typedef struct battle_gfx_packed_seq_record {
    u8 first_attack_sequence[2];   /* 0x000; unsigned little-endian 16-bit value */
    u8 first_sp2_sequence[2];      /* 0x002; unsigned little-endian 16-bit value */
    u8 sequence_offsets[0x100][4]; /* 0x004; signed little-endian 32-bit values, -1 means offset zero */
    u8 sequence_data_size[2];      /* 0x404; unsigned little-endian 16-bit value */
    u8 sequence_data[0];           /* 0x406; variable byte tail */
} battle_gfx_packed_seq_record_t;
typedef char battle_gfx_packed_seq_record_size_must_be_1030[sizeof(battle_gfx_packed_seq_record_t) == 1030 ? 1 : -1];

/* SHP frames */
/* One part of an SHP frame: a rectangle of the sheet and where it is drawn. */
typedef struct battle_gfx_source_part {
    s8 x_shift; /* pixels from the sprite's origin to the part's left and top edges */
    s8 y_shift;
    u16 attributes; /* BATTLE_SHP_PART_* masks */
} battle_gfx_source_part_t;

/* Encoded fields of battle_gfx_source_part_t.attributes. */
enum {
    BATTLE_SHP_PART_TILE_MASK = 0x03ff, /* the picture's place in the sheet in 8-pixel tiles: bits 0-4 u, 5-9 v */
    BATTLE_SHP_PART_SIZE_MASK = 0x3c00, /* bits 10-13: index of g_battle_gfx_part_sizes */
    BATTLE_SHP_PART_FLIP_MASK = 0xc000, /* shifted down 13: BATTLE_SPRITE_DISPLAY_HORIZONTAL_FLIP and _VERTICAL_FLIP */
};

/* The part size that a unit sheet draws as two 48x24 halves, the upper one nearer the camera. */
enum {
    BATTLE_SHP_PART_SIZE_48X48 = 14,
};

/* One frame of an SHP file: a 2-byte header followed by 4-byte parts
 * (battle_gfx_load_trap_and_unit_frame_parts). */
typedef struct battle_gfx_source_frame {
    /* Bits 0-2 are the part count minus one, bits 3-7 index the rotation
     * table g_battle_gfx_sprite_y_rotations. */
    u8 part_count_and_rotation;
    u8 flags; /* the BATTLE_SPRITE_DISPLAY_SEMI_TRANSPARENT and _BLEND_MASK bits of every part */
    battle_gfx_source_part_t parts[1];
} battle_gfx_source_frame_t;

/* Unit SHP frame tables filled by battle_gfx_unpack_unit_shp_data:
 * SHP header bytes 4..5 and 6..7, then two 0xd0-entry frame pointer tables,
 * each entry a battle_gfx_source_frame_t. The second table draws the same
 * frames without the legs, for a unit standing in water or riding
 * (battle_gfx_select_unit_shp_frame); it repeats the first when the file
 * carries a single frame set (header 8). */
typedef struct battle_gfx_unit_shp_frame_tables {
    s32 first_attack_frame;                  /* 0x000; frames from here up are drawn from the sheet's attack image */
    s32 first_sp2_frame;                     /* 0x004; set from the file and never read */
    battle_gfx_source_frame_t* frames[0xd0]; /* 0x008 */
    battle_gfx_source_frame_t* submerged_frames[0xd0]; /* 0x348 */
} battle_gfx_unit_shp_frame_tables_t;
typedef char
    battle_gfx_unit_shp_frame_tables_size_must_be_1672[sizeof(battle_gfx_unit_shp_frame_tables_t) == 1672 ? 1 : -1];

/* WEP/EFF SHP data: the first frame of each weapon type, then the frame
 * pointers from 0x40. */
typedef struct battle_gfx_weapon_shp {
    u16 first_frame[0x20];                    /* 0x000; indexed by weapon type */
    battle_gfx_source_frame_t* frames[0x200]; /* 0x040 */
} battle_gfx_weapon_shp_t;
typedef char battle_gfx_weapon_shp_size_must_be_2112[sizeof(battle_gfx_weapon_shp_t) == 2112 ? 1 : -1];

/* SEQ sequences */
/* SEQ sequence bytes, as battle_gfx_run_unit_seq_script (unit sheets) and
 * battle_gfx_run_wep_eff_seq_script (weapon and effect sheets) read them.
 * A sequence is a run of two-byte frame instructions (an SHP frame, then the
 * frames to wait; a wait of 0 holds the frame for good) and of commands:
 * BATTLE_SEQ_OP_PREFIX, an opcode and its operand bytes. A jump offset is a
 * signed byte counted from the command's own prefix byte, so -2 repeats the
 * frame instruction before it. UNKNOWN_XX commands only skip their operands. */
enum {
    BATTLE_SEQ_OP_PREFIX = 0xff,
};

typedef enum battle_seq_opcode {
    /* 0xbe-0xd2 and 0xd4 are unit commands: a weapon or effect sequence that uses one is an error. */
    BATTLE_SEQ_OP_STOP_POSITION_COPIES = 0xbe,
    BATTLE_SEQ_OP_START_POSITION_COPIES = 0xbf, /* the trail of battle_gfx_draw_unit_sprite_position_copies */
    BATTLE_SEQ_OP_WAIT_FOR_DISTORTION = 0xc0,   /* offset: jump while a distortion animation runs */
    BATTLE_SEQ_OP_START_DISTORTION = 0xc1,      /* distortion animation id - 2, timer */
    BATTLE_SEQ_OP_NOP = 0xc2,
    BATTLE_SEQ_OP_HIDE_ITEM = 0xc3,
    BATTLE_SEQ_OP_SET_ITEM_POSITION = 0xc4, /* x, y: signed offsets of the item sprite from the unit */
    BATTLE_SEQ_OP_SHOW_ITEM = 0xc5,         /* draw the unit's item display (battle_gfx_init_item_get_rendering) */
    BATTLE_SEQ_OP_WAIT_FOR_INPUT = 0xc6,    /* offset: jump while g_battle_action_post_action is 0 */
    BATTLE_SEQ_OP_UNKNOWN_C7 = 0xc7,        /* two operands */
    BATTLE_SEQ_OP_UNKNOWN_C8 = 0xc8,        /* two operands */
    BATTLE_SEQ_OP_UNKNOWN_C9 = 0xc9,        /* two operands */
    BATTLE_SEQ_OP_UNKNOWN_CA = 0xca,        /* one operand */
    BATTLE_SEQ_OP_MOVE_UP_2 = 0xcb,         /* 0xcb-0xd2 shift the sprite by one or two units of seq_offset */
    BATTLE_SEQ_OP_MOVE_UP_1 = 0xcc,
    BATTLE_SEQ_OP_MOVE_BACKWARD_2 = 0xcd,
    BATTLE_SEQ_OP_MOVE_BACKWARD_1 = 0xce,
    BATTLE_SEQ_OP_MOVE_DOWN_2 = 0xcf,
    BATTLE_SEQ_OP_MOVE_DOWN_1 = 0xd0,
    BATTLE_SEQ_OP_MOVE_FORWARD_2 = 0xd1,
    BATTLE_SEQ_OP_MOVE_FORWARD_1 = 0xd2,
    BATTLE_SEQ_OP_WAIT_FOR_EFFECT_LOAD = 0xd3, /* offset: jump while g_battle_state_animation_continue_check is 1 */
    BATTLE_SEQ_OP_PLAY_WEAPON_SOUND = 0xd4,    /* 0 swing, 1 hit: battle_sound_play_weapon_sfx */
    BATTLE_SEQ_OP_RESTART = 0xd5,              /* back to the sequence's first byte, counting the restart */
    BATTLE_SEQ_OP_WAIT_FOR_EFFECT = 0xd6,      /* offset: jump while g_battle_state_animation_continue_check is not 0 */
    BATTLE_SEQ_OP_UNKNOWN_D7 = 0xd7,           /* one operand; the sequences of death and revival start with it */
    BATTLE_SEQ_OP_SET_FRAME_BIAS = 0xd8,       /* signed; added to every later frame id */
    BATTLE_SEQ_OP_LAUNCH_PROJECTILE = 0xd9,    /* two unread operands: the thrown item, stone or arrow of the action */
    BATTLE_SEQ_OP_UNDEFINED_DA = 0xda,         /* no handler: MAIN_SYSTEM_ERROR_SEQ_OPCODE like any other value */
    BATTLE_SEQ_OP_SET_WAIT_BIAS = 0xdb,        /* signed; added to every later wait */
    BATTLE_SEQ_OP_RETURN = 0xdc,               /* resume the sequence BATTLE_SEQ_OP_CALL_SEQUENCE left */
    BATTLE_SEQ_OP_CALL_SEQUENCE = 0xdd,        /* sequence id: run it from its start; one level deep */
    BATTLE_SEQ_OP_STRIKE_TARGETS = 0xde,       /* the blow of a weapon-like ability lands: its targets react */
    BATTLE_SEQ_OP_CLEAR_ROTATION = 0xdf,
    BATTLE_SEQ_OP_HIDE_SHADOW = 0xe0,
    BATTLE_SEQ_OP_SHOW_SHADOW = 0xe1,
    BATTLE_SEQ_OP_SET_LAYER_PRIORITY = 0xe2, /* row of g_battle_gfx_animation_layer_priorities */
    BATTLE_SEQ_OP_UNKNOWN_E3 = 0xe3,         /* one operand */
    BATTLE_SEQ_OP_UNKNOWN_E4 = 0xe4,         /* one operand */
    BATTLE_SEQ_OP_SET_ROTATION = 0xe5,       /* low byte, high byte: the display's rotation, ONE to the turn */
    BATTLE_SEQ_OP_UNKNOWN_E6 = 0xe6,         /* two operands */
    BATTLE_SEQ_OP_UNKNOWN_E7 = 0xe7,         /* two operands */
    BATTLE_SEQ_OP_UNKNOWN_E8 = 0xe8,         /* two operands */
    BATTLE_SEQ_OP_UNKNOWN_E9 = 0xe9,         /* one operand */
    BATTLE_SEQ_OP_UNKNOWN_EA = 0xea,         /* two operands */
    BATTLE_SEQ_OP_FLIP_VERTICAL = 0xeb,
    BATTLE_SEQ_OP_FLIP_HORIZONTAL = 0xec,
    BATTLE_SEQ_OP_UNKNOWN_ED = 0xed,           /* one operand */
    BATTLE_SEQ_OP_MOVE_UNIT_FB = 0xee,         /* signed: forward (+) or backward; 0xee-0xf0 and 0xfa move units only */
    BATTLE_SEQ_OP_MOVE_UNIT_DU = 0xef,         /* signed: down (+) or up */
    BATTLE_SEQ_OP_MOVE_UNIT_RL = 0xf0,         /* signed: right (+) or left */
    BATTLE_SEQ_OP_UNKNOWN_F1 = 0xf1,           /* two operands */
    BATTLE_SEQ_OP_START_BLOCK_SEQUENCE = 0xf2, /* sprite block 1-3, sequence id: show the weapon or effect sprite */
    BATTLE_SEQ_OP_UNKNOWN_F3 = 0xf3,           /* one operand */
    BATTLE_SEQ_OP_UNKNOWN_F4 = 0xf4,           /* one operand */
    BATTLE_SEQ_OP_UNKNOWN_F5 = 0xf5,           /* one operand */
    BATTLE_SEQ_OP_PLAY_SOUND = 0xf6,           /* main_sfx_e */
    BATTLE_SEQ_OP_UNKNOWN_F7 = 0xf7,           /* three operands */
    BATTLE_SEQ_OP_UNKNOWN_F8 = 0xf8,           /* two operands */
    BATTLE_SEQ_OP_MOVE_SCREEN_OFFSET = 0xf9,   /* signed x, y: added to the unit's screen_offset */
    BATTLE_SEQ_OP_MOVE_UNIT = 0xfa,            /* right, down, forward: the operands of 0xf0, 0xef and 0xee */
    BATTLE_SEQ_OP_UNKNOWN_FB = 0xfb,           /* two operands */
    BATTLE_SEQ_OP_REPEAT = 0xfc,               /* offset, count: jump count times, then go on */
    BATTLE_SEQ_OP_JUMP = 0xfd,                 /* offset */
    BATTLE_SEQ_OP_END_ANIMATION = 0xfe,        /* stop; a weapon or effect sprite is hidden too */
    BATTLE_SEQ_OP_PAUSE_ANIMATION = 0xff, /* stop; either one puts a unit in its g_battle_gfx_targeted_frame_* pose */
} battle_seq_opcode_e;

/* Animation ids from 500 up are not doubled into a front and a back
 * sequence: 500 + n and 600 + n play sequence n of EVTCHR slot 0 and 1. */
enum {
    BATTLE_SEQUENCE_EVTCHR_SLOT_0 = 500,
    BATTLE_SEQUENCE_EVTCHR_SLOT_1 = 600,
};

/* Common header of an unpacked unit SEQ table. The pointer tail has 0x100
 * entries in a native table and 0x40 in an EVTCHR slot. */
typedef struct battle_gfx_unit_seq_table {
    s32 first_attack_sequence; /* 0x00; sequences from here use the attack image */
    s32 first_sp2_sequence;    /* 0x04; sequences from here use the SP2 file */
    u8* sequences[0];          /* 0x08; variable pointer tail */
} battle_gfx_unit_seq_table_t;
typedef char battle_gfx_unit_seq_table_size_must_be_8[sizeof(battle_gfx_unit_seq_table_t) == 8 ? 1 : -1];

/* Native table storage: five resident tables and the separate boss table.
 * Their sequences live in the shared sequence-data buffer. */
typedef struct battle_gfx_native_unit_seq_table {
    battle_gfx_unit_seq_table_t header; /* 0x000 */
    u8* sequences[0x100];               /* 0x008 */
} battle_gfx_native_unit_seq_table_t;
typedef char
    battle_gfx_native_unit_seq_table_size_must_be_1032[sizeof(battle_gfx_native_unit_seq_table_t) == 1032 ? 1 : -1];

/* An EVTCHR slot's fixed SEQ storage. Its 32-bit header fields are unset and unread;
 * its 0x40 pointers refer into the slot's own 0x400 bytes. */
typedef struct battle_gfx_unit_seq {
    battle_gfx_unit_seq_table_t header; /* 0x000 */
    u8* sequences[0x40];                /* 0x008 */
    u8 sequence_data[0x400];            /* 0x108 */
} battle_gfx_unit_seq_t;
typedef char battle_gfx_unit_seq_size_must_be_1288[sizeof(battle_gfx_unit_seq_t) == 1288 ? 1 : -1];

/* Native WEP/EFF storage pairs the SHP frame table with its SEQ pointers. */
typedef struct battle_gfx_wep_eff_graphics {
    battle_gfx_weapon_shp_t shp; /* 0x000 */
    u8* seq[0x100];              /* 0x840; sequence command streams */
} battle_gfx_wep_eff_graphics_t;
typedef char battle_gfx_wep_eff_graphics_size_must_be_3136[sizeof(battle_gfx_wep_eff_graphics_t) == 3136 ? 1 : -1];

/* sprite displays */
/* How battle_gfx_construct_polygon_data_for_units draws a sprite part. The
 * unit's sprite_display_flags, its horizontal_flip_flag, the flags a
 * sequence toggles and each part's own are XORed together. */
typedef enum battle_sprite_display_flags {
    BATTLE_SPRITE_DISPLAY_SEMI_TRANSPARENT = 0x01,
    BATTLE_SPRITE_DISPLAY_HORIZONTAL_FLIP = 0x02,
    BATTLE_SPRITE_DISPLAY_VERTICAL_FLIP = 0x04,
    /* A part's flags only: queued nearer the camera than the rest by its pitch and zoom. */
    BATTLE_SPRITE_DISPLAY_NEARER = 0x80,
} battle_sprite_display_flags_e;

/* Two-bit selector in sprite flags and tpage, already shifted to bits 5-6. */
typedef enum battle_sprite_display_blend_mode {
    BATTLE_SPRITE_DISPLAY_BLEND_AVERAGE = 0x00,
    BATTLE_SPRITE_DISPLAY_BLEND_ADD = 0x20,
    BATTLE_SPRITE_DISPLAY_BLEND_SUBTRACT = 0x40,
    BATTLE_SPRITE_DISPLAY_BLEND_ADD_QUARTER = 0x60,
} battle_sprite_display_blend_mode_e;

enum { BATTLE_SPRITE_DISPLAY_BLEND_MASK = 0x60 };

/* One part of a sprite display: a rectangle of the display's texture page,
 * in pixels, and its place relative to the sprite's origin. */
typedef struct battle_gfx_sprite_part_display_data {
    s8 x_shift;
    s8 y_shift;
    u8 width;
    u8 height;
    u8 u;
    u8 v;
    u8 flags; /* battle_sprite_display_flags_e and battle_sprite_display_blend_mode_e */
} battle_gfx_sprite_part_display_data_t;
typedef char
    battle_gfx_sprite_part_display_data_size_must_be_7[(sizeof(battle_gfx_sprite_part_display_data_t) == 7) ? 1 : -1];

/* A sprite display: texture page, CLUT, color and the parts cut from it.
 * Renderer APIs use this common header and variable part tail; the fixed
 * storage records below describe each slot inside a 0x440-byte runtime record. */
typedef struct battle_gfx_sprite_display_data {
    rgb24_t color; /* 0x00 */
    u8 part_count; /* 0x03; parts in use */
    s16 tpage;     /* 0x04; BATTLE_GFX_TPAGE_*, with battle_sprite_display_blend_mode_e */
    u16 clut;      /* 0x06 */
    u16 scale_x;   /* 0x08; ONE is neutral scale */
    u16 scale_y;   /* 0x0a; ONE is neutral scale */
    /* 0x0c; turns the sprite about its origin, ONE to the turn. Each frame
     * sets it from its header; BATTLE_SEQ_OP_SET_ROTATION and _CLEAR_ROTATION
     * then replace it. */
    u16 rotation;
    battle_gfx_sprite_part_display_data_t parts[0]; /* 0x0e; variable tail */
} battle_gfx_sprite_display_data_t;
typedef char battle_gfx_sprite_display_data_size_must_be_14[sizeof(battle_gfx_sprite_display_data_t) == 14 ? 1 : -1];

typedef struct battle_gfx_unit_sprite_display {
    battle_gfx_sprite_display_data_t header;        /* 0x00 */
    battle_gfx_sprite_part_display_data_t parts[8]; /* 0x0e */
} battle_gfx_unit_sprite_display_t;
typedef char battle_gfx_unit_sprite_display_size_must_be_70[sizeof(battle_gfx_unit_sprite_display_t) == 70 ? 1 : -1];

typedef struct battle_gfx_weapon_sprite_display {
    battle_gfx_sprite_display_data_t header;        /* 0x00 */
    battle_gfx_sprite_part_display_data_t parts[3]; /* 0x0e */
    u8 _padding_23;                                 /* aligns the following display */
} battle_gfx_weapon_sprite_display_t;
typedef char
    battle_gfx_weapon_sprite_display_size_must_be_36[sizeof(battle_gfx_weapon_sprite_display_t) == 36 ? 1 : -1];

typedef struct battle_gfx_single_part_sprite_display {
    battle_gfx_sprite_display_data_t header;        /* 0x00 */
    battle_gfx_sprite_part_display_data_t parts[1]; /* 0x0e */
    u8 _padding_15;                                 /* aligns the following display */
} battle_gfx_single_part_sprite_display_t;
typedef char battle_gfx_single_part_sprite_display_size_must_be_22[sizeof(battle_gfx_single_part_sprite_display_t) == 22
        ? 1
        : -1];

/* Sprite lookup result filled in by battle_gfx_get_unit_sprite_frame_and_vram_ids. */
typedef struct battle_unit_sprite_query {
    s16 clut;             /* 0x00; the unit's CLUT of MAP_CLUT_ROW_UNITS */
    s16 tpage;            /* 0x02 */
    s16 graphic_height;   /* 0x04 */
    s16 graphic_y_offset; /* 0x06 */
} battle_unit_sprite_query_t;

/* animation blocks */
/* graphic_type of a unit's sprite blocks: which sheet the block animates. */
typedef enum battle_sprite_graphic_type {
    BATTLE_SPRITE_GRAPHIC_UNIT = 0,
    BATTLE_SPRITE_GRAPHIC_WEAPON = 1, /* WEP sheets */
    BATTLE_SPRITE_GRAPHIC_EFFECT = 2, /* EFF sheets */
} battle_sprite_graphic_type_e;

/* Unit sprite animation state passed to the sequence and frame renderers.
 * Weapon/effect sprite blocks share its first 0x1c bytes, with distinct tails. */
typedef struct battle_unit_anim_state {
    u16 trigger;      /* 0x00; nonzero while the sprite is drawn */
    u16 graphic_type; /* 0x02; battle_sprite_graphic_type_e */
    /* 0x04; index of the sequence in the SEQ table. A unit's is its
     * battle_unit_animation_e * 2, + 1 when it faces away from the camera;
     * from BATTLE_SEQUENCE_EVTCHR_SLOT_0 up it is a sequence of an EVTCHR slot. */
    u16 sequence_id;
    u16 script_pos;        /* 0x06; offset of the next byte to read in the sequence */
    u16 frame;             /* 0x08; SHP frame on display */
    u16 wait;              /* 0x0a; frames until the sequence is read again; 0 when it has stopped */
    u16 restart_count;     /* 0x0c; BATTLE_SEQ_OP_RESTART commands run since the sequence started */
    u16 saved_sequence_id; /* 0x0e; where BATTLE_SEQ_OP_RETURN resumes */
    u16 saved_script_pos;  /* 0x10 */
    u16 wait_bias;         /* 0x12; BATTLE_SEQ_OP_SET_WAIT_BIAS: added to each wait read */
    u16 frame_bias;        /* 0x14; BATTLE_SEQ_OP_SET_FRAME_BIAS: added to each frame id read */
    u16 repeat_count;      /* 0x16; jumps the BATTLE_SEQ_OP_REPEAT in progress has left; 0 when none is */
    u16 flags;             /* 0x18; the flip bits of battle_sprite_display_flags_e that the sequence toggled */
    u8 _padding_1a[2];     /* 0x1a; aligns shp */
    battle_gfx_unit_shp_frame_tables_t* shp;       /* 0x1c */
    battle_gfx_unit_seq_table_t* seq;              /* 0x20 */
    battle_gfx_unit_shp_frame_tables_t* saved_shp; /* 0x24; the sheet's own tables while a status sheet replaces them */
    battle_gfx_unit_seq_table_t* saved_seq;        /* 0x28 */
    battle_gfx_sprite_display_data_t* display;     /* 0x2c */
} battle_unit_anim_state_t;
typedef char battle_unit_anim_state_size_must_be_48[sizeof(battle_unit_anim_state_t) == 48 ? 1 : -1];

/* Weapon/effect sprite animation block, runtime unit data 0x208, 0x238 and
 * 0x268: block 0 plays the weapon sheet's sequences, blocks 1 and 2 the
 * effect sheet's. It shares the first 0x1c bytes of battle_unit_anim_state_t,
 * which has their meanings, but keeps its display pointer at 0x24
 * (battle_gfx_run_wep_eff_seq_script, battle_gfx_draw_unit_sprite_layers,
 * battle_unit_init). Its SEQ table has no header: seq[sequence_id]
 * is the sequence. */
typedef struct battle_unit_sprite_block {
    u16 trigger;                               /* 0x00; nonzero while the sprite is drawn */
    u16 graphic_type;                          /* 0x02; battle_sprite_graphic_type_e */
    u16 sequence_id;                           /* 0x04 */
    u16 script_pos;                            /* 0x06 */
    u16 frame;                                 /* 0x08; includes the weapon type's first frame */
    u16 wait;                                  /* 0x0a */
    u16 restart_count;                         /* 0x0c */
    u16 saved_sequence_id;                     /* 0x0e */
    u16 saved_script_pos;                      /* 0x10 */
    u16 wait_bias;                             /* 0x12 */
    u16 frame_bias;                            /* 0x14 */
    u16 repeat_count;                          /* 0x16 */
    u16 flags;                                 /* 0x18 */
    u8 _padding_1a[2];                         /* 0x1a; aligns shp */
    battle_gfx_weapon_shp_t* shp;              /* 0x1c */
    u8** seq;                                  /* 0x20 */
    battle_gfx_sprite_display_data_t* display; /* 0x24 */
    u8 _unused_28[8];                          /* 0x28 */
} battle_unit_sprite_block_t;
typedef char battle_unit_sprite_block_size_must_be_48[(sizeof(battle_unit_sprite_block_t) == 48) ? 1 : -1];

#endif
