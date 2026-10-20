#ifndef WINDOW_H
#define WINDOW_H

/* Framed text images, paged text windows and menu panels. */

#include "fft/gfx.h"
#include "fft/menu.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* window images */
extern u32 g_window_image_buffer[];

/* Each program builds and uploads images through its own window record layout. */
void window_build_render_record_image(s32 index, RECT bounds, s32 coordinate_mode, u32* image);
void window_load_render_record_image(s32 index, u32* image);
void window_text_record_init(s32 record_index, RECT quad, s32 image_placement, s32 combined_text_index, u32* image);
void window_display_text_entry(s32 record_index, s32 combined_text_index, point32_t position, u32* image);

/* OPEN measures character columns; WLDCORE measures proportional pixel widths. */
void window_text_image_measure(s32 combined_text_index, size32_t* dimensions);
void window_centered_text_draw(s32 window_index, s32 text_id);

/* text window */
/* Values of text_window_t.scroll_state. */
typedef enum text_window_scroll {
    TEXT_WINDOW_SCROLL_PAGING = 0,
    TEXT_WINDOW_SCROLL_MANUAL = 1, /* while square is held: up and down scroll by rows */
    TEXT_WINDOW_SCROLL_RETURN = 2  /* scrolls back to the page start */
} text_window_scroll_e;

/* A paged text window and its four decorative window records. */
typedef struct text_window {
    point32_t image_position; /* 0x00; image origin passed to window_build_render_record_image */
    point32_t base;
    s32 text_width;
    s32 rows_per_page;
    s32 measured_columns;
    s32 measured_rows;
    /* 0x20..0x2e: copy of the optional image object's position and bounds, taken as two block copies;
     * page_render scrolls extra_base.y by row * 16 into the object's position and clips the
     * record's y and height from the bounds. */
    point32_t extra_base; /* 0x20 */
    RECT extra_bounds;    /* 0x28 */
    s32 scroll_state;     /* 0x30; text_window_scroll_e */
    s32 text_id;
    s32 page_prompt_record;
    s32 up_arrow_record;
    s32 down_arrow_record;
    s32 render_record_index;
    s32 scroll_thumb_record;
    s32 priority; /* 0x4c; OT priority of the text window's records (animated_record_init) */
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
s32 text_window_update(text_window_t* window);

/* panels */
/* Text width is in pixels; the second value counts visible list rows. */
typedef struct window_panel_layout {
    s32 text_width;
    s32 visible_rows;
} window_panel_layout_t;

/* The panel renderer's entry (g_window_panel_render_state), filled by
 * window_panel_state_init and window_number_panel_thread_start and read by
 * screen_text_list_thread. Its text binding is the label layout g_window_panel_label_state (column mode
 * 0 text, 2 hidden, 3 alternate number) or, for number panels, g_window_panel_number_state. */
extern menu_text_layout_t g_window_panel_label_state;
extern menu_entry_t g_window_panel_render_state;
extern s16 g_window_panel_value;

void window_yes_no_panel_build(point32_t origin);
void window_panel_state_init(s32 unused, s32 extent, window_panel_layout_t* layout, point32_t* origin);
void window_panel_thread_start(s32 thread_id, s32 extent, window_panel_layout_t* layout, point32_t* origin);
void window_number_panel_thread_start(s32 thread_id, u32 packed_bounds, s32 text_id, point32_t* origin);

#endif
