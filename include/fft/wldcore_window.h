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
/* Rows of g_wldcore_anim_sequence_table that the code selects: the animation
 * a window record plays. The CURSOR and *_PRESS sequences other than
 * CURSOR_IDLE end on a pinned frame. */
typedef enum wldcore_anim_sequence_id {
    WLDCORE_ANIM_SEQUENCE_CURSOR_STILL = 0, /* list cursor, one pinned frame */
    WLDCORE_ANIM_SEQUENCE_CURSOR_PRESS = 1, /* list cursor after its row is confirmed */
    WLDCORE_ANIM_SEQUENCE_CURSOR_IDLE = 2,  /* list cursor of the level with focus, looping */
    WLDCORE_ANIM_SEQUENCE_LIST_SIDE_LABEL = 4,
    WLDCORE_ANIM_SEQUENCE_MAP_FRAME = 5,
    WLDCORE_ANIM_SEQUENCE_ROOT_MENU_FRAME = 6,
    WLDCORE_ANIM_SEQUENCE_TEXT_PAGE_PROMPT = 8, /* shown while a paged text window has more pages */
    WLDCORE_ANIM_SEQUENCE_TEXT_SCROLL_UP = 9,
    WLDCORE_ANIM_SEQUENCE_TEXT_SCROLL_DOWN = 10,
    WLDCORE_ANIM_SEQUENCE_LOCATION_PICTURE_BORDER = 11,
    WLDCORE_ANIM_SEQUENCE_CHOICE_PANEL_HEADER = 12,
    WLDCORE_ANIM_SEQUENCE_MAP_EDGE_ARROWS = 13, /* four arrows at the screen edges, two alternating frames */
    WLDCORE_ANIM_SEQUENCE_MESSAGE_LABEL = 14,
    WLDCORE_ANIM_SEQUENCE_TEXT_SCROLL_THUMB = 15,
    WLDCORE_ANIM_SEQUENCE_MARKER_FIRST = 16,           /* the player's marker, eight facings */
    WLDCORE_ANIM_SEQUENCE_MARKER_ALTERNATE_FIRST = 24, /* the same eight at the faster frame rate */
    WLDCORE_ANIM_SEQUENCE_MONTH_FIRST = 32,            /* twelve month labels */
    WLDCORE_ANIM_SEQUENCE_DIGIT_FIRST = 44,            /* ten date digits */
    WLDCORE_ANIM_SEQUENCE_LOCATION_NAME_FIRST = 55,    /* one label per map dot */
    WLDCORE_ANIM_SEQUENCE_PROPOSITION_TITLE_LABEL = 99,
    WLDCORE_ANIM_SEQUENCE_PROPOSITION_COUNT_LABEL = 100,
    WLDCORE_ANIM_SEQUENCE_PROPOSITION_DAYS_LABEL = 101,
    WLDCORE_ANIM_SEQUENCE_L1_BUTTON = 102,
    WLDCORE_ANIM_SEQUENCE_L1_BUTTON_PRESS = 103,
    WLDCORE_ANIM_SEQUENCE_R1_BUTTON = 104,
    WLDCORE_ANIM_SEQUENCE_R1_BUTTON_PRESS = 105,
    WLDCORE_ANIM_SEQUENCE_MAP_DOT = 106, /* the next row is the dot of a kind-2 location */
    WLDCORE_ANIM_SEQUENCE_SOUND_NOVEL_HISTORY_UPPER = 108,
    WLDCORE_ANIM_SEQUENCE_SOUND_NOVEL_HISTORY_LOWER = 109,
    WLDCORE_ANIM_SEQUENCE_SOUND_NOVEL_START_BUTTON = 110,
    WLDCORE_ANIM_SEQUENCE_SOUND_NOVEL_START_BUTTON_PRESS = 111,
    WLDCORE_ANIM_SEQUENCE_SOUND_NOVEL_SQUARE_BUTTON = 112,
    WLDCORE_ANIM_SEQUENCE_SOUND_NOVEL_SQUARE_BUTTON_PRESS = 113,
    WLDCORE_ANIM_SEQUENCE_SOUND_NOVEL_OVERLAY = 114,
    WLDCORE_ANIM_SEQUENCE_SEND_UNIT_FRAME_A = 115,
    WLDCORE_ANIM_SEQUENCE_SEND_UNIT_FRAME_B = 116,
    WLDCORE_ANIM_SEQUENCE_SEND_UNIT_PORTRAIT = 117,
    WLDCORE_ANIM_SEQUENCE_LAYOUT_CURSOR_IDLE = 120, /* the layout windows' cursor, looping */
    WLDCORE_ANIM_SEQUENCE_BRAVE_STORY_MARKER = 121, /* the brave-story list's two completion markers */
} wldcore_anim_sequence_id_e;

