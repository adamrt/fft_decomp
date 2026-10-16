#ifndef BATTLE_EFFECT_FILE_H
#define BATTLE_EFFECT_FILE_H

/* Ability effect files (EFFECT/E###.BIN): ids, section layouts and the pointers bound to the loaded sections. */

#include "fft/gfx.h"
#include "psx/libgte.h"
#include "psx/suzuki.h"
#include "psx/types.h"

/* file ids */
/* An ability-to-file entry: file ID bits and the Item flag, negative for none.
 * Event effects follow the action/reaction rows and use the script ID remapping. */
enum {
    EFFECT_ABILITY_FILE_MASK = 0x1ff, /* the number in the file's name, EFFECT/E###.BIN */
    EFFECT_ABILITY_FLAG_ITEM = 0x800, /* Item abilities and Throw Ball: battle_effect_is_item_ability */
};

/* One resident header and disc-table slot for each 9-bit effect file ID. */
enum { EFFECT_FILE_ID_COUNT = 512 };

/* The effect files the code names by number. */
enum {
    EFFECT_FILE_CURE = 1, /* loaded in place of a file of no size */
    EFFECT_FILE_FIRE = 16,
    EFFECT_FILE_BOLT = 20,
    EFFECT_FILE_ICE = 24,
};

/* section table */
/* Section table at the start of an effect file's header section: byte offsets
 * from the table itself, each read by its section's binder in effect_script.c
 * (the battle_effect_set_*_pointer binders and the flags, sound and palette loaders). */
typedef struct effect_file_header {
    s32 frame_data;          /* 0x00: the binder skips the section's leading 32-bit value */
    s32 animation_sequences; /* 0x04: likewise */
    s32 script;              /* 0x08: effect script section */
    s32 particle_system;     /* 0x0c: emitter motion and control */
    s32 animation_curve;     /* 0x10 */
    s32 timing_curves;       /* 0x14 */
    s32 flags_section;       /* 0x18 */
    s32 timeline;            /* 0x1c: timing, palette and camera tracks */
    s32 sound_section;       /* 0x20: 0 when the effect has no sound */
    s32 palette_and_texture; /* 0x24: two CLUT blocks followed by the image */
} effect_file_header_t;
typedef char effect_file_header_size_must_be_40[sizeof(effect_file_header_t) == 40 ? 1 : -1];

/* flags and sound */
/* How an effect sound channel picks among its three sound ids on each use. */
typedef enum effect_sound_mode {
    EFFECT_SOUND_MODE_FIRST = 0,                /* always the first */
    EFFECT_SOUND_MODE_ALTERNATE = 1,            /* the first and second in turn */
    EFFECT_SOUND_MODE_FIRST_THEN_SECOND = 2,    /* the first once, then the second */
    EFFECT_SOUND_MODE_FIRST_THEN_ALTERNATE = 3, /* the first once, then the third and second in turn */
    EFFECT_SOUND_MODE_CYCLE = 4,                /* the three in turn */
} effect_sound_mode_e;

/* One of the four sound channels of an effect: the sound tracks of the
 * timelines name a channel, and each use plays the id its mode picks. */
typedef struct effect_sound_channel {
    u8 mode;         /* 0x00; effect_sound_mode_e */
    u8 sound_ids[3]; /* 0x01; ids in the effect's sound section, 0 for none */
} effect_sound_channel_t;

/* The flags section (effect_file_header_t.flags_section). The engine loads flags as a 32-bit value
 * and tests single bits; spawn_delay_override replaces the timeline's spawn_delay only while
 * g_battle_effect_context.math_skill_flag is set. */
typedef struct effect_flags_section {
    u32 flags;                          /* 0x00; EFFECT_FLAG_* bits */
    u8 spawn_delay_override;            /* 0x04 */
    u8 _unused_05[3];                   /* 0x05 */
    effect_sound_channel_t channels[4]; /* 0x08 */
} effect_flags_section_t;
typedef char effect_flags_section_size_must_be_24[(sizeof(effect_flags_section_t) == 24) ? 1 : -1];

/* effect_flags_section_t.flags bits read by the engine. */
enum {
    /* caster and target anchors take the tile's surface height: battle_effect_resolve_target_world_coords */
    EFFECT_FLAG_TERRAIN_HEIGHT_ADJUST = 0x08,
    EFFECT_FLAG_QUIET_MUSIC = 0x10,         /* lower the music while the effect loads and plays */
    EFFECT_FLAG_TIMING_CURVE_PHASE1 = 0x20, /* phase 1 of the parent timeline sets the time scale from its curve */
    EFFECT_FLAG_TIMING_CURVE_CHILD = 0x40,  /* a child timeline sets the time scale from its curve */
};

extern const effect_flags_section_t* g_battle_effect_flags_section;
extern u8 g_battle_effect_sound_call_counts[]; /* per-channel call counts */
extern s32 g_battle_effect_sound_data_base;
extern struct suzuki_sound_resource* g_battle_effect_sound_section_ptr;

