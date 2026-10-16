#ifndef MENU_LIST_H
#define MENU_LIST_H

/* Menu entry threads: starting, polling and stopping them, the thread bodies of simple menu windows, rows
 * drawn as text columns and the packet pages of scrolling text lists. */

#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/menu_window.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* entry threads */
/* Each scene keeps its own current table; event screens save and restore it. */
extern menu_entry_t* g_menu_thread_menu_data;
/* Whether the thread a screen manages is still running, as menu_start_or_poll_thread last saw it. */
extern s32 g_menu_managed_thread_state;
extern s32 g_menu_building_thread_started;

/* Starts thread_id on the entry's thread and returns 1 on the first call; later calls return whether the
 * thread still runs. */
s32 menu_start_managed_thread(s32 thread_id, menu_entry_t* descriptor);
void menu_request_thread_stop(s32 thread_id);
void menu_stop_managed_thread(s32 thread_id);
/* Requests a stop and runs input-update frames until the thread has ended. */
void menu_stop_thread_and_wait(s32 thread_id);
void menu_start_or_poll_thread(s32 thread_id, menu_entry_t* descriptor);
s32 menu_thread_is_running(s32 thread_id);
/* Empty; menu_init_subsystems calls it between the palette and text setup and the subsystem reset. */
void menu_init_stub(void);
/* Acts on Circle for a row: starts the row's entry one thread slot below, clears the selection, or (for a
 * negative action -n) closes n menu threads; stores the row as the entry's result. */
void menu_handle_action(menu_entry_t* entry, s32 row_index);
void menu_handle_cancel_input(menu_entry_t* context);
void menu_step_wrapping_cursor_on_scroll_buttons(menu_entry_t* bounds, s32* cursor);

/* entry window threads */
/* Thread bodies a menu entry names as its thread_entry. */

/* An entry with nothing to draw: handles Circle on row 0 and cancel with the menu sounds muted. */
void menu_input_thread(void);
/* A refused command: queues the invalid-choice sound and exits. */
void menu_run_invalid_choice_thread(void);
/* A simple menu window: its text in the frame, a wrapping cursor and an animated cursor icon. */
void menu_icon_list_thread(void);
/* A window without a cursor whose text image is rebuilt every seventh frame. */
void menu_icon_strip_thread(void);
/* One text entry shown until input arrives or g_message_message_duration_frames runs out. */
void menu_icon_timed_thread(void);
/* An icon window that also opens the entry named by row_actions[0] below itself. */
void menu_icon_linked_entry_thread(void);
/* An entry that shows a message through its menu_text_binding_t, then waits for the text thread. */
void menu_text_entry_wait_thread(void);
/* A wrapping icon menu; nonzero thread parameter 2 makes it return instead of exiting. */
void menu_run_icon_selection_loop(void);

/* text lists */
/* A text layout followed by the first visible row, as bound to a scrolling
 * list's menu_entry_t::context (ability_list_scrolling_thread and the AT list
 * built by at_list_build_ability_preview). */
typedef struct menu_scroll_text_layout {
    menu_text_layout_t columns; /* 0x00 */
    s16 row_offset;             /* 0x28 */
} menu_scroll_text_layout_t;

/* Task words of a scrolling text list thread: its *_redraw_text_page_on_scroll
 * helper redraws the page once and clears the request, which
 * screen_thread_request_redraw sets. */
enum {
    MENU_LIST_THREAD_WORD_REDRAW_REQUEST = 4,
};

void menu_draw_text_columns(menu_entry_t* entry, s32* row_offset, u8* buffer);
/* Applies scroll input, rebuilds the entry's text image and uploads the rows that changed. */
u8* menu_redraw_text_page_on_scroll(menu_entry_t* menu, s32* first_row, s32* render_pending, s32* unused_cursor);

/* list pages */
/* One of the two 0x134-byte packet pages a scrolling text list alternates between frames
 * (g_formation_ability_list_pages, g_at_list_pages, g_screen_text_list_pages). Provisional. */
typedef struct menu_list_page {
    SPRT frame;                         /* 0x00: text image / cursor frame */
    SPRT arrows[2];                     /* 0x14: scroll arrows */
    SPRT arrow_marks[2];                /* 0x3c */
    SPRT thumb;                         /* 0x64: scroll-bar thumb */
    DR_MODE text_mode;                  /* 0x78 */
    DR_MODE icon_mode;                  /* 0x84 */
    menu_icon_record_t icons;           /* 0x90 */
    scaled_draw_area_pair_t draw_areas; /* 0x10c */
} menu_list_page_t;
typedef char menu_list_page_size_must_be_308[sizeof(menu_list_page_t) == 308 ? 1 : -1];

/* Text image placement: the upload rectangle plus the screen offsets
 * gfx_init_image_loading reads. */
typedef struct menu_text_window {
    RECT rect;
    s16 x_screen_offset;
    s16 y_screen_offset;
} menu_text_window_t;

#endif
