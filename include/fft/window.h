#ifndef WINDOW_H
#define WINDOW_H

/* Window render records, the display objects OPEN and WLDCORE build their windows and text from (each program
 * links its own revision: 56-byte image records in OPEN, 52-byte in WLDCORE), their window animations,
 * display-object queues, paged text window and panels. */

#include "fft/gfx.h"
#include "fft/menu.h"
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

/* OPEN and WLDCORE each define these tables with their own sequence catalog. */
extern display_object_sequence_t** g_window_anim_sequences;
extern display_object_cel_header_t** g_window_anim_cels;

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
    /* WLDCORE animated object: steps its frame counter even while SYSTEM_FLAG_FREEZE_ANIMATION is set. */
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
typedef char display_object_animated_size_must_be_36[sizeof(display_object_animated_t) == 36 ? 1 : -1];

/* Provisional: the column view of the 36-byte WLDCORE window records and OPEN
 * render records, rebased on their x/y pair (+0x18) and keeping the 0x24-byte
 * stride. */
typedef struct window_xy {
    s32 x;
    s32 y;
    u8 _unused_08[0x1c];
} window_xy_t;
typedef char window_xy_size_must_be_36[sizeof(window_xy_t) == 36 ? 1 : -1];

/* record lists */
s32 window_append_animated_record(void** list, s32* count);
s32 window_append_record(void** list, s32* count);
void window_dispatch_record_list(GsOT* otag, void** records, s32 count);

/* Each program builds and uploads images through its own window record layout. */
void window_build_render_record_image(s32 index, RECT bounds, s32 coordinate_mode, u32* image);
void window_load_render_record_image(s32 index, u32* image);
void window_init_text_record(s32 record_index, RECT quad, s32 depth, s32 combined_text_index, u32* image);
void window_display_text_entry(s32 record_index, s32 combined_text_index, point32_t position, u32* image);
void window_init_record_pools(void);

/* render records */
/* window_dispatch_record_list dispatches on the leading flags: ANIMATED selects
 * a 36-byte record, IMAGE selects the program's image record, and BLANK or
 * HIDDEN skips the record. Each pool holds WINDOW_RECORD_CAPACITY entries. */
/* CLUT position and source rectangle at +0x24 of a 56-byte record.
 * window_build_render_record_image takes its address as one pointer. */
typedef struct window_render_record_tail {
    /* 0x00 */ point32_t clut; /* window_draw_image_record_56 passes it to GetClut */
    /* 0x08 */ RECT quad;      /* sprite u, v, w, h: texels within the texture page */
} window_render_record_tail_t;

/* OPEN stores VRAM coordinates directly; WLDCORE stores a packed texture page. */
#ifdef FFT_OPEN
enum { WINDOW_RECORD_CAPACITY = 16 };

typedef struct window_render_record {
    /* 0x00 */ u32 flags;
    /* 0x04 */ s32 vram_x; /* quad.x / 4 + 64 * depth + 0x180 */
    /* 0x08 */ s32 vram_y;
    /* 0x0c */ s32 ot_layer;
    /* 0x10 */ s32 depth;
    /* 0x14 */ s32 pop_in_step;
    /* 0x18 */ s32 palette;
    /* 0x1c */ s32 x; /* quad.x - 0x80 */
    /* 0x20 */ s32 y; /* quad.y - 0x78 */
    /* 0x24 */ window_render_record_tail_t tail;
    /* 0x34: a sibling of `tail`, not part of it. window_build_render_record_image
     * computes its address as records + index * 0x38 + 0x34, which a nested
     * `&rec[i].tail.r` cannot give (CSE rewrites that as tail + 0x10). */
    CVECTOR color;
} window_render_record_t;
typedef char window_render_record_size_must_be_56[(sizeof(window_render_record_t) == 56) ? 1 : -1];
#else
enum { WINDOW_RECORD_CAPACITY = 32 };

/* A 52-byte image render record (g_window_render_records). window_init_record_pools sets its flags to
 * DISPLAY_OBJECT_IMAGE, so window_dispatch_record_list draws it as a sprite through
 * window_draw_image_record. */
typedef struct window_render_record {
    s32 flags; /* 0x00; display_object_flag_e */
    s32 tpage;
    s32 priority; /* 0x08; OT priority passed to the sprite sort; 1 from the pool init */
    s32 coordinate_mode;
    s32 anim_counter; /* 0x10; DISPLAY_OBJECT_POP_IN animation counter, cleared on every rebind */
    /* 0x14; CLUT selector: nonzero gives cx = clut_x + (palette - 1) << 4.
     * proposition_resume_send_unit_level and proposition_suspend_send_unit_level clear it or set it
     * to 2. */
    s32 palette;
    point32_t base; /* 0x18 */
    point32_t clut; /* 0x20; (0x3c0, 0x1f0) from the pool init */
    /* 0x28; image position and size, copied and passed by value as one struct; the width is read signed */
    RECT bounds;
    CVECTOR color; /* 0x30 */
} window_render_record_t;
typedef char window_render_record_size_must_be_52[sizeof(window_render_record_t) == 52 ? 1 : -1];
#endif

