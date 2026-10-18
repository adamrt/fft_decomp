#ifndef SCREEN_H
#define SCREEN_H

/* WORLD's menu-screen framework, used by every WORLD screen and by WLDCORE and OPEN: screen threads, the per-frame menu
 * step and its input, packet buffers, fades, palettes and window frames, the main menu entries, the display script's
 * lists, cursors, grid menus, number entries, text sections and sound requests. */

#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/menu_script.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* screen threads */
/* Slots that the WORLD screens use for threads of their own; thread.h has the maps shared with BATTLE, world_map.h
 * and title.h the WLDCORE and OPEN ones. */
typedef enum screen_thread_slot {
    SCREEN_THREAD_SLOT_MENU_GROUP = 2,           /* world_map_menu_group_thread */
    SCREEN_THREAD_SLOT_EQUIP_ABILITY_PANEL = 10, /* the equipment (change banner) or the ability panel */
    SCREEN_THREAD_SLOT_PREVIEW_STATS = 11,       /* the numeric panel thread of the preview stats window */
    SCREEN_THREAD_SLOT_UPPER_MENU_CHILD = 14,    /* the child of the menu in slot 15, or a wait-forever thread
                                                  * standing in for it */
    SCREEN_THREAD_SLOT_UPPER_MENU = 15,          /* a second menu beside the one in slot 6 */
    SCREEN_THREAD_SLOT_TUTORIAL_TEXT = 16        /* tutorial script text: the slot past the sixteen reset ones */
} screen_thread_slot_e;

/* at_list_run runs its window frame thread in the upper menu's slot. */
enum { SCREEN_THREAD_SLOT_AT_LIST_FRAME = 15 };

void screen_thread_yield_forever(void);
void screen_thread_request_redraw(s32 thread_id);
void screen_thread_request_stop(s32 thread_id);

/* menu frame */
/* Ordinary outfitter screens use a town's location_id_e. These values
 * identify the other screens that reuse the formation menu framework. */
typedef enum screen_context {
    SCREEN_CONTEXT_NAME_ENTRY = -2,
    SCREEN_CONTEXT_FORMATION = -1,
    SCREEN_CONTEXT_FUR_SHOP = 100,
    SCREEN_CONTEXT_SOLDIER_OFFICE = 101
} screen_context_e;

extern s16 g_screen_context_id; /* town location ID or screen_context_e */
extern s8 g_screen_music_slot;  /* formation music slot */
extern menu_input_state_t g_screen_input_state;

/* Alias for the first 32-bit field retained for the target's separate absolute store. */
extern s32 g_screen_new_buttons;

s16 screen_menu_frame(u32* otag, u32 input);
void screen_reset_context(void);
void screen_set_allowed_buttons(u32 mask);
s32 screen_is_busy(void);
void screen_reset_game(void);

/* controller input */
extern u16 g_screen_input_latched_button;
extern u32 g_screen_input_primary_repeat;
extern s32 g_screen_input_primary_repeat_latched;
extern s32 g_screen_input_secondary_repeat_latched;

s32 screen_input_filter(void);
s32 screen_input_get_lockout(void);
void screen_input_update_lockout(void);

/* packet buffers */
extern menu_gfx_context_t* g_screen_packet_buffers;
extern u16 g_screen_ot_length;
extern u16 g_screen_draw_buffer_clip_y;
extern u16 g_screen_textured_quad_capacity;          /* textured quad count */
extern u16 g_screen_gradient_quad_capacity;          /* gradient quad count */
extern u16 g_screen_textured_gradient_quad_capacity; /* textured gradient quad count */
extern u16 g_screen_flat_line_capacity;              /* +0x24 line count */
extern u16 g_screen_gradient_line_capacity;          /* gradient line count */
extern u16 g_screen_gradient_line_count;
extern u16 g_screen_line_g3_capacity;
extern u16 g_screen_poly_f4_count;
extern u16 g_screen_draw_area_count;

