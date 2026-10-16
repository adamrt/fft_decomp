#ifndef MENU_H
#define MENU_H

/* The menu library compiled into BATTLE, WORLD and the EVENT overlays. This base header holds frame timing, the
 * menu arena, menu entries and the icon and number records that the topic headers (menu_*.h) share. */

#include "fft/gfx.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* frame timing */
/* Raw VSync mode; animation and script steps use 1 when this is zero. */
extern s8 g_menu_event_speed;
/* Set while a menu thread forces event speed 2; the previous speed is saved. */
extern u16 g_menu_vsync_interval_forced;
extern u16 g_menu_saved_vsync_interval;

s32 menu_get_event_speed(void);
void menu_set_vsync_mode_and_event_speed(s32 speed);
s32 menu_update_menus_and_event_speed(u32* otag, s32 controller_input);

/* menu arena */
/* A first-fit allocator over 256-byte pages with sixteen allocation slots. Each program has its own arena of
 * VARIANT_MENU_BUFFER_PAGE_COUNT pages (53 in BATTLE, 112 in WORLD); menu_alloc_memory yields until a run of
 * free pages and a free slot exist. */
enum {
    MENU_BUFFER_ALLOCATION_COUNT = 16,
    MENU_BUFFER_PAGE_SIZE = 256,
};

/* One allocation: its first page, or -1 when the slot is free, and the length of the run in pages. */
typedef struct menu_allocation {
    u8* buffer;     /* 0x00 */
    s32 page_count; /* 0x04 */
} menu_allocation_t;
typedef char menu_allocation_size_must_be_8[(sizeof(menu_allocation_t) == 8) ? 1 : -1];

/* Nonzero for each arena page in use. */
extern u8 g_menu_buffer_allocation_map[];
extern menu_allocation_t g_menu_allocator_slot_records[MENU_BUFFER_ALLOCATION_COUNT];
extern u8 g_menu_buffer_arena[][MENU_BUFFER_PAGE_SIZE];

void* menu_alloc_memory(s32 size);
void menu_free_memory(void* buffer);
void menu_init_buffer_allocator(void);
void* menu_alloc_buffer(s32 bytes);

/* menu entries */
/* A scene's menus are a table of menu entries, one per window: its size and text image, its rows and their
 * actions, and the thread that runs it. The thread receives the entry as its first parameter and reads its
 * own record type through context. */

/* Record reached through menu_entry_t::context by menu_text_entry_wait_thread and WORLD's
 * world_menu_text_entry_thread: they call prepare, then start the text thread text_thread_id with speaker_id
 * as its third and fourth parameters. Provisional: only these members are known. */
typedef struct menu_text_binding {
    void (*prepare)(void); /* 0x00 */
    u16 speaker_id;        /* 0x04: unit whose portrait speaks (roster slot) */
    u16 text_thread_id;    /* 0x06 */
    u8 _unused_08[8];
    s16* row_text_ids; /* 0x10: text ids indexed by entry selected_index (world_menu_help_text_thread) */
} menu_text_binding_t;

/* Row actions 0..0xff open that menu entry; this flag suppresses the confirm sound. */
enum { MENU_ROW_ACTION_SILENT = 0x1000 };

/* Special row actions. CLEAR_SELECTION records no selection without opening another entry; NONE,
 * like any other value from 0x100 up, opens nothing, so Circle only records the row. */
enum {
    MENU_ROW_ACTION_CLEAR_SELECTION = 0x4d2,
    MENU_ROW_ACTION_NONE = 0x7d0,
};

/* menu_text_layout_t.mode[]: what a column of the layout draws. */
typedef enum menu_text_column_mode {
    MENU_TEXT_COLUMN_TEXT = 0,          /* text ids, drawn by menu_display_text_entry */
    MENU_TEXT_COLUMN_NUMBER = 1,        /* two-digit numbers, drawn by menu_display_number_entry */
    MENU_TEXT_COLUMN_HIDDEN = 2,        /* the column is skipped */
    MENU_TEXT_COLUMN_NUMBER_GLYPHS = 3, /* two-digit numbers, drawn by menu_draw_large_number_glyphs */
} menu_text_column_mode_e;

/* Three-column text layout walked by menu_draw_text_columns and WORLD's world_screen_menu_draw_text_columns,
 * reached through menu_entry_t::context when the entry draws a list. Column i draws mode[i] at x[i] from
 * text_ids[i] in text_colors[i], both indexed by row plus the row offset. */
typedef struct menu_text_layout {
    s16 row_count;       /* 0x00 */
    s16 hidden_rows;     /* 0x02: rows beyond the visible page (count - visible, 0 when
                            all fit); row_offset is clamped to it */
    s16 x[3];            /* 0x04 */
    s16 mode[3];         /* 0x0a: menu_text_column_mode_e */
    s16* text_ids[3];    /* 0x10 */
    s16* text_colors[3]; /* 0x1c */
} menu_text_layout_t;
typedef char menu_text_layout_size_must_be_40[sizeof(menu_text_layout_t) == 40 ? 1 : -1];

