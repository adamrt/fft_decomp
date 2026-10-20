#ifndef DISPLAY_OBJECT_H
#define DISPLAY_OBJECT_H

/* Animated and image display objects, serialized cels, pools and render queues. */

#include "fft/gfx.h"
#include "psx/libgpu.h"
#include "psx/libgs.h"
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
typedef char display_object_frame_size_must_be_4[sizeof(display_object_frame_t) == 4 ? 1 : -1];

/* Serialized animation header, followed immediately by frame_count four-byte frame records. */
typedef struct display_object_sequence_header {
    s32 frame_count;                  /* 0x00 */
    display_object_frame_t frames[0]; /* variable-length serialized frame records */
} display_object_sequence_header_t;
typedef char display_object_sequence_header_size_must_be_4[sizeof(display_object_sequence_header_t) == 4 ? 1 : -1];

/* Serialized cel header, followed by part_count eight-byte sprite parts.
 * Origins pack the x coordinate in the low half and y in the high half. */
typedef struct display_object_cel_header {
    u32 part_count;
    u32 packed_texture_page_origin;
    u32 packed_clut_origin;
} display_object_cel_header_t;
typedef char display_object_cel_header_size_must_be_12[sizeof(display_object_cel_header_t) == 12 ? 1 : -1];

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
typedef char display_object_part_size_must_be_8[sizeof(display_object_part_t) == 8 ? 1 : -1];

/* display objects */
/* 32-bit flags that lead every animated or image display object in WLDCORE and OPEN.
 * The two kind bits select the draw
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
    /* WLDCORE animated object: steps its frame counter even while WORLD_MAP_FLAG_FREEZE_ANIMATION is set. */
    DISPLAY_OBJECT_ANIMATE_WHILE_PAUSED = 0x800
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
typedef char display_object_animated_size_must_be_36[sizeof(display_object_animated_t) == 36 ? 1 : -1];

/* Retained compiler stride view of display_object_animated_t.position.
 * It advances between x/y pairs at the 36-byte record stride; it is not a
 * separate allocation or a recovered original record type. */
typedef struct display_object_position_stride {
    s32 x;
    s32 y;
    u8 _unused_08[0x1c];
} display_object_position_stride_t;
typedef char display_object_position_stride_size_must_be_36[sizeof(display_object_position_stride_t) == 36 ? 1 : -1];

/* gpu image records */
enum { DISPLAY_OBJECT_GPU_POOL_CAPACITY = 16 };

/* OPEN emits GPU packets from full VRAM coordinates. image_page_offset selects
 * 64-halfword pages relative to x=0x180; color depth comes from the flags. */
typedef struct display_object_gpu_image {
    u32 flags;               /* 0x00; display_object_flag_e */
    point32_t vram_position; /* 0x04; texture-page origin used by GetTPage */
    s32 priority;            /* 0x0c */
    s32 image_page_offset;   /* 0x10 */
    s32 pop_in_step;         /* 0x14 */
    s32 palette;             /* 0x18; nonzero selects CLUT x + (palette - 1) * 16 */
    point32_t position;      /* 0x1c; centered screen position */
    point32_t clut;          /* 0x24 */
    RECT bounds;             /* 0x2c; texel position and image dimensions */
    CVECTOR color;           /* 0x34 */
} display_object_gpu_image_t;
typedef char display_object_gpu_image_size_must_be_56[sizeof(display_object_gpu_image_t) == 56 ? 1 : -1];

extern display_object_gpu_image_t g_display_object_gpu_image_pool[DISPLAY_OBJECT_GPU_POOL_CAPACITY];
void display_object_gpu_image_init(
    s32 record_index, point16_t vram_position, point16_t image_size, point32_t clut_position, u32 flags);
void display_object_gpu_list_draw(u32* otag, void** records, s32 count);

/* libgs image records */
enum { DISPLAY_OBJECT_GS_POOL_CAPACITY = 32 };

/* WLDCORE stores the packed texture page used by its libgs sprite renderer.
 * coordinate_mode also selects the image uploader's VRAM mapping. */
typedef struct display_object_gs_image {
    s32 flags;           /* 0x00; display_object_flag_e */
    s32 tpage;           /* 0x04 */
    s32 priority;        /* 0x08; ordering-table priority, initialized to 1 */
    s32 coordinate_mode; /* 0x0c */
    s32 pop_in_step;     /* 0x10; pop-in animation counter */
    s32 palette;         /* 0x14; nonzero selects CLUT x + (palette - 1) * 16 */
    point32_t position;  /* 0x18; centered screen position */
    point32_t clut;      /* 0x20 */
    RECT bounds;         /* 0x28; texel position and image dimensions */
    CVECTOR color;       /* 0x30 */
} display_object_gs_image_t;
typedef char display_object_gs_image_size_must_be_52[sizeof(display_object_gs_image_t) == 52 ? 1 : -1];

extern display_object_gs_image_t g_display_object_gs_image_pool[DISPLAY_OBJECT_GS_POOL_CAPACITY];
void display_object_gs_image_init(s32 index, RECT bounds, point32_t clut, s32 flags);
void display_object_gs_list_draw(GsOT* otag, void** records, s32 count);
void display_object_gs_animated_draw(display_object_animated_t* object, GsOT* ot);

/* animation catalog (WLDCORE) */
/* Rows of g_display_object_sequences that the code selects: the animation
 * a window record plays. The CURSOR and *_PRESS sequences other than
 * CURSOR_IDLE end on a pinned frame. */