/* The animation tables of the display objects: sequences by wldcore_anim_sequence_id_e, and the cels they draw. */
extern world_sprite_animation_sequence_t** g_wldcore_anim_sequence_table;
extern world_sprite_cel_header_t** g_wldcore_anim_cel_table;

/* window and render records */
enum { WLDCORE_WINDOW_RECORD_CAPACITY = 32 };

/* A 52-byte image render record (g_wldcore_window_render_records). window_init_record_pools sets its flags to
 * WORLD_RENDER_RECORD_IMAGE, so window_dispatch_record_list draws it as a sprite through
 * wldcore_gfx_draw_display_object_sprite. */
typedef struct wldcore_window_render_record {
    s32 flags; /* 0x00; world_render_record_flag_e */
    s32 tpage;
    s32 priority; /* 0x08; OT priority passed to the sprite sort; 1 from the pool init */
    s32 coordinate_mode;
    s32 anim_counter; /* 0x10; WORLD_RENDER_RECORD_POP_IN animation counter, cleared on every rebind */
    /* 0x14; CLUT selector: nonzero gives cx = clut_x + (palette - 1) << 4.
     * wldcore_window_clear_states_and_restore_tint and wldcore_window_set_states_and_blue_tint clear it or set it
     * to 2. */
    s32 palette;
    point32_t base; /* 0x18 */
    point32_t clut; /* 0x20; (0x3c0, 0x1f0) from the pool init */
    /* 0x28; image position and size, copied and passed by value as one struct; the width is read signed */
    RECT bounds;
    CVECTOR color; /* 0x30 */
} wldcore_window_render_record_t;
typedef char wldcore_window_render_record_size_must_be_52[sizeof(wldcore_window_render_record_t) == 52 ? 1 : -1];

extern s32 g_wldcore_window_record_count;
/* The animated window records; g_wldcore_window_record_positions views their x/y columns. */
extern world_animated_render_record_t g_wldcore_window_records[WLDCORE_WINDOW_RECORD_CAPACITY];
extern wldcore_window_xy_t g_wldcore_window_record_positions[];
extern s32 g_wldcore_window_render_record_count;
extern wldcore_window_render_record_t g_wldcore_window_render_records[WLDCORE_WINDOW_RECORD_CAPACITY];
extern u32 g_wldcore_window_image_buffer[];
void wldcore_window_init_vram_render_record(s32 index, RECT bounds, point32_t clut, s32 flags);
void wldcore_window_build_render_record_image(s32 index, RECT bounds, s32 coordinate_mode, u32* image);
void wldcore_window_load_image_record_to_vram(s32 index, u32* image);
void wldcore_window_draw_centered_text(s32 window_index, s32 text_id);
void wldcore_menu_suspend_proposition_detail_level(wldcore_window_render_pair_t* pair);
void wldcore_menu_suspend_location_menu_level(wldcore_menu_slot_set_t* slots);

/* display object queues */
extern s32 g_wldcore_window_render_object_count;
/* The display objects window_dispatch_record_list draws each frame. Every entry points at the leading flags of a
 * window record, render record or map dot, and the flags select the routine: WORLD_RENDER_RECORD_ANIMATED for a
 * world_animated_render_record_t, WORLD_RENDER_RECORD_IMAGE for a wldcore_window_render_record_t.
 * wldcore_window_set_render_objects_visible toggles WORLD_RENDER_RECORD_BLANK through them. */
extern void* g_wldcore_window_render_object_queue[];
extern s32 g_wldcore_window_aux_render_object_count;
extern void* g_wldcore_window_aux_render_object_queue[];
void wldcore_window_set_render_objects_visible(s32 clear);
void wldcore_gfx_draw_animated_display_object(world_animated_render_record_t* object, GsOT* ot);

