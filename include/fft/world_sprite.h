#ifndef WORLD_SPRITE_H
#define WORLD_SPRITE_H

/* The sprite display objects of the world map (WLDCORE) and the opening (OPEN): the flags that lead every display
 * object, the animated record, and the animation sequences, cels and parts it draws. */

#include "fft/gfx.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* animations */
/* One frame of a WLDCORE display-object or OPEN sprite-actor animation
 * sequence: the cel (part list) it draws and how long it holds. duration
 * 0xffff pins the frame. */
typedef struct display_object_frame {
    u16 cel;      /* 0x00 */
    u16 duration; /* 0x02 */
} display_object_frame_t;

/* An animation: a frame count followed by its frames. */
typedef struct display_object_sequence {
    s32 frame_count;                  /* 0x00 */
    display_object_frame_t frames[1]; /* 0x04 */
} display_object_sequence_t;

/* Serialized cel header, followed by part_count eight-byte sprite parts.
 * Origins pack the x coordinate in the low half and y in the high half. */
typedef struct display_object_cel_header {
    u32 part_count;
    u32 packed_texture_page_origin;
    u32 packed_clut_origin;
} display_object_cel_header_t;
typedef char world_sprite_cel_header_size_must_be_12[sizeof(display_object_cel_header_t) == 12 ? 1 : -1];

/* One serialized cel part. Offsets are biased by 0x80; texture_flags
 * selects vertical/horizontal UV flips and eight-bit texture depth. */
typedef struct display_object_part {
    u32 x_offset : 8;
    u32 y_offset : 8;
    u32 palette_index : 8;
    u32 texture_flags : 3;
    u32 use_part_palette : 1;
    u32 blend_rate : 2;
    u32 semi_transparent : 1;
    u32 hidden : 1;
    u8 height;
    u8 width;
    u8 v;
    u8 u;
} display_object_part_t;
typedef char world_sprite_part_size_must_be_8[sizeof(display_object_part_t) == 8 ? 1 : -1];

/* display objects */
/* 32-bit flags that lead every display object: the WLDCORE window and render
 * records and the OPEN render records. The two kind bits select the draw
 * routine; an object with either BLANK or HIDDEN set is skipped. */
typedef enum display_object_flag {
    DISPLAY_OBJECT_ANIMATED = 0x001,  /* 36-byte record drawn from its animation sequence */
    DISPLAY_OBJECT_IMAGE = 0x002,     /* 52- or 56-byte record drawn as one sprite */
    DISPLAY_OBJECT_8BIT_CLUT = 0x004, /* image: 8-bit texture instead of 4-bit */
    DISPLAY_OBJECT_BLANK = 0x008,     /* nothing to draw, whatever HIDDEN says */
    DISPLAY_OBJECT_HIDDEN = 0x010,    /* the show/hide toggle */
    /* Semi-transparent at the rate in BLEND_RATE_MASK; a WLDCORE animated object otherwise takes
     * both from each cel part. */
    DISPLAY_OBJECT_SEMI_TRANSPARENT = 0x020,
    DISPLAY_OBJECT_BLEND_RATE_MASK = 0x0c0,
    DISPLAY_OBJECT_POP_IN = 0x100,   /* image: the opening animation is running; clears itself */
    DISPLAY_OBJECT_UNTINTED = 0x400, /* WLDCORE: the color is used as is, without the fade tint */
    /* WLDCORE animated object: steps its frame counter even while MAIN_SYSTEM_FLAG_FREEZE_ANIMATION is set. */
    DISPLAY_OBJECT_ANIMATE_WHILE_PAUSED = 0x800,
} display_object_flag_e;

/* A 36-byte animated display object of WLDCORE or OPEN. sequence selects a row of the program's animation table; a
 * zero palette keeps each sprite part's CLUT. */
typedef struct display_object_animated {
    s32 flags;          /* 0x00; display_object_flag_e */
    s32 sequence;       /* 0x04 */
    s32 priority;       /* 0x08; ordering-table priority */
    s32 frame_index;    /* 0x0c; current sequence frame */
    s32 anim_counter;   /* 0x10; frames spent on the current frame */
    s32 palette;        /* 0x14 */
    point32_t position; /* 0x18; screen coordinates */
    CVECTOR color;      /* 0x20; RGB tint */
} display_object_animated_t;
typedef char world_animated_render_record_size_must_be_36[sizeof(display_object_animated_t) == 36 ? 1 : -1];

/* Provisional: the column view of the 36-byte WLDCORE window records and OPEN
 * render records, rebased on their x/y pair (+0x18) and keeping the 0x24-byte
 * stride. */
typedef struct window_xy {
    s32 x;
    s32 y;
    u8 _unused_08[0x1c];
} window_xy_t;
typedef char wldcore_window_xy_size_must_be_36[sizeof(window_xy_t) == 36 ? 1 : -1];

#endif
