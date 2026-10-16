#ifndef WLDCORE_WINDOW_H
#define WLDCORE_WINDOW_H

/* WLDCORE's window records: the animated windows and image render records drawn each frame, their animations, the
 * layout table, the list and number panels, and the window state of menu levels. */

#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/wldcore_menu_level.h"
#include "fft/world_sprite.h"
#include "psx/libgpu.h"
#include "psx/libgs.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* window animations */
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

/* The animation tables of the display objects: sequences by window_anim_sequence_e, and the cels they draw. */
extern display_object_sequence_t** g_window_anim_sequences;
extern display_object_cel_header_t** g_window_anim_cels;

/* window and render records */
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
     * wldcore_window_clear_states_and_restore_tint and wldcore_window_set_states_and_blue_tint clear it or set it
     * to 2. */
    s32 palette;
    point32_t base; /* 0x18 */
    point32_t clut; /* 0x20; (0x3c0, 0x1f0) from the pool init */
    /* 0x28; image position and size, copied and passed by value as one struct; the width is read signed */
    RECT bounds;
    CVECTOR color; /* 0x30 */
} window_render_record_t;
typedef char wldcore_window_render_record_size_must_be_52[sizeof(window_render_record_t) == 52 ? 1 : -1];

extern s32 g_window_record_count;
/* The animated window records; g_window_record_positions views their x/y columns. */
extern display_object_animated_t g_window_records[WINDOW_RECORD_CAPACITY];
extern window_xy_t g_window_record_positions[];
extern s32 g_window_render_record_count;
extern window_render_record_t g_window_render_records[WINDOW_RECORD_CAPACITY];
extern u32 g_window_image_buffer[];
void window_init_vram_render_record(s32 index, RECT bounds, point32_t clut, s32 flags);
void window_build_render_record_image(s32 index, RECT bounds, s32 coordinate_mode, u32* image);
void window_load_render_record_image(s32 index, u32* image);
void window_draw_centered_text(s32 window_index, s32 text_id);
void proposition_suspend_detail_level(window_render_pair_t* pair);
void location_suspend_menu_level(menu_stack_slot_set_t* slots);

/* display object queues */
extern s32 g_window_render_object_count;
/* The display objects window_dispatch_record_list draws each frame. Every entry points at the leading flags of a
 * window record, render record or map dot, and the flags select the routine: DISPLAY_OBJECT_ANIMATED for a
 * display_object_animated_t, DISPLAY_OBJECT_IMAGE for a window_render_record_t.
 * window_set_render_objects_visible toggles DISPLAY_OBJECT_BLANK through them. */
extern void* g_window_render_object_queue[];
extern s32 g_window_aux_render_object_count;
extern void* g_window_aux_render_object_queue[];
void window_set_render_objects_visible(s32 clear);
void window_draw_animated_record(display_object_animated_t* object, GsOT* ot);

/* layouts */
/* Provisional: a row of the window-layout table. unit_panel_handle_help_input reads the first ten bytes:
 * a pointer to the level's live counter, the base message id, and the four neighboring layout ids reached by the
 * directional pad. unit_panel_move_help_cursor reads the rest: the record's x and y extents (plus 14 and 12) and the
 * kind that selects the redraw delay. */
typedef struct unit_panel_help_row {
    s16* counter;     /* 0x00 */
    u16 message_base; /* 0x04 */
    u8 up;            /* 0x06 */
    u8 down;          /* 0x07 */
    u8 right;         /* 0x08 */
    u8 left;          /* 0x09 */
    s16 x;            /* 0x0a */
    s16 y;            /* 0x0c */
    s16 kind;         /* 0x0e */
} unit_panel_help_row_t;
typedef char wldcore_window_layout_row_size_must_be_16[sizeof(unit_panel_help_row_t) == 16 ? 1 : -1];

extern unit_panel_help_row_t g_unit_panel_help_rows[];

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
void window_build_yes_no_panel(point32_t origin);
void window_init_panel_state(s32 unused, s32 extent, window_panel_layout_t* layout, point32_t* origin);
void window_start_panel_thread(s32 thread_id, s32 extent, window_panel_layout_t* layout, point32_t* origin);
void window_start_number_panel_thread(s32 thread_id, u32 packed_values, s32 text_id, point32_t* origin);

/* level windows */
/* The window state of menu levels. The resume handlers take the stack record and read the view its level type
 * selects; the rest dim, tint or restore the windows of one kind of level. */
void brave_story_resume_advancing_list(menu_stack_record_t* level);
void brave_story_resume_participant_level(menu_stack_record_t* level);
void sound_novel_resume_quit_level(menu_stack_record_t* level);
void sound_novel_suspend_quit_level(sound_novel_quit_level_t* level);
void world_map_suspend_cursor_level(menu_stack_record_t* level);
void unit_panel_resume_help_level(menu_stack_record_t* level);
void location_resume_menu_level(menu_stack_record_t* level);
void proposition_resume_detail_level(menu_stack_record_t* level);
void brave_story_suspend_advancing_list(brave_story_advancing_level_t* level);
void brave_story_suspend_level(menu_stack_window_level_t* state);
/* Publish handler of the bar menu level: dim its windows (palettes 10, 6, 2) and show the pressed cursor. */
void bar_suspend_level(menu_stack_window_level_t* level);
/* Publish handler of the tutorial categories level; a second copy of the bar menu body. */
void tutorial_suspend_menu_level(menu_stack_window_level_t* level);
void brave_story_suspend_propositions_level(menu_stack_window_level_t* level);
void brave_story_resume_propositions_level(menu_stack_window_level_t* level);
void proposition_suspend_send_unit_level(proposition_send_unit_level_t* level);
void proposition_resume_send_unit_level(proposition_send_unit_level_t* level);

#endif