typedef enum display_object_animation {
    DISPLAY_OBJECT_ANIM_CURSOR_STILL = 0, /* list cursor, one pinned frame */
    DISPLAY_OBJECT_ANIM_CURSOR_PRESS = 1, /* list cursor after its row is confirmed */
    DISPLAY_OBJECT_ANIM_CURSOR_IDLE = 2,  /* list cursor of the level with focus, looping */
    DISPLAY_OBJECT_ANIM_LIST_SIDE_LABEL = 4,
    DISPLAY_OBJECT_ANIM_MAP_FRAME = 5,
    DISPLAY_OBJECT_ANIM_ROOT_MENU_FRAME = 6,
    DISPLAY_OBJECT_ANIM_TEXT_PAGE_PROMPT = 8, /* shown while a paged text window has more pages */
    DISPLAY_OBJECT_ANIM_TEXT_SCROLL_UP = 9,
    DISPLAY_OBJECT_ANIM_TEXT_SCROLL_DOWN = 10,
    DISPLAY_OBJECT_ANIM_LOCATION_PICTURE_BORDER = 11,
    DISPLAY_OBJECT_ANIM_CHOICE_PANEL_HEADER = 12,
    DISPLAY_OBJECT_ANIM_MAP_EDGE_ARROWS = 13, /* four arrows at the screen edges, two alternating frames */
    DISPLAY_OBJECT_ANIM_MESSAGE_LABEL = 14,
    DISPLAY_OBJECT_ANIM_TEXT_SCROLL_THUMB = 15,
    DISPLAY_OBJECT_ANIM_MARKER_FIRST = 16,           /* the player's marker, eight facings */
    DISPLAY_OBJECT_ANIM_MARKER_ALTERNATE_FIRST = 24, /* the same eight at the faster frame rate */
    DISPLAY_OBJECT_ANIM_MONTH_FIRST = 32,            /* twelve month labels */
    DISPLAY_OBJECT_ANIM_DIGIT_FIRST = 44,            /* ten date digits */
    DISPLAY_OBJECT_ANIM_LOCATION_NAME_FIRST = 55,    /* one label per map dot */
    DISPLAY_OBJECT_ANIM_PROPOSITION_TITLE_LABEL = 99,
    DISPLAY_OBJECT_ANIM_PROPOSITION_COUNT_LABEL = 100,
    DISPLAY_OBJECT_ANIM_PROPOSITION_DAYS_LABEL = 101,
    DISPLAY_OBJECT_ANIM_L1_BUTTON = 102,
    DISPLAY_OBJECT_ANIM_L1_BUTTON_PRESS = 103,
    DISPLAY_OBJECT_ANIM_R1_BUTTON = 104,
    DISPLAY_OBJECT_ANIM_R1_BUTTON_PRESS = 105,
    DISPLAY_OBJECT_ANIM_MAP_DOT = 106, /* the next row is the dot of a kind-2 location */
    DISPLAY_OBJECT_ANIM_SOUND_NOVEL_HISTORY_UPPER = 108,
    DISPLAY_OBJECT_ANIM_SOUND_NOVEL_HISTORY_LOWER = 109,
    DISPLAY_OBJECT_ANIM_SOUND_NOVEL_START_BUTTON = 110,
    DISPLAY_OBJECT_ANIM_SOUND_NOVEL_START_BUTTON_PRESS = 111,
    DISPLAY_OBJECT_ANIM_SOUND_NOVEL_SQUARE_BUTTON = 112,
    DISPLAY_OBJECT_ANIM_SOUND_NOVEL_SQUARE_BUTTON_PRESS = 113,
    DISPLAY_OBJECT_ANIM_SOUND_NOVEL_OVERLAY = 114,
    DISPLAY_OBJECT_ANIM_SEND_UNIT_FRAME_A = 115,
    DISPLAY_OBJECT_ANIM_SEND_UNIT_FRAME_B = 116,
    DISPLAY_OBJECT_ANIM_SEND_UNIT_PORTRAIT = 117,
    DISPLAY_OBJECT_ANIM_LAYOUT_CURSOR_IDLE = 120, /* the layout windows' cursor, looping */
    DISPLAY_OBJECT_ANIM_BRAVE_STORY_MARKER = 121  /* the Brave-story list's two completion markers */
} display_object_animation_e;

/* record pools */
/* OPEN and WLDCORE each define this animated pool and the two allocation counts.
 * OPEN has 16 entries in each pool; WLDCORE has 32. */
extern display_object_animated_t g_display_object_animated_pool[];
extern s32 g_display_object_animated_count;
extern s32 g_display_object_image_count;

/* Claim a pool entry, append its pointer to the supplied queue and return its pool index. */
s32 display_object_animated_append(void** list, s32* count);
s32 display_object_image_append(void** list, s32* count);
void display_object_pools_init(void);

/* display object queues */
/* OPEN and WLDCORE each publish their display objects to this queue. Every entry points at the leading flags of a
 * display object, and the flags select the routine: DISPLAY_OBJECT_ANIMATED for a
 * display_object_animated_t, DISPLAY_OBJECT_IMAGE for that program's image record.
 * display_object_queues_set_enabled toggles DISPLAY_OBJECT_BLANK through them. */
extern void* g_display_object_queue[];
extern s32 g_display_object_queue_count;

/* WLDCORE's separate auxiliary queue. */
extern void* g_display_object_aux_queue[];
extern s32 g_display_object_aux_queue_count;

void display_object_queues_set_enabled(s32 enabled);

#endif
