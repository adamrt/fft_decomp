#ifndef FFT_EVENT_OPTION_H
#define FFT_EVENT_OPTION_H

/* EVENT/OPTION.OUT: menu screen overlay that runs with BATTLE. */

#include "fft/battle.h"

/* dead unit */
/* Provisional layouts for the dead-unit ("treasure/crystal") result panel
 * started by option_menu_start_dead_unit_threads. Only the fields that routine
 * touches are named; the surrounding bytes stay anonymous. dead_unit_menu_t
 * opens with the 0x3c menu record and uses the world_menu_entry_t names (WORLD
 * twin world_dead_unit_menu_t). */
typedef struct dead_unit_entry_counts {
    s16 visible_count;  /* 0x00 */
    s16 overflow_count; /* 0x02 */
} dead_unit_entry_counts_t;

typedef struct dead_unit_menu {
    u8 _unused_000[4];
    s16 inner_width; /* 0x04 */
    u8 _unused_006[2];
    s16 window_x;     /* 0x08 */
    s16 window_y;     /* 0x0a */
    s16 window_width; /* 0x0c */
    u8 _unused_00e[6];
    s16 overall_width; /* 0x14 */
    u8 _unused_016[0x18];
    dead_unit_entry_counts_t* entries; /* 0x30 */
    u8 _unused_034[6];
    s16 menu_type;         /* 0x3a */
    u8 at_list_menu[0x78]; /* 0x3c */
    u8 confirm_menu[0x78]; /* 0xb4 */
    u8 message_menu[0x1c]; /* 0x12c */
    s16 message_id;        /* 0x148 */
    u8 _unused_14a[0x1e];
    u8 move_menu[4]; /* 0x168 */
} dead_unit_menu_t;
typedef char dead_unit_menu_size_must_be_0x16c[sizeof(dead_unit_menu_t) == 0x16c ? 1 : -1];

extern u32* g_dead_unit_controller_input;
extern s32 g_dead_unit_item_value;
extern dead_unit_menu_t g_dead_unit_menu;
extern dead_unit_entry_counts_t g_dead_unit_menu_entries;
extern s16 g_dead_unit_menu_flags[];
extern s16 g_dead_unit_menu_values[];
extern s16 g_dead_unit_primary_result;
extern s32 g_dead_unit_roster_id;
extern s32 g_dead_unit_roster_id_copy;
extern s16 g_dead_unit_secondary_result;

/* gfx */
extern const u16 g_option_gfx_draw_area_scale_percent[12];
extern RECT g_option_gfx_draw_area_template;

void option_gfx_build_scaled_draw_area_packets(battle_menu_status_panel_portrait_primitive_tail_t* packet,
    const void* source, s32 scale_index, s32 lower_half, const s16* offset);

void option_gfx_init_scaled_draw_area_packets(battle_menu_status_panel_portrait_primitive_tail_t* packet);

/* menu */
/* Record types hoisted out of the defining .c files so that callers and
 * definitions agree on one spelling. */
/* Scroll geometry block shared by option_menu_init_at_list and
 * option_menu_update_scroll. */
typedef struct option_scroll_layout {
    s16 step;
    s16 maximum;
} option_scroll_layout_t;

/* OPTION's view of the 0x3c-byte menu record (g_battle_menu_thread_menu_data);
 * field names follow world_menu_entry_t. */
typedef struct option_at_menu {
    s16 vram_x;       /* 0x00: text-image VRAM x */
    s16 vram_y;       /* 0x02: text-image VRAM y */
    s16 inner_width;  /* 0x04 */
    s16 inner_height; /* 0x06 */
    s16 window_x;     /* 0x08 */
    s16 window_y;     /* 0x0a */
    s16 window_width; /* 0x0c */
    u8 _unused_0e[6];
    s16 overall_width; /* 0x14 */
    u8 _unused_16[0x1a];
    option_scroll_layout_t* layout; /* 0x30 */
} option_at_menu_t;

/* Provisional: three-column menu text layout, the same record
 * world_menu_draw_text_columns walks (world_menu_text_layout_t). Column i
 * uses x[i], mode[i] (0 text, 1 number, 2 hidden, 3 alternate number),
 * text_ids[i] and text_colors[i], each indexed by row + row offset. */
typedef struct option_menu_text_layout {
    s16 row_count;       /* 0x00 */
    s16 hidden_rows;     /* 0x02: rows beyond the visible page; row_offset clamp */
    s16 x[3];            /* 0x04 */
    s16 mode[3];         /* 0x0a */
    s16* text_ids[3];    /* 0x10 */
    s16* text_colors[3]; /* 0x1c */
} option_menu_text_layout_t;

/* OPTION's text-menu view of the 0x3c-byte menu record. */
typedef struct option_menu_entry {
    u16 vram_x;       /* 0x00 */
    u16 vram_y;       /* 0x02 */
    s16 inner_width;  /* 0x04: rect width * 4 */
    u16 inner_height; /* 0x06 */
    u8 _unused_08[0x30 - 0x08];
    option_menu_text_layout_t* text_binding; /* 0x30 */
    u8 _unused_34[6];
    s16 select_text_table; /* 0x3a */
} option_menu_entry_t;