/* Volatile: screen_init_packet_buffers must read the draw-area count
 * after the draw-move count, as the target does. */
extern volatile u16 g_screen_draw_area_capacity;

/* Draw move count; volatile for the same read order as g_screen_draw_area_capacity. */
extern volatile u16 g_screen_draw_move_capacity;

void screen_init_packet_buffers(menu_gfx_context_t* buffers, u32* otags, void* unused_2, void* unused_3, void* unused_4,
    POLY_FT4* textured_quads, void* unused_6, void* unused_7, POLY_G4* gradient_quads,
    POLY_GT4* textured_gradient_quads, LINE_F2* flat_lines, void* unused_11, void* unused_12, LINE_G2* gradient_lines,
    void* unused_14, void* unused_15, TILE* tiles, void* unused_17, void* unused_18, void* unused_19, void* unused_20,
    void* unused_21, void* unused_22, DR_MOVE* draw_moves, DR_AREA* draw_areas, DR_MODE* draw_modes);
void screen_present_frame(s32 first_otag, s32 draw_otag);
void screen_append_poly_f4(point16_t* xy, rgb24_t* rgb, s32 semi_trans, s32 ot_index);
void screen_add_gradient_line(point16_t* endpoints, rgb24_t* colors, s32 semi_trans, s32 ot_index);
void screen_add_draw_area(RECT* rect, s32 otag_index);

/* draw areas and blits */
extern RECT g_screen_temporary_draw_area;

/* A full-screen texture window (x=0, y=0, w=0x100, h=0x100), copied verbatim into a menu record's DR_MODE by
 * screen_init_scaled_draw_area_packets; the formation view reads the same bytes as a zero screen point.
 * Same 8-byte RECT shape as g_gfx_texture_window, whose reset sibling is byte-for-byte identical. */
extern const RECT g_screen_full_texture_window;

void screen_set_blit_dest_position(s32 x, s32 y);
void screen_set_blit_source_rect(s16 x, s16 y, s16 width, s16 height);

/* fades */
/* Separate decreasing/increasing RGB-intensity flags.
 * The getter packs them into bits 0/1 and returns 0 when disabled. */
typedef enum screen_fade_state {
    SCREEN_FADE_STATE_NONE = 0,
    SCREEN_FADE_STATE_DECREASING = 1,
    SCREEN_FADE_STATE_INCREASING = 2
} screen_fade_state_e;

extern rgb24_t g_screen_black_tile_color;

void screen_fade_set_disabled(u8 disabled);
s32 screen_fade_get_state(void);
void screen_fade_start_decreasing(void);
void screen_fade_start_increasing(void);
s32 screen_fade_update_in(void);
s32 screen_fade_update_out(void);

/* graphics resources */
/* Texture-page 7 uses a grid of 16-pixel square allocation cells. */
enum {
    SCREEN_TPAGE7_GRID_ROWS = 15,
    SCREEN_TPAGE7_GRID_COLUMNS = 16,
    SCREEN_TPAGE7_CELL_PIXELS = 16,
    SCREEN_TPAGE7_CELL_PIXEL_SHIFT = 4,
    SCREEN_TPAGE7_CELL_PIXEL_MASK = 0x0f,
    SCREEN_TPAGE7_GRID_OCCUPIED = 0xff
};

/* palettes and colors */
extern u16 g_screen_window_tpage;
extern u16 g_screen_window_clut;
extern u16 g_screen_palette_clut_normal;
extern u16 g_screen_alternate_number_clut;
extern u16 g_screen_alternate_number_clut_normal;
extern u16 g_screen_disabled_number_clut;
extern u8 g_screen_use_alternate_palette;
extern s32 g_screen_color_red;
extern s32 g_screen_color_green;
extern s32 g_screen_color_blue;
extern rgb24_t g_screen_sprite_color;