/* One menu window, shared by BATTLE, WORLD and the overlays; g_menu_thread_menu_data points at the scene's
 * current table. The icon threads read vram_origin and the inner size as the text image's upload rectangle and
 * window.x/y as the icon origin (the icon list thread clears bit 0 of window.y);
 * menu_step_wrapping_cursor_on_scroll_buttons wraps the cursor at max_row_index. */
typedef struct menu_entry {
    point16_t vram_origin; /* 0x00: text-image VRAM origin passed to the draw-mode helpers */
    s16 inner_width;       /* 0x04: text-image width; RECT.w = inner_width >> 2 (4bpp) */
    s16 inner_height;      /* 0x06: text-image height (menu_size_entry_to_text) */
    RECT window;           /* 0x08: screen position and extent passed to image and draw-area helpers */
    /* 0x10: text sprite placement (menu_build_window_sprites). width/height are set with the
     * inner and window sizes; the load origin and screen offsets are cleared
     * with them. */
    image_load_parameters_t text_image;
    s16 text_id;                 /* 0x1c: passed to text_find_entry */
    s16 max_row_index;           /* 0x1e: row count - 1 (menu_build_skillset_entries) */
    s16 thread_count;            /* 0x20: threads menu_handle_cancel_input stops, -1 when there is no thread
                                    group; set from the system-function table by menu_run_system_function_thread */
    u8 _padding_22[0x24 - 0x22]; /* aligns row_actions */
    s16* row_actions;            /* 0x24: each row's action, indexed by selected_index */
    void (*thread_entry)(void);  /* 0x28 */
    s16 header_id;               /* 0x2c: menu header; text-thread parameter (world_menu_text_entry_thread); the
                                    icon threads read it signed as the icon kind: 0x10 arrow, 0..4 single
                                    icon, 5..15 four-piece icon (menu_build_window_sprites) */
    u8 _padding_2e[2];           /* aligns context */
    /* 0x30: context interpreted by the menu thread. Text threads use menu_text_binding_t
     * here, the list threads a menu_text_layout_t, the number threads a
     * world_menu_number_range_t and the icon threads the s16 redraw flag they
     * poll (set to 1 to rebuild the text image). */
    void* context;
    void* value;           /* 0x34: thread-specific value data: s16 result cell or main-window s32 substitution pair */
    s16 selected_index;    /* 0x38: selected row; indexes the binding's 16-bit table
                              (world_menu_help_text_thread) */
    s16 select_text_table; /* 0x3a: "Index for select text": row of g_world_menu_entry_text_id_tables
                              (0x11 = the binding's row_text_ids); 2 / -1 defer to the next running
                              thread's entry. The AT-list and option-list builders park their
                              scroll row offset here instead. */
} menu_entry_t;
typedef char menu_entry_size_must_be_60[(sizeof(menu_entry_t) == 60) ? 1 : -1];

/* Numeric-entry and action-selection threads use value as a 16-bit result cell. */
static inline s16* menu_entry_result(menu_entry_t* entry) {
    return entry->value;
}

/* icons and numbers */
/* Texture rectangle, CLUT and texture page of an icon, as the menu row callbacks return it. */
typedef struct menu_icon_source {
    s16 u;     /* 0x00 */
    s16 v;     /* 0x02 */
    s16 w;     /* 0x04 */
    s16 h;     /* 0x06 */
    u16 clut;  /* 0x08 */
    u16 tpage; /* 0x0a */
} menu_icon_source_t;
typedef char menu_icon_source_size_must_be_12[sizeof(menu_icon_source_t) == 12 ? 1 : -1];

/* One positioned number: a text position, a pointer to the live value and the decimal format.
 * menu_draw_numeric_display_entries draws tables of them (kind 0 and 1 select the plain and colored number
 * renderers), as do the status panels' menu_text_render_decimal_entry_list and
 * menu_text_render_signed_decimal_entries, which leave kind unset. */
typedef struct menu_number_entry {
    u16 x;      /* 0x00 */
    u16 y;      /* 0x02 */
    s16* value; /* 0x04 */
    s16 format; /* 0x08; low byte digit count, high byte sign/marker flags
                   (0x400/0x800/0x1000; 0xc00/0xe00 draw dashes) */
    s16 kind;   /* 0x0a */
} menu_number_entry_t;
typedef char menu_number_entry_size_must_be_12[(sizeof(menu_number_entry_t) == 12) ? 1 : -1];

/* scene state */
/* Set by WORLD's formation screen each frame and cleared by the menu input update; WORLD's dialog thread
 * treats Help boxes differently while it is set. */
extern s32 g_formation_screen_active;
/* The field y (0 or 0xf0) handed to menu_input_update_event_state each frame; the panels draw into the other
 * frame-buffer half. */
extern u16 g_frame_arg;
/* Advanced once per frame by the menu frame update. */
extern s32 g_frame_counter;
extern u8 g_monster_formation_sprite_table[20]; /* indexed by sprite code - 0x3f */
extern s32 g_menu_formation_display_mode;
extern s32 g_menu_post_battle_unit_changes_active;

/* unreferenced stubs */
/* Empty functions that nothing on the disc calls or points at; the names are neutral placeholders. */
void menu_noop_1(void);
void menu_noop_2(void);
void menu_noop_3(void);
void menu_noop_4(void);
void menu_noop_5(void);
void menu_noop_6(void);
void menu_noop_7(void);

#endif