/* Provisional layout of the OPTION dead-unit panel primitive block at
 * 0x801cc074, which holds two identical frame records. The 0x158 record size
 * is fixed by the second frame starting there and by the byte copy between
 * them. Members come from the panels' own accesses plus the two BATTLE
 * helpers they call: battle_menu_init_sprite_array (0x8012e2b8) is handed the
 * nine SPRT packets, and battle_menu_set_disabled_texture_window (0x80138adc)
 * writes +0x1c and the 8-byte window at +0x20 of the cursor record, which
 * lands on the cursor sprite's w and the RECT that follows it. The block is
 * zero-filled in the overlay image, so none of it is initialized data. */
typedef struct option_panel_frame {
    DR_MODE modes[7];           /* 0x000: one per texture window */
    SPRT sprites[9];            /* 0x054: 5 border spans then 4 corners */
    u8 texture_prim[0x28];      /* 0x108: tpage-7 VRAM allocation record */
    DR_MODE cursor_mode;        /* 0x130 */
    SPRT cursor;                /* 0x13c */
    RECT cursor_texture_window; /* 0x150 */
} option_panel_frame_t;
typedef char option_panel_frame_size_must_be_0x158[sizeof(option_panel_frame_t) == 0x158 ? 1 : -1];

extern const u8 g_option_menu_at_list_entry_table[];
extern const void* g_option_menu_at_list_entry_table_pointer;
extern s16 g_option_menu_at_list_flags[];
extern s16 g_option_menu_at_list_indices[];
extern s16 g_option_menu_at_list_menu_type;
extern s16 g_option_menu_at_list_primary_values[];
extern s16 g_option_menu_at_list_secondary_values[];
extern option_at_menu_t g_option_menu_at_list_state[];
extern u8 g_option_menu_at_list_text[];
extern option_menu_text_layout_t g_option_menu_entry_table[1];
extern option_panel_frame_t g_option_menu_panel_frames[2];
extern u8 g_option_menu_panel_pixels[];

/* One libgpu texture window per dead-unit panel DR_MODE packet. The static
 * tables already hold each window's w/h (0x10x0x10, 8x0x10, 0x10x8 and two
 * full 0x100x0x100 windows); the panels fill in only the runtime x/y.
 * Indexing the array is what reproduces the target's symbol-plus-byte-offset
 * addressing. */
extern RECT g_option_menu_panel_texture_windows[7];
extern RECT g_option_menu_panel_texture_windows_b[7];
extern u8 g_option_menu_render_buffer[];
extern s32 g_option_menu_state;
extern u16 g_option_menu_transition_duration;
extern s16 g_option_menu_transition_timer;
extern RECT g_option_menu_entry_panel_rect;
extern struct world_menu_icon_thread_param g_option_menu_icon_thread_param;
extern RECT g_option_menu_title_image_rect;
extern RECT g_option_menu_value_image_rect;
void option_menu_init_at_list(option_at_menu_t* menu);
void option_menu_render_entries(option_menu_entry_t* entry, s32* row_offset, void* buffer);
void option_menu_start_at_list_thread(void);
void option_menu_start_dead_unit_threads(void);
void option_menu_run_dead_unit_panel_a(void);
void* option_menu_update_scroll(option_menu_entry_t* menu, s32* first_row, s32* render_pending);

/* text */
/* The two glyph blit descriptors at 0x801c9ce4 and 0x801c9cf4, filled by
 * option_text_set_blit_source_rect and option_text_set_blit_destination. Each
 * is 0x10 bytes: the second starts 0x10 past the first, and
 * g_option_menu_panel_pixels starts 0x10 past the second. The stride is the
 * surface width in pixels: 0x100 for the glyph sheet, 0x80 for the dead-unit
 * panels' scratch image. */
typedef struct option_glyph_blit {
    RECT rect;
    s32 stride;
    u8 _unused_0c[4];
} option_glyph_blit_t;
typedef char option_glyph_blit_size_must_be_0x10[sizeof(option_glyph_blit_t) == 0x10 ? 1 : -1];

extern option_glyph_blit_t g_option_text_glyph_dest;
extern option_glyph_blit_t g_option_text_glyph_source;
void option_text_set_blit_destination(s16 destination_x, s16 destination_y);
void option_text_set_blit_source_rect(s16 source_x, s16 source_y, s16 width, s16 height);

/* other */
extern s32 g_option_current_values[];
extern s32 g_option_reference_values[];
extern s32 g_option_values_unchanged;
extern s32 g_option_choice_counts[];
extern s32 g_option_choice_window_widths[];
extern battle_menu_window_record_t g_option_picker_icon_records[2];
extern s32 g_option_value_text_id_bases[];
extern world_input_timing_profile_t g_option_input_repeat_timings[];
void option_build_at_list(void);
void option_build_options_menu(void);
void option_thread_wait_forever(void);
void option_run_option_choice_menu(void);
void option_entrypoint(s32 menu_type);

#endif