/* layouts */
/* Provisional: a row of the window-layout table. wldcore_menu_handle_layout_window_input reads the first ten bytes:
 * a pointer to the level's live counter, the base message id, and the four neighboring layout ids reached by the
 * directional pad. wldcore_window_apply_layout reads the rest: the record's x and y extents (plus 14 and 12) and the
 * kind that selects the redraw delay. */
typedef struct wldcore_window_layout_row {
    s16* counter;     /* 0x00 */
    u16 message_base; /* 0x04 */
    u8 up;            /* 0x06 */
    u8 down;          /* 0x07 */
    u8 right;         /* 0x08 */
    u8 left;          /* 0x09 */
    s16 x;            /* 0x0a */
    s16 y;            /* 0x0c */
    s16 kind;         /* 0x0e */
} wldcore_window_layout_row_t;
typedef char wldcore_window_layout_row_size_must_be_16[sizeof(wldcore_window_layout_row_t) == 16 ? 1 : -1];

extern wldcore_window_layout_row_t g_wldcore_window_layout_rows[];

/* panels */
/* Text width is in pixels; the second value counts visible list rows. */
typedef struct wldcore_window_panel_layout {
    s32 text_width;
    s32 visible_rows;
} wldcore_window_panel_layout_t;

/* The panel renderer's entry (g_wldcore_window_panel_render_state), filled by
 * wldcore_window_init_panel_render_state and wldcore_window_init_number_panel_render_thread and read by
 * world_screen_build_at_list. Its text binding is the label layout g_wldcore_window_panel_label_state (column mode
 * 0 text, 2 hidden, 3 alternate number) or, for number panels, g_wldcore_window_panel_number_state. */
extern menu_text_layout_t g_wldcore_window_panel_label_state;
extern menu_entry_t g_wldcore_window_panel_render_state;
extern struct world_menu_number_range g_wldcore_window_panel_number_state;
void wldcore_window_build_yes_no_panel(point32_t origin);
void wldcore_window_init_panel_render_state(
    s32 unused, s32 extent, wldcore_window_panel_layout_t* layout, point32_t* origin);
void wldcore_window_start_panel_render_thread(
    s32 thread_id, s32 extent, wldcore_window_panel_layout_t* layout, point32_t* origin);
void wldcore_window_init_number_panel_render_thread(s32 thread_id, u32 packed_values, s32 text_id, point32_t* origin);

/* level windows */
/* The window state of menu levels. The resume handlers take the stack record and read the view its level type
 * selects; the rest dim, tint or restore the windows of one kind of level. */
void wldcore_menu_resume_active_propositions_level(wldcore_menu_stack_record_t* level);
void wldcore_menu_resume_participant_detail_level(wldcore_menu_stack_record_t* level);
void wldcore_menu_resume_sound_novel_quit_level(wldcore_menu_stack_record_t* level);
void wldcore_menu_suspend_sound_novel_quit_level(wldcore_menu_sound_novel_quit_level_t* level);
void wldcore_menu_suspend_root_level(wldcore_menu_stack_record_t* level);
void wldcore_menu_resume_layout_window_level(wldcore_menu_stack_record_t* level);
void wldcore_menu_resume_location_menu_level(wldcore_menu_stack_record_t* level);
void wldcore_menu_resume_proposition_detail_level(wldcore_menu_stack_record_t* level);
void wldcore_menu_suspend_active_propositions_level(wldcore_menu_proposition_list_level_t* level);
void wldcore_menu_suspend_brave_story_level(wldcore_menu_list_window_level_t* state);
/* Publish handler of the bar menu level: dim its windows (palettes 10, 6, 2) and show the pressed cursor. */
void wldcore_menu_suspend_bar_menu_level(wldcore_menu_list_window_level_t* level);
/* Publish handler of the tutorial categories level; a second copy of the bar menu body. */
void wldcore_menu_suspend_tutorial_categories_level(wldcore_menu_list_window_level_t* level);
void wldcore_menu_suspend_proposition_category_level(wldcore_menu_list_window_level_t* level);
void wldcore_menu_resume_proposition_category_level(wldcore_menu_list_window_level_t* level);
void wldcore_menu_suspend_proposition_send_unit_level(wldcore_menu_send_unit_level_t* level);
void wldcore_menu_resume_proposition_send_unit_level(wldcore_menu_send_unit_level_t* level);

#endif