/* palette and image */
/* Two palette rows, then a 24-bit image size, width flag and pixel tail. */
typedef struct effect_palette_section {
    clut_t primary_clut;   /* 0x000 */
    clut_t secondary_clut; /* 0x200 */
    u8 image_size[3];      /* 0x400; little-endian bytes */
    u8 wide_image;         /* 0x403; 256-byte rows when nonzero, otherwise 128 */
    u32 pixels[0];         /* 0x404; packed 32-bit GPU image-transfer values */
} effect_palette_section_t;
typedef char effect_palette_section_size_must_be_1028[sizeof(effect_palette_section_t) == 1028 ? 1 : -1];

extern effect_palette_section_t* g_battle_effect_palette_ptr;

/* script */
/* An effect script instruction starts with a 16-bit header, its opcode in the low nine bits
 * (g_battle_effect_instructions holds 120 handlers; retail scripts stay below
 * 2e) and small operands in the bits above, followed by up to three operand
 * 16-bit values. Script offsets are bytes from the start of the script. */
enum { EFFECT_SCRIPT_OPCODE_MASK = 0x1ff };

/* Each opcode interprets the selector byte differently. Unnamed bits are ignored
 * by that view. Copy the byte before reading bit fields to retain byte loads. */
typedef union effect_script_parameters {
    struct {
        u8 : 4;
        u8 high_nibble : 4;
    } opcode_05;
    struct {
        u8 : 4;
        u8 slot : 4;
    } child;
    struct {
        u8 : 4;
        u8 slot : 2;
        u8 : 2;
    } free_child;
    struct {
        u8 : 6;
        u8 slot : 2;
    } counter;
    struct {
        u8 : 2;
        u8 slot : 6;
    } callback;
    struct {
        u8 : 2;
        u8 count : 6;
    } targets;
} effect_script_parameters_t;
typedef char effect_script_parameters_size_must_be_1[sizeof(effect_script_parameters_t) == 1 ? 1 : -1];

/* 16-bit-aligned instructions have a nine-bit opcode and seven selector bits. */
typedef struct effect_script_header {
    u8 opcode_low;                         /* 0x00 */
    effect_script_parameters_t parameters; /* 0x01: opcode bit 8 and opcode-specific selectors */
} effect_script_header_t;
typedef char effect_script_header_size_must_be_2[(sizeof(effect_script_header_t) == 2) ? 1 : -1];

/* A view at one instruction boundary in a variable-length byte stream.
 * Immediate, branch and camera instructions occupy 4, 6 and 8 bytes respectively;
 * two-byte instructions contain only the header. No array uses this union's stride. */
typedef union effect_script_instruction {
    effect_script_header_t header;
    struct {
        effect_script_header_t header; /* 0x00 */
        s16 value;                     /* 0x02 */
    } immediate;
    struct {
        effect_script_header_t header; /* 0x00 */
        s16 comparison;                /* 0x02 */
        u16 target_offset;             /* 0x04: script byte offset */
    } branch;
    struct {
        effect_script_header_t header; /* 0x00 */
        s16 x;                         /* 0x02 */
        s16 y;                         /* 0x04 */
        s16 z;                         /* 0x06 */
    } vector;
} effect_script_instruction_t;
typedef char effect_script_instruction_size_must_be_8[sizeof(effect_script_instruction_t) == 8 ? 1 : -1];

extern const u8* g_battle_effect_script;

/* sprite frames */
/* Byte offset to a group's frame-offset table, relative to the group-table base. */
typedef struct effect_sprite_group {
    s16 frame_table_offset;
} effect_sprite_group_t;

/* Frame sections start with a group count; group offsets are relative to this table. */
typedef struct effect_sprite_section {
    u8 group_count;                  /* 0x00 */
    u8 _unused_01[3];                /* 0x01; reserved header bytes */
    effect_sprite_group_t groups[0]; /* 0x04; variable-length group table */
} effect_sprite_section_t;
typedef char effect_sprite_section_size_must_be_4[sizeof(effect_sprite_section_t) == 4 ? 1 : -1];

/* flags of a sprite part (effect_sprite_part_t, effect_sprite_frameset_t). */
enum {
    EFFECT_SPRITE_PART_CLUT_MASK = 0x000f,
    EFFECT_SPRITE_PART_FLAG_SECOND_CLUT_ROW = 0x0010,  /* the CLUT is of MAP_CLUT_ROW_EFFECT_SECOND */
    EFFECT_SPRITE_PART_TPAGE_MODE_MASK = 0x00e0,       /* the tpage's blend rate and 8-bit color bits */
    EFFECT_SPRITE_PART_FLAG_SEMI_TRANSPARENT = 0x0200, /* draw it semi-transparent */
};

/* One textured quad of a multi-part effect sprite: a frame of an effect file's
 * frame data holds a count of them. */
typedef struct effect_sprite_part {
    u16 flags; /* 0x00; EFFECT_SPRITE_PART_*; the tpage mode bits repeat those of tpage and are not read here */
    u16 tpage; /* 0x02 */
    u8 u;
    u8 v;
    u8 width;
    u8 height;
    /* Four-byte alignment preserves the original x/y 32-bit copies. */
    DVECTOR corners[4] __attribute__((aligned(4))); /* 0x08 */
} effect_sprite_part_t;
typedef char effect_sprite_part_size_must_be_24[(sizeof(effect_sprite_part_t) == 24) ? 1 : -1];

