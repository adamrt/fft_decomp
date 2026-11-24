#ifndef MENU_SCREEN_H
#define MENU_SCREEN_H

/* WORLD's menu-screen framework, used by every WORLD screen and by WLDCORE and OPEN: screen threads and setup, the
 * per-frame menu step and its input, packet buffers, fades and brightness, palettes and window frames, the main menu
 * entries, the display script's lists, cursors, grid menus, entry threads, text sections and sound requests. */

#include "fft/gfx/gfx.h"
#include "fft/menu/menu.h"
#include "fft/menu/menu_input.h"
#include "fft/menu/menu_script.h"
#include "fft/text/text.h"
#include "psx/libgpu.h"
#include "psx/types.h"

struct menu_gfx_context;

/* screen threads */
/* Slots that the WORLD screens use for threads of their own; thread.h has the maps shared with BATTLE, world_map.h
 * the WLDCORE ones and music.h OPEN's music test slot. */
typedef enum menu_screen_thread_slot {
    MENU_SCREEN_THREAD_SLOT_MENU_GROUP = 2,           /* dispatch_window_group_thread */
    MENU_SCREEN_THREAD_SLOT_EQUIP_ABILITY_PANEL = 10, /* the equipment (change banner) or the ability panel */
    MENU_SCREEN_THREAD_SLOT_PREVIEW_STATS = 11,       /* the numeric panel thread of the preview stats window */
    MENU_SCREEN_THREAD_SLOT_UPPER_MENU_CHILD = 14,    /* the child of the menu in slot 15, or a wait-forever thread
                                                       * standing in for it */
    MENU_SCREEN_THREAD_SLOT_UPPER_MENU = 15,          /* a second menu beside the one in slot 6 */
    MENU_SCREEN_THREAD_SLOT_TUTORIAL_TEXT = 16        /* tutorial script text: the slot past the sixteen reset ones */
} menu_screen_thread_slot_e;

/* at_list_run runs its window frame thread in the upper menu's slot. */
enum { MENU_SCREEN_THREAD_SLOT_AT_LIST_FRAME = 15 };

void menu_screen_thread_request_redraw(s32 thread_id);
void menu_screen_thread_request_stop(s32 thread_id);

/* setup */
/* Set up the WORLD menus with EVENT/WORLD.LZW from the main thread. */
void menu_screen_init_world_text(void);

/* Set up the WORLD menus with EVENT/WORLD.LZW and the WLDFACE portraits. */
void menu_screen_init_world_resources(void);

/* Set up the menus with EVENT/OPEN.LZW for the new-game prologue. */
void menu_screen_init_open_text(void);

/* menu frame */
/* Ordinary shop screens use a town's location_id_e. These values
 * identify the other screens that reuse the formation menu framework. */
typedef enum menu_screen_context {
    MENU_SCREEN_CONTEXT_NAME_ENTRY = -2,
    MENU_SCREEN_CONTEXT_FORMATION = -1,
    MENU_SCREEN_CONTEXT_FUR_SHOP = 100,
    MENU_SCREEN_CONTEXT_SOLDIER_OFFICE = 101
} menu_screen_context_e;

extern s16 g_menu_screen_context_id; /* town location ID or menu_screen_context_e */
extern s8 g_menu_screen_music_slot;  /* formation music slot */
extern menu_input_state_t g_menu_screen_input_state;

/* Alias for the first 32-bit field retained for the target's separate absolute store. */
extern s32 g_menu_screen_new_buttons;

s16 menu_screen_update_menu_frame(u32* otag, u32 input);
void menu_screen_reset_context(void);
void menu_screen_set_allowed_buttons(u32 mask);
s32 menu_screen_check_busy(void);
void menu_screen_reset_game(void);

/* controller input */
extern s32 g_menu_screen_input_primary_repeat_latched;
extern s32 g_menu_screen_input_secondary_repeat_latched;

s32 menu_screen_filter_buttons(void);
s32 menu_screen_get_input_lockout(void);
void menu_screen_update_input_lockout(void);
s32 menu_screen_read_buttons(s32 unused_0);