/* x/y view of OPEN's image records, based at g_window_render_records[0].x; the
 * retail code addresses that pair through its own base register, loaded
 * before the index is scaled, which &g_window_render_records[i].x does not reproduce. */
typedef struct window_record_56_position {
    /* 0x00 */ s32 x; /* == g_window_render_records[i].x */
    /* 0x04 */ s32 y;
    /* 0x08 */ u8 _unused_08[0x38 - 8];
} window_record_56_position_t;

/* One DR_MODE + SPRT pair of the shared 32-entry per-buffer sprite pool. */
typedef struct window_sprite_prim {
    /* 0x00 */ DR_MODE mode;
    /* 0x0c */ SPRT sprt;
} window_sprite_prim_t;
typedef char window_sprite_prim_size_must_be_32[(sizeof(window_sprite_prim_t) == 32) ? 1 : -1];

extern window_record_56_position_t g_window_record_translation[WINDOW_RECORD_CAPACITY];
extern window_sprite_prim_t g_window_sprite_primitive_pool[2][32];
extern s32 g_window_primitive_count;

void window_init_vram_record(
    s32 record_index, point16_t vram_position, point16_t image_size, point32_t clut_position, u32 flags);

/* OPEN measures character columns; WLDCORE measures proportional pixel widths. */
void window_measure_text_image(s32 combined_text_index, size32_t* dimensions);

/* window animations (WLDCORE) */
/* Rows of g_window_anim_sequences that the code selects: the animation
 * a window record plays. The CURSOR and *_PRESS sequences other than
 * CURSOR_IDLE end on a pinned frame. */
typedef enum window_anim_sequence {
    WINDOW_ANIM_CURSOR_STILL = 0, /* list cursor, one pinned frame */
    WINDOW_ANIM_CURSOR_PRESS = 1, /* list cursor after its row is confirmed */
    WINDOW_ANIM_CURSOR_IDLE = 2,  /* list cursor of the level with focus, looping */
    WINDOW_ANIM_LIST_SIDE_LABEL = 4,
    WINDOW_ANIM_MAP_FRAME = 5,
    WINDOW_ANIM_ROOT_MENU_FRAME = 6,
    WINDOW_ANIM_TEXT_PAGE_PROMPT = 8, /* shown while a paged text window has more pages */
    WINDOW_ANIM_TEXT_SCROLL_UP = 9,
    WINDOW_ANIM_TEXT_SCROLL_DOWN = 10,
    WINDOW_ANIM_LOCATION_PICTURE_BORDER = 11,
    WINDOW_ANIM_CHOICE_PANEL_HEADER = 12,
    WINDOW_ANIM_MAP_EDGE_ARROWS = 13, /* four arrows at the screen edges, two alternating frames */
    WINDOW_ANIM_MESSAGE_LABEL = 14,
    WINDOW_ANIM_TEXT_SCROLL_THUMB = 15,
    WINDOW_ANIM_MARKER_FIRST = 16,           /* the player's marker, eight facings */
    WINDOW_ANIM_MARKER_ALTERNATE_FIRST = 24, /* the same eight at the faster frame rate */
    WINDOW_ANIM_MONTH_FIRST = 32,            /* twelve month labels */
    WINDOW_ANIM_DIGIT_FIRST = 44,            /* ten date digits */
    WINDOW_ANIM_LOCATION_NAME_FIRST = 55,    /* one label per map dot */
    WINDOW_ANIM_PROPOSITION_TITLE_LABEL = 99,
    WINDOW_ANIM_PROPOSITION_COUNT_LABEL = 100,
    WINDOW_ANIM_PROPOSITION_DAYS_LABEL = 101,
    WINDOW_ANIM_L1_BUTTON = 102,
    WINDOW_ANIM_L1_BUTTON_PRESS = 103,
    WINDOW_ANIM_R1_BUTTON = 104,
    WINDOW_ANIM_R1_BUTTON_PRESS = 105,
    WINDOW_ANIM_MAP_DOT = 106, /* the next row is the dot of a kind-2 location */
    WINDOW_ANIM_SOUND_NOVEL_HISTORY_UPPER = 108,
    WINDOW_ANIM_SOUND_NOVEL_HISTORY_LOWER = 109,
    WINDOW_ANIM_SOUND_NOVEL_START_BUTTON = 110,
    WINDOW_ANIM_SOUND_NOVEL_START_BUTTON_PRESS = 111,
    WINDOW_ANIM_SOUND_NOVEL_SQUARE_BUTTON = 112,
    WINDOW_ANIM_SOUND_NOVEL_SQUARE_BUTTON_PRESS = 113,
    WINDOW_ANIM_SOUND_NOVEL_OVERLAY = 114,
    WINDOW_ANIM_SEND_UNIT_FRAME_A = 115,
    WINDOW_ANIM_SEND_UNIT_FRAME_B = 116,
    WINDOW_ANIM_SEND_UNIT_PORTRAIT = 117,
    WINDOW_ANIM_LAYOUT_CURSOR_IDLE = 120, /* the layout windows' cursor, looping */
    WINDOW_ANIM_BRAVE_STORY_MARKER = 121, /* the brave-story list's two completion markers */
} window_anim_sequence_e;