/* One frame selected by the animation stream; its parts render together. */
typedef struct effect_sprite_frameset {
    u16 flags;                     /* 0x00; EFFECT_SPRITE_PART_*: SEMI_TRANSPARENT and the tpage mode bits */
    s16 count;                     /* 0x02; number of sprite parts */
    effect_sprite_part_t parts[0]; /* 0x04; variable-length part table */
} effect_sprite_frameset_t;
typedef char effect_sprite_frameset_size_must_be_4[sizeof(effect_sprite_frameset_t) == 4 ? 1 : -1];

extern u8* g_battle_effect_frame_data;
extern u8* g_battle_effect_frame_data_start;

/* sprite sequences */
/* Opcodes in the low seven bits of a sprite-sequence control byte. */
typedef enum effect_sequence_opcode {
    EFFECT_SEQUENCE_OP_LOOP = 1,       /* back to the first byte */
    EFFECT_SEQUENCE_OP_SET_OFFSET = 2, /* two s16 sprite offsets follow */
    EFFECT_SEQUENCE_OP_ADD_OFFSET = 3, /* two s8 sprite offset deltas follow */
} effect_sequence_opcode_e;

/* The high bit distinguishes control bytes from ordinary frame indices. */
enum {
    EFFECT_SEQUENCE_OP_MASK = 0x7f,
    EFFECT_SEQUENCE_OP_FLAG = 0x80,
};

/* effect_list_node_t.depth_mode: the third byte of a sequence frame, which adjusts the sprite's
 * ordering-table depth. */
typedef enum effect_sprite_depth_mode {
    EFFECT_SPRITE_DEPTH_UNCHANGED = 0,
    EFFECT_SPRITE_DEPTH_NEARER_8 = 1,
    EFFECT_SPRITE_DEPTH_FIXED_8 = 2,
    EFFECT_SPRITE_DEPTH_FIXED_FARTHEST = 3, /* EFFECT_SPRITE_OT_DEPTH_MAX */
    EFFECT_SPRITE_DEPTH_FIXED_16 = 4,
    EFFECT_SPRITE_DEPTH_NEARER_16 = 5,
} effect_sprite_depth_mode_e;

/* View at a sprite-sequence byte cursor: commands occupy one, three or five
 * bytes. The coordinate operands are little-endian bytes without alignment. */
typedef union effect_sequence_instruction {
    struct {
        u8 opcode;
    } header;
    struct {
        u8 frame_index;
        u8 hold;
        u8 depth_mode; /* effect_sprite_depth_mode_e */
    } frame;
    struct {
        u8 opcode;
        u8 x_le[2];
        u8 y_le[2];
    } set_offset;
    struct {
        u8 opcode;
        s8 x;
        s8 y;
    } add_offset;
} effect_sequence_instruction_t;
typedef char effect_sequence_instruction_size_must_be_5[sizeof(effect_sequence_instruction_t) == 5 ? 1 : -1];

extern u8* g_battle_effect_animation_sequences;
extern u8* g_battle_effect_animation_sequences_start;

/* animation curves */
/* Frames in one animation curve: emitter and particle frame counters wrap here. */
enum { EFFECT_ANIMATION_CURVE_FRAMES = 160 };

/* Textured RGB intensity that preserves the texture color. */
enum { EFFECT_COLOR_NEUTRAL = 128 };

/* One interpolation factor per frame; curve indices select emitter and color tracks. */
typedef u8 effect_animation_curve_t[EFFECT_ANIMATION_CURVE_FRAMES];
typedef char assert_effect_animation_curve_size[sizeof(effect_animation_curve_t) == 160 ? 1 : -1];

/* The effect file has one count followed by one 160-byte curve per entry. */
typedef struct effect_animation_curve_table {
    u32 count;
    effect_animation_curve_t curves[0];
} effect_animation_curve_table_t;
typedef char effect_animation_curve_table_size_must_be_4[(sizeof(effect_animation_curve_table_t) == 4) ? 1 : -1];

extern const effect_animation_curve_table_t* g_battle_effect_animation_curve_data;

/* Read a curve value for a one-based binding; zero disables the curve and
 * reads as 0. BATTLE's emitter and the effect overlays share this idiom.
 * The decrement-then-compare-with--1 test is the original's: a zero test or a
 * post-decrement changes over 180 functions. The caller's index doubles as a
 * loop temporary, and a do-while wrapper changes register allocation, so use
 * the block as a standalone statement. */
#define EFFECT_READ_CURVE(index, result, binding, frame)                                                               \
    {                                                                                                                  \
        index = (binding);                                                                                             \
        if (--index != -1) {                                                                                           \
            result = g_battle_effect_animation_curve_data->curves[index][frame];                                       \
        } else {                                                                                                       \
            result = 0;                                                                                                \
        }                                                                                                              \
    }

#endif