void screen_set_palette_mode(s32 mode);
void screen_set_sprite_color(const rgb24_t* src);
rgb24_t* screen_get_draw_color(void);
void screen_set_brightness(s32 red, s32 green, s32 blue);

/* window frames */
extern RECT g_screen_thin_panel_texture_windows[7];

s32 screen_build_window_frame_image(s32 width, s32 height, u16* destination);
void screen_build_window_image_row(const u16* source, u16* dest, s32 count, s32 offset);
void screen_window_frame_thread(void);
void screen_panel_init_scaled_draw_area_packets(scaled_draw_area_pair_t* pair);

/* main menu */
extern s32 g_screen_thread_result;
extern s32 g_screen_dialog_waiting;

void screen_open_entry_window(s32 index, s32 unused_x, s32 unused_y);
void screen_set_default_entry_0_text(u32 text_index, u32 value);
void screen_set_default_entry_1_text(s32 value);
void screen_set_entry_4_cursor(s32 index);
s32 screen_is_dialog_waiting(void);
void screen_text_list_thread(void);

/* Set up the WORLD menus with EVENT/WORLD.LZW from the main thread. */
void screen_init_with_world_text(void);

/* Set up the WORLD menus with EVENT/WORLD.LZW and the WLDFACE portraits. */
void screen_init_with_world_text_and_faces(void);

/* Set up the menus with EVENT/OPEN.LZW for the new-game prologue. */
void screen_init_with_open_text(void);

/* display script commands */
typedef char LINE_F2_size_must_be_16[(sizeof(LINE_F2) == 16) ? 1 : -1];

enum { SCREEN_SCRIPT_END = 0x1c };

/* scrollable lists */
extern const s16* g_screen_list_entry_ids;
extern u8* g_screen_list_text_table;
extern s8 g_screen_list_layout_pending;
extern s16 g_screen_use_scroll_position;
extern s16 g_screen_text_redraw_request;

void screen_init_list(const s16* values, s32 selected_index, u8* text_table, menu_instruction_t* script);
void screen_load_list_layout(const menu_instruction_t* script);
void screen_init_and_draw_list(
    const s16* entries, s32 selected_index, s32 scroll_base_index, u8* text_table, const menu_instruction_t* script);
void screen_scroll_list_page(menu_vertical_scroll_direction_e direction, const menu_instruction_t* list);
void screen_draw_item_tabs(s32 wide, s32 row, s32 mode);
void screen_draw_text_columns(menu_entry_t* entry, s32* row_offset, u8* buffer);

/* cursors, selections and input */
/* Result slots the shop menus share. */
enum {
    SCREEN_SELECTION_SHOP_QUANTITY = 3, /* chosen purchase/sale quantity */
    SCREEN_SELECTION_PROMPT_RESULT = 4  /* yes/no prompt result (0 confirms) */
};

/* Index of a saved cursor in g_menu_selection_values, as the formation
 * and shop menus use them. */
typedef enum screen_cursor {
    SCREEN_CURSOR_SHOP_ITEM_TAB = 0,
    SCREEN_CURSOR_EQUIP_SLOT = 2,
    SCREEN_CURSOR_REMOVE_SLOT = 3, /* equipment slot of the remove and sell menus */
    SCREEN_CURSOR_SET_ABILITY_SLOT = 4,
    SCREEN_CURSOR_REMOVE_ABILITY_SLOT = 5,
    SCREEN_CURSOR_ITEM_TAB = 7,
    SCREEN_CURSOR_ABILITY_TAB = 8
} screen_cursor_e;

extern u8 g_screen_prompt_active;

/* Result slot per menu (-1 while none); screen_reset_selection_results clears all 16. */
extern s16 g_screen_selection_results[16];

void screen_reset_selection_results(void);
s16 screen_init_cursor_position(u8 index, s16 value);
s32 screen_input_read(s32 unused_0);
void screen_input_update_dialog_state(void);
void screen_draw_sliding_cursor(point16_t* pos, u16* state, s32 mode);