/* packet buffers */
extern struct menu_gfx_context* g_menu_screen_packet_buffers;
extern u16 g_menu_screen_ot_length;
extern u16 g_menu_screen_draw_buffer_clip_y;
extern u16 g_menu_screen_textured_quad_capacity;          /* textured quad count */
extern u16 g_menu_screen_gradient_quad_capacity;          /* gradient quad count */
extern u16 g_menu_screen_textured_gradient_quad_capacity; /* textured gradient quad count */
extern u16 g_menu_screen_flat_line_capacity;              /* line count */
extern u16 g_menu_screen_gradient_line_capacity;          /* gradient line count */
extern u16 g_menu_screen_gradient_line_count;
extern u16 g_menu_screen_line_g3_capacity;
extern u16 g_menu_screen_poly_f4_count;
extern u16 g_menu_screen_draw_area_count;

/* Draw-area count, which menu_gfx_init_contexts reads after the draw-move count. Volatile: gcc 2.6.3 keeps two loads in
 * order only when both are volatile, and the target puts the second in the first's delay slot. */
extern volatile u16 g_menu_screen_draw_area_capacity;

/* Draw move count; volatile for the same read order as g_menu_screen_draw_area_capacity. */
extern volatile u16 g_menu_screen_draw_move_capacity;

void menu_screen_append_poly_f4(point16_t* xy, rgb24_t* rgb, s32 semi_trans, s32 ot_index);
void menu_screen_add_gradient_line(point16_t* endpoints, rgb24_t* colors, s32 semi_trans, s32 ot_index);

/* draw areas and blits */
extern RECT g_menu_screen_temporary_draw_area;

void menu_screen_set_blit_destination(s32 x, s32 y);
void menu_screen_set_blit_source_rect(s16 x, s16 y, s16 width, s16 height);

/* fades and brightness */
extern rgb24_t g_menu_screen_black_tile_color;
extern s32 g_menu_screen_brightness_red;
extern s32 g_menu_screen_brightness_green;
extern s32 g_menu_screen_brightness_blue;

void menu_screen_set_brightness(s32 red, s32 green, s32 blue);

/* palettes and colors */
extern u16 g_menu_screen_window_tpage;
extern u16 g_menu_screen_window_clut;
extern u16 g_menu_screen_palette_clut_normal;
extern u16 g_menu_screen_alternate_number_clut;
extern u16 g_menu_screen_alternate_number_clut_normal;
extern u16 g_menu_screen_disabled_number_clut;
extern u8 g_menu_screen_use_alternate_palette;
extern rgb24_t g_menu_screen_sprite_color;

/* window frames */
s32 menu_screen_build_window_frame_image(s32 width, s32 height, u16* destination);
void menu_screen_build_window_image_row(const u16* source, u16* dest, s32 count, s32 offset);
void menu_screen_window_frame_thread(void);
void menu_screen_panel_init_scaled_draw_area_packets(scaled_draw_area_pair_t* pair);

/* main menu */
extern s32 g_menu_screen_thread_result;
extern s32 g_menu_screen_dialog_waiting;

void menu_screen_open_entry_window(s32 index, s32 unused_x, s32 unused_y);
void menu_screen_set_world_menu_cursor(s32 index);
s32 menu_screen_is_dialog_waiting(void);

/* display script */
enum { MENU_SCREEN_SCRIPT_END = 0x1c };

/* scrollable lists */
extern const s16* g_menu_screen_list_entry_ids;
extern u8* g_menu_screen_list_text_table;
extern s8 g_menu_screen_list_layout_pending;
extern s16 g_menu_screen_use_scroll_position;
extern s16 g_menu_screen_text_redraw_request;

void menu_screen_draw_text_columns(menu_entry_t* entry, s32* row_offset, u8* buffer);

/* cursors and selections */
/* Result slots the shop menus share. */
enum {
    MENU_SCREEN_SELECTION_SHOP_QUANTITY = 3, /* chosen purchase/sale quantity */
    MENU_SCREEN_SELECTION_PROMPT_RESULT = 4  /* yes/no prompt result (0 confirms) */
};

/* Index of a saved cursor in g_menu_selection_values, as the formation
 * and shop menus use them. */
typedef enum menu_screen_cursor {
    MENU_SCREEN_CURSOR_SHOP_ITEM_TAB = 0,
    MENU_SCREEN_CURSOR_EQUIP_SLOT = 2,
    MENU_SCREEN_CURSOR_REMOVE_SLOT = 3, /* equipment slot of the remove and sell menus */
    MENU_SCREEN_CURSOR_SET_ABILITY_SLOT = 4,
    MENU_SCREEN_CURSOR_REMOVE_ABILITY_SLOT = 5,
    MENU_SCREEN_CURSOR_ITEM_TAB = 7,
    MENU_SCREEN_CURSOR_ABILITY_TAB = 8
} menu_screen_cursor_e;

