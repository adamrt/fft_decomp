#ifndef MENU_LIST_H
#define MENU_LIST_H

/* Menu entry threads: starting, polling and stopping them, the thread bodies of simple menu windows, rows drawn as text
 * columns and the packet pages of scrolling text lists. */

#include "fft/gfx/gfx.h"
#include "fft/menu/menu.h"
#include "fft/menu/menu_text.h"
#include "fft/menu/menu_window.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* entry threads */
/* Each scene keeps its own current table; event screens save and restore it. */
extern menu_entry_t* g_menu_thread_menu_data;

/* Starts thread_id on the entry's thread and returns 1 on the first call; later calls return whether the
 * thread still runs. */
s32 menu_managed_thread_start_or_poll(s32 thread_id, menu_entry_t* descriptor);
void menu_thread_request_stop(s32 thread_id);
void menu_managed_thread_stop(s32 thread_id);

/* Requests a stop and runs input-update frames until the thread has ended. */
void menu_thread_stop_and_wait(s32 thread_id);
void menu_managed_thread_replace_or_poll(s32 thread_id, menu_entry_t* descriptor);

/* The menu callers' running-flag accessor. WORLD links this separately from thread_is_running. */
s32 menu_thread_is_running(s32 thread_id);

/* Acts on Circle for a row: starts the row's entry one thread slot below, clears the selection, or (for a
 * negative action -n) closes n menu threads; stores the row as the entry's result. */
void menu_handle_confirm_input(menu_entry_t* entry, s32 row_index);
void menu_handle_cancel_input(menu_entry_t* context);
void menu_update_row_cursor(menu_entry_t* bounds, s32* cursor);

/* entry window threads */
/* Thread bodies a menu entry names as its thread_entry. */

/* An entry with nothing to draw: handles Circle on row 0 and cancel with the menu sounds muted. */
void menu_input_thread(void);

/* A simple menu window: its text in the frame, a wrapping cursor and an animated cursor icon. */
void menu_icon_list_thread(void);

/* A window without a cursor whose text image is rebuilt every seventh frame. */
void menu_icon_message_thread(void);

/* One text entry shown until input arrives or g_message_duration_frames runs out. */
void menu_icon_timed_thread(void);

/* An icon window that also opens the entry named by row_actions[0] below itself. */
void menu_icon_linked_entry_thread(void);

/* A wrapping icon menu; nonzero thread parameter 2 makes it return instead of exiting. */
void menu_icon_selection_thread(void);

/* text lists */
/* A text layout followed by the first visible row, as bound to a scrolling
 * list's menu_entry_t::context (ability_list_scrolling_thread and the AT list
 * built by build_ability_preview). */
typedef struct menu_scroll_text_layout {
    menu_text_layout_t columns; /* 0x00 */
    s16 row_offset;             /* 0x28 */
} menu_scroll_text_layout_t;
typedef char menu_scroll_text_layout_size_must_be_44[sizeof(menu_scroll_text_layout_t) == 44 ? 1 : -1];

/* Task words of a scrolling text list thread: its *_redraw_text_page_on_scroll
 * helper redraws the page once and clears the request, which
 * menu_screen_thread_request_redraw sets. */
enum { MENU_LIST_THREAD_WORD_REDRAW_REQUEST = 4 };

void menu_draw_text_columns(menu_entry_t* entry, s32* row_offset, u8* buffer);

/* list pages */
/* One of the two 0x134-byte packet pages a scrolling text list alternates between frames
 * (g_formation_ability_list_pages, g_at_list_pages and text_list.c's s_text_list_pages). Provisional. */
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

#endif