/* window and render records */
/* OPEN and WLDCORE each define animated and image pools with separate allocation counts.
 * The image layout and both pool capacities differ by program. */
extern display_object_animated_t g_window_records[WINDOW_RECORD_CAPACITY];
extern window_render_record_t g_window_render_records[WINDOW_RECORD_CAPACITY];
extern s32 g_window_record_count;
extern s32 g_window_render_record_count;
extern u32 g_window_image_buffer[];

/* WLDCORE's x/y column view of the animated records. */
extern window_xy_t g_window_record_positions[];

void window_init_vram_render_record(s32 index, RECT bounds, point32_t clut, s32 flags);
void window_draw_centered_text(s32 window_index, s32 text_id);

/* display object queues */
/* OPEN and WLDCORE each publish their display objects to this queue. Every entry points at the leading flags of a
 * window record, render record or map dot, and the flags select the routine: DISPLAY_OBJECT_ANIMATED for a
 * display_object_animated_t, DISPLAY_OBJECT_IMAGE for a window_render_record_t.
 * window_set_render_objects_visible toggles DISPLAY_OBJECT_BLANK through them. */
extern void* g_window_render_object_queue[];
extern s32 g_window_render_object_count;

/* WLDCORE's separate auxiliary queue. */
extern void* g_window_aux_render_object_queue[];
extern s32 g_window_aux_render_object_count;

void window_set_render_objects_visible(s32 clear);
void window_draw_animated_record(display_object_animated_t* object, GsOT* ot);

/* text window */
/* Values of text_window_t.scroll_state. */
typedef enum text_window_scroll {
    TEXT_WINDOW_SCROLL_PAGING = 0,
    TEXT_WINDOW_SCROLL_MANUAL = 1, /* while square is held: up and down scroll by rows */
    TEXT_WINDOW_SCROLL_RETURN = 2, /* scrolls back to the page start */
} text_window_scroll_e;

/* A paged text window and its four decorative window records. */
typedef struct text_window {
    s32 image_x; /* 0x00; text image position.x for window_build_render_record_image; cleared by the setups */
    s32 image_y; /* 0x04; text image position.y; 0x70 or 0x60 from the setups */
    point32_t base;
    s32 text_width;
    s32 rows_per_page;
    s32 measured_columns;
    s32 measured_rows;
    /* 0x20..0x2e: copy of the optional extra render record's base and bounds, taken as two block copies;
     * text_window_render_page scrolls base.y by row * 16 into the record's base and clips the
     * record's y and height from the bounds. */
    point32_t extra_base; /* 0x20 */
    RECT extra_bounds;    /* 0x28 */
    s32 scroll_state;     /* 0x30; text_window_scroll_e */
    s32 text_id;
    s32 left_window_index;
    s32 middle_window_index;
    s32 right_window_index;
    s32 render_record_index;
    s32 lower_window_index;
    s32 priority; /* 0x4c; OT priority of the text window's records (window_init_animated_record) */
    s32 last_page_row;
    s32 page_start_row;
    s32 scroll_row;            /* 0x58; row shown while scrolling (MANUAL and RETURN) */
    s32 image_coordinate_mode; /* 0x5c; coordinate mode of the text render record image */
    s32 scroll_timer;          /* 0x60; frame counter of the auto-scroll state */
    s32 extra_render_index;    /* 0x64; optional render record scrolled with the text, -1 if none */
    s32 text_substitutions[2]; /* 0x68; copied to the text substitution values unless -1 */
} text_window_t;
typedef char text_window_size_must_be_112[sizeof(text_window_t) == 112 ? 1 : -1];

extern text_window_t g_text_window;

void text_window_open(text_window_t* state);
s32 text_window_step(text_window_t* window);

/* panels */
/* Text width is in pixels; the second value counts visible list rows. */
typedef struct window_panel_layout {
    s32 text_width;
    s32 visible_rows;
} window_panel_layout_t;

/* The panel renderer's entry (g_window_panel_render_state), filled by
 * window_init_panel_state and window_start_number_panel_thread and read by
 * screen_text_list_thread. Its text binding is the label layout g_window_panel_label_state (column mode
 * 0 text, 2 hidden, 3 alternate number) or, for number panels, g_window_panel_number_state. */
extern menu_text_layout_t g_window_panel_label_state;
extern menu_entry_t g_window_panel_render_state;
extern struct screen_number_range g_window_panel_number_state;
extern s16 g_window_panel_value;

void window_build_yes_no_panel(point32_t origin);
void window_init_panel_state(s32 unused, s32 extent, window_panel_layout_t* layout, point32_t* origin);
void window_start_panel_thread(s32 thread_id, s32 extent, window_panel_layout_t* layout, point32_t* origin);
void window_start_number_panel_thread(s32 thread_id, u32 packed_values, s32 text_id, point32_t* origin);

#endif