extern u8 g_menu_screen_prompt_active;

/* Result slot per menu (-1 while none); menu_screen_reset_selection_results clears all 16. */
extern s16 g_menu_screen_selection_results[16];

void menu_screen_reset_selection_results(void);
void menu_screen_update_dialog_input(void);

/* grid menus */
extern s32 g_menu_screen_grid_id; /* help_grid_id_e */

void menu_screen_update_grid_cursor(s16 alternate);

/* entry threads */
/* Value range of a numeric menu entry (menu_screen_number_entry_thread and
 * menu_screen_labeled_number_entry_thread). The callback runs when the entry is
 * confirmed, including when the value is unchanged. */
typedef struct menu_screen_number_range {
    ranges16_t bounds;        /* 0x00 */
    s16 _unread_04[2];        /* 0x04: -1 from window_start_number_panel_thread */
    void (*on_confirm)(void); /* 0x08 */
} menu_screen_number_range_t;
typedef char menu_screen_number_range_size_must_be_12[sizeof(menu_screen_number_range_t) == 12 ? 1 : -1];

void menu_screen_number_entry_thread(void);
void menu_screen_labeled_number_entry_thread(void);
void menu_screen_help_text_thread(void);
void menu_screen_text_list_thread(void);

/* text sections */
/* Resident text resource indices, used by menu_screen_text_get_resident and
 * menu_screen_text_bind_section. */
typedef enum menu_screen_text_data {
    MENU_SCREEN_TEXT_DATA_SKILLSET_NAMES = 0,
    MENU_SCREEN_TEXT_DATA_JOB_NAMES = 1,
    MENU_SCREEN_TEXT_DATA_ABILITY_NAMES = 2,
    MENU_SCREEN_TEXT_DATA_FORMATION_MESSAGES = 3,   /* TEXT_SECTION_FORMATION_MESSAGES */
    MENU_SCREEN_TEXT_DATA_DISMISS_UNIT_REPLIES = 4, /* MENU_SCREEN_TEXT_SECTION_FORMATION_TEXT */
    MENU_SCREEN_TEXT_DATA_JOB_DESCRIPTIONS = 5,     /* MENU_SCREEN_TEXT_SECTION_FORMATION_TEXT */
    MENU_SCREEN_TEXT_DATA_NAME_ENTRY_KEYBOARD = 7,
    MENU_SCREEN_TEXT_DATA_SHOP_DIALOGUE = 8 /* MENU_SCREEN_TEXT_SECTION_SHOP_DIALOGUE */
} menu_screen_text_data_e;

/* The text sections WORLD's screens bind while they are up (text_section_e
 * has the resident ones). */
typedef enum menu_screen_text_section {
    MENU_SCREEN_TEXT_SECTION_TUTORIAL_MESSAGES = 23, /* the running tutorial's message file */
    MENU_SCREEN_TEXT_SECTION_NAME_ENTRY_KEYS = 28,   /* name entry: the key rows */
    MENU_SCREEN_TEXT_SECTION_FORMATION_TEXT = 29,    /* formation: what a dismissed unit says, or the job wheel's
                                                      * descriptions, whichever was bound last */
    MENU_SCREEN_TEXT_SECTION_SHOP_MENU_LABELS = 30,
    MENU_SCREEN_TEXT_SECTION_SHOP_DIALOGUE = 31 /* the shopkeeper's lines */
} menu_screen_text_section_e;

enum { TEXT_ID_FORMATION_TEXT_BASE = MENU_SCREEN_TEXT_SECTION_FORMATION_TEXT << TEXT_ID_SECTION_SHIFT };

extern text_bank_t g_menu_screen_text_bank;

u8* menu_screen_text_get_resident(menu_screen_text_data_e index);
u8* menu_screen_text_bind_section(u32 index);
const u8* menu_screen_text_advance_chars(const u8* text, s32 count);
s16 menu_screen_text_get_char_at(const u8* table, s16 index, s32 position);

/* text images */
void menu_screen_text_kern(const u8* text, const s16* text_ids, const RECT* destination, s32 flags);
s32 menu_screen_text_measure_width(const u8* entry);

/* text printing */
s32 menu_screen_text_check_printing(s32 thread_id);
void menu_screen_text_resume_printing(s32 thread_id);

/* music and sound effects */
extern u8 g_menu_screen_sound_release_held_loop;

void menu_screen_update_sound_effect(void);

#endif