/* grid menus */
extern s32 g_screen_grid_id; /* help_grid_id_e */

void screen_update_grid_cursor(s16 alternate);
void screen_start_help_text(s32 position);

/* number entries */
/* Value range of a numeric menu entry (screen_number_entry_thread and
 * screen_labeled_number_entry_thread). */
typedef struct screen_number_range {
    ranges16_t bounds;       /* 0x00 */
    s16 _unknown_04[2];      /* 0x04: -1 from window_start_number_panel_thread */
    void (*on_change)(void); /* 0x08 */
} screen_number_range_t;

void screen_number_entry_thread(void);
void screen_labeled_number_entry_thread(void);
void screen_help_text_thread(void);
void screen_fit_parent_entry_to_digits(void);

/* text sections */
/* Resident text resource indices, used by screen_text_get_resident and
 * screen_text_bind_section. */
typedef enum screen_text_data {
    SCREEN_TEXT_DATA_SKILLSET_NAMES = 0,
    SCREEN_TEXT_DATA_JOB_NAMES = 1,
    SCREEN_TEXT_DATA_ABILITY_NAMES = 2,
    SCREEN_TEXT_DATA_FORMATION_MESSAGES = 3,   /* TEXT_SECTION_FORMATION_MESSAGES */
    SCREEN_TEXT_DATA_DISMISS_UNIT_REPLIES = 4, /* SCREEN_TEXT_SECTION_FORMATION_TEXT */
    SCREEN_TEXT_DATA_JOB_DESCRIPTIONS = 5,     /* SCREEN_TEXT_SECTION_FORMATION_TEXT */
    SCREEN_TEXT_DATA_NAME_ENTRY_KEYBOARD = 7,
    SCREEN_TEXT_DATA_SHOP_DIALOGUE = 8 /* SCREEN_TEXT_SECTION_SHOP_DIALOGUE */
} screen_text_data_e;

/* WORLD text files materialize 32 section pointers after their 0x80-byte
 * offset/header area. The same table is saved and restored around overlays. */
enum { SCREEN_TEXT_SECTION_TABLE_BYTES = 128, SCREEN_TEXT_FILE_HEADER_BYTES = 128 };

/* The text sections WORLD's screens bind while they are up (text_section_e
 * has the resident ones). */
typedef enum screen_text_section {
    SCREEN_TEXT_SECTION_TUTORIAL_MESSAGES = 23, /* the running tutorial's message file */
    SCREEN_TEXT_SECTION_NAME_ENTRY_KEYS = 28,   /* name entry: the key rows */
    SCREEN_TEXT_SECTION_FORMATION_TEXT = 29,    /* formation: what a dismissed unit says, or the job wheel's
                                                 * descriptions, whichever was bound last */
    SCREEN_TEXT_SECTION_SHOP_MENU_LABELS = 30,
    SCREEN_TEXT_SECTION_SHOP_DIALOGUE = 31 /* the shopkeeper's lines */
} screen_text_section_e;

extern u32 g_screen_text_section_offsets[];

u8* screen_text_get_resident(screen_text_data_e index);
u8* screen_text_bind_section(u32 index);
const u8* screen_text_advance_chars(const u8* text, s32 count);
s16 screen_text_get_char_at(const u8* table, s16 index, s32 position);

/* MAIN selects the WORLD address of text_find_entry through this existing alias. */
u8* world_text_find_entry(u32 text_id);

/* text images */
void screen_text_render_rows(u8* font, s16* text_ids, RECT* destination, s32 palette);
void screen_text_kern(u8* text, s16* name, RECT* rect, s32 palette);
s32 screen_text_measure_width(const u8* entry);

/* message box */
s32 screen_text_is_printing(s32 thread_id);
void screen_text_resume_printing(s32 thread_id);

/* music and sound effects */
extern u8 g_screen_sound_release_held_loop;

void screen_sound_dispatch_effect(void);

#endif
