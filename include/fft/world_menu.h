#ifndef WORLD_MENU_H
#define WORLD_MENU_H

/* WORLD's menu framework: menu contexts, the main menu and its thread groups, window frames, palettes, the display
 * script commands, scrollable lists, cursors and selections, grid menus, number entries, and the selected and
 * comparison unit panels. */

#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/menu_panel.h"
#include "fft/menu_script.h"
#include "fft/menu_status.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* menu context */
/* Ordinary outfitter screens use a town's location_id_e. These values
 * identify the other screens that reuse the formation menu framework. */
typedef enum screen_context {
    SCREEN_CONTEXT_NAME_ENTRY = -2,
    SCREEN_CONTEXT_FORMATION = -1,
    SCREEN_CONTEXT_FUR_SHOP = 100,
    SCREEN_CONTEXT_SOLDIER_OFFICE = 101,
} screen_context_e;

extern s16 g_screen_context_id; /* town location ID or screen_context_e */
extern s8 g_screen_music_slot;  /* formation music slot */

/* main menu */
/* A 0x24-byte menu thread group (g_world_map_menu_groups, by request index 0-3): up to three menu threads the
 * main menu starts together. */
typedef struct world_map_menu_group {
    void (*function[3])(void); /* 0x00; -1 when unused */
    s32 thread_id[3];          /* 0x0c; -1 when unused */
    s32 task_id[3];            /* 0x18 */
} world_map_menu_group_t;

/* Partial image view used by screen_load_common_graphics. Upload rectangles determine the second page's size;
 * it begins at byte +0x8000 in both resource layouts. */
typedef struct screen_image_pages {
    u32 first_page[8192];
    u32 second_page[1];
} screen_image_pages_t;

extern world_map_menu_group_t g_world_map_menu_groups[];
extern s32 g_world_map_menu_group_parameter;
extern s32 g_screen_thread_result;
extern s32 g_screen_dialog_waiting;
extern s16 g_world_map_screen_request;
extern s16 g_screen_open_entry_index;
extern s32 g_world_map_menu_scroll_direction;
extern s32 g_world_map_menu_scroll_threshold;
extern s32 g_world_map_menu_dimmed;
extern menu_entry_t g_screen_default_entries[];
extern u32 g_screen_default_entry_0_text_values[2]; /* text index + 0xa000, value */
extern s32 g_screen_default_entry_1_text_values[2]; /* value, 0 */
extern u8 g_screen_submenu_page_counts[];
extern u8* g_screen_submenu_page_text_ids[];
extern menu_scroll_text_layout_t g_screen_submenu_page_layout;
extern s32 g_screen_number_entry_value;
extern s32 g_world_map_destination_id;
extern s32 g_world_map_destination_text_id;
extern s32 g_world_map_destination_window_x;
extern s32 g_world_map_destination_window_y;
extern u16 g_world_map_destination_list_text_ids[];
extern s16 g_world_map_destination_list_row_colors[];
extern menu_scroll_text_layout_t g_world_map_destination_list_layout;
extern RECT g_screen_clear_rect;
extern s16 g_world_map_debug_variable_list_request_state;
extern s16 g_world_map_fixed_number_panel_request_state;
void option_entrypoint(s32 mode);
void world_map_menu_text_window_thread(void);
void screen_open_entry_window(s32 index, s32 unused_x, s32 unused_y);
void screen_set_default_entry_0_text(u32 text_index, u32 value);
void screen_set_default_entry_1_text(s32 value);
void screen_set_entry_4_cursor(s32 index);
s32 screen_is_dialog_waiting(void);
void world_map_destination_list_thread(void);
/* Set up the WORLD menus with EVENT/WORLD.LZW from the main thread. */
void screen_init_with_world_text(void);
/* Set up the WORLD menus with EVENT/WORLD.LZW and the WLDFACE portraits. */
void screen_init_with_world_text_and_faces(void);
/* Set up the menus with EVENT/OPEN.LZW for the new-game prologue. */
void screen_init_with_open_text(void);
void option_game_options_thread(void);
void screen_text_list_thread(void);

/* window frames */
extern RECT g_screen_frame_image_rect;
extern RECT g_screen_frame_palette_rect;
extern RECT g_screen_frame_palette_tail_rect;
extern RECT g_screen_item_image_rect;
extern RECT g_screen_item_palette_rect;
extern RECT g_screen_unit_image_rect_a;
extern RECT g_screen_unit_image_rect_b;
extern RECT g_screen_common_clut_rect;
extern clut_t g_screen_common_clut_data;
extern RECT g_screen_thin_panel_texture_windows[7];
/* One libgpu texture window per DR_MODE packet (the seven RECTs after
 * g_screen_thin_panel_texture_windows). The static table holds each
 * window's w/h; screen_window_frame_thread fills in the runtime x/y.
 * Indexing the array reproduces the target's symbol-plus-byte-offset addressing. */
extern RECT g_screen_panel_texture_windows[7];
extern fade_overlay_t g_screen_tint_tiles[];    /* one tint tile per packet buffer */
extern const u16 g_screen_zoom_percentages[12]; /* twelve crop percentages, indexed by step */
s32 screen_build_window_frame_image(s32 width, s32 height, u16* destination);
void screen_build_window_image_row(const u16* source, u16* dest, s32 count, s32 offset);
void screen_window_frame_thread(void);
void screen_panel_init_scaled_draw_area_packets(scaled_draw_area_pair_t* pair);
void option_init_scaled_draw_area_packets(scaled_draw_area_pair_t* pair);
void screen_add_gradient_line(point16_t* endpoints, rgb24_t* colors, s32 semi_trans, s32 ot_index);
void screen_set_brightness(s32 red, s32 green, s32 blue);

/* palettes and colors */
extern u16 g_screen_clut;
extern u16 g_screen_window_tpage;
extern u16 g_screen_window_clut;
extern u16 g_screen_window_clut_normal;
extern u16 g_screen_window_clut_alt;
extern u16 g_screen_palette_clut_normal;
extern u16 g_screen_palette_clut_alt;
extern u16 g_screen_alternate_number_clut;
extern u16 g_screen_alternate_number_clut_normal;
extern u16 g_screen_alternate_number_clut_alt;
extern u16 g_screen_disabled_number_clut;
extern u16 g_screen_disabled_number_clut_normal;
extern u16 g_screen_disabled_number_clut_alt;
extern u16 g_screen_active_clut_front;
extern u16 g_screen_active_clut_back;
extern u16 g_screen_hp_gauge_clut;
extern u16 g_screen_mp_gauge_clut;
extern u16 g_screen_ct_gauge_clut;
extern u16 g_screen_background_clut;
extern u8 g_screen_use_alternate_palette;
extern s32 g_screen_color_red;
extern s32 g_screen_color_green;
extern s32 g_screen_color_blue;
extern rgb24_t g_screen_sprite_color;
void screen_set_palette_mode(s32 mode);
void screen_set_sprite_color(const rgb24_t* src);
rgb24_t* screen_get_draw_color(void);

/* display script commands */
enum { SCREEN_SCRIPT_END = 0x1c };

typedef char world_menu_line_f2_size_must_be_16[(sizeof(LINE_F2) == 16) ? 1 : -1];

extern menu_callback_t g_screen_script_callbacks[16];
extern RECT g_screen_window_rect;
extern s16 g_screen_window_right_x;
extern u16 g_screen_texture_page;
extern s16 g_screen_row_height;
extern s32 g_screen_scroll_row_offset;
extern textured_quad_source_t g_screen_vertical_cursor_quad;
extern const RECT g_option_draw_area_template;
/* WORLD's display-script command handlers (menu_script.h has the shared ones). The CARD and JOBSTTS overlays call
 * menu_opcode_00_draw_window_frame directly. */
menu_instruction_t* menu_opcode_00_draw_window_frame(menu_instruction_t* instruction);
menu_instruction_t* menu_opcode_01_if_group(menu_instruction_t* instruction);
menu_instruction_t* menu_opcode_02_if_else_group(menu_instruction_t* instruction);
menu_instruction_t* menu_opcode_03_draw_tile(menu_instruction_t* instruction);
menu_instruction_t* menu_opcode_04_draw_textured_quad(menu_instruction_t* instruction);
menu_instruction_t* menu_opcode_06_draw_callback_quad(menu_instruction_t* instruction);
menu_instruction_t* menu_opcode_07_draw_line(menu_instruction_t* instruction);
menu_instruction_t* menu_opcode_09_set_fixed_clut(menu_instruction_t* instruction);
menu_instruction_t* menu_opcode_0a_set_clut(menu_instruction_t* instruction);
menu_instruction_t* menu_opcode_0b_set_draw_color(menu_instruction_t* instruction);
menu_instruction_t* menu_opcode_0c_set_texture_page(menu_instruction_t* instruction);
menu_instruction_t* menu_opcode_0d_draw_formatted_number(menu_instruction_t* instruction);
menu_instruction_t* menu_opcode_0e_set_semi_trans(menu_instruction_t* instruction);
menu_instruction_t* menu_opcode_0f_set_draw_priority(menu_instruction_t* instruction);
menu_instruction_t* menu_opcode_10_draw_scrollable_list(menu_instruction_t* instruction);
menu_instruction_t* menu_opcode_11_draw_background_tiles(menu_instruction_t* instruction);
menu_instruction_t* menu_opcode_12_draw_opening_window_frame(menu_instruction_t* instruction);
menu_instruction_t* menu_opcode_13_clear_temporary_draw_area(menu_instruction_t* instruction);
menu_instruction_t* menu_opcode_14_set_draw_area(menu_instruction_t* instruction);
menu_instruction_t* menu_opcode_15_repeat_draw_area(menu_instruction_t* instruction);
menu_instruction_t* menu_opcode_17_draw_right_aligned_number(menu_instruction_t* instruction);
menu_instruction_t* menu_opcode_18_add_blend_draw_mode(menu_instruction_t* instruction);

/* scrollable lists */
extern const s16* g_screen_list_entry_ids;
extern u8* g_screen_list_text_table;
extern s16 g_screen_list_row_text_ids[];
extern s16 g_screen_list_visible_rows;
extern s16 g_screen_list_row_record_count;
extern s8 g_screen_list_layout_pending;
extern u16 g_screen_list_image_width;
extern u8 g_screen_list_glyph_width;
extern u8 g_screen_list_text_columns;
extern s8 g_screen_list_scroll_direction;
extern u16 g_screen_list_cursor_anim;
extern s16 g_screen_use_scroll_position;
extern u16 g_screen_scroll_hold_counter;
extern u16 g_screen_scroll_repeat_timer;
extern s32 g_screen_scrolling_list_depth;
extern s16 g_screen_text_redraw_request;
void screen_init_list(const s16* values, s32 selected_index, u8* text_table, menu_instruction_t* script);
void screen_load_list_layout(menu_instruction_t* script);
void screen_init_and_draw_list(
    const s16* entries, s32 selected_index, s32 scroll_base_index, u8* text_table, const menu_instruction_t* script);
void screen_scroll_list_page(menu_vertical_scroll_direction_e direction, const menu_instruction_t* list);
menu_vertical_scroll_direction_e menu_input_read_page_scroll_direction(void);
void screen_draw_item_tabs(s32 wide, s32 row, s32 mode);
void screen_draw_text_columns(menu_entry_t* entry, s32* row_offset, u8* buffer);
/* Also used by the event library's icon selection and the scrolling ability list thread. */
u8* menu_redraw_text_page_on_scroll_in_rect(menu_entry_t* entry, s32* row_offset, s32* redraw, s32* unused, RECT* area);

/* cursors, selections and input */
/* Result slots the shop menus share. */
enum {
    SCREEN_SELECTION_SHOP_QUANTITY = 3, /* chosen purchase/sale quantity */
    SCREEN_SELECTION_PROMPT_RESULT = 4, /* yes/no prompt result (0 confirms) */
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
    SCREEN_CURSOR_ABILITY_TAB = 8,
} screen_cursor_e;

/* Result slot per menu (-1 while none); screen_reset_selection_results clears all 16. */
extern s16 g_screen_selection_results[16];
extern u8 g_screen_prompt_active;
/* Per-piece 4.12 fixed-point x/y scale; a negative factor flips that axis. */
extern menu_button_piece_t g_screen_button_pieces[];
extern point16_t g_screen_button_piece_scales[];
void screen_reset_selection_results(void);
s16 screen_init_cursor_position(u8 index, s16 value);
void menu_get_selection_record(s32 index, s16* selected_index, s16* scroll_base_index, s16* entries);
s32 screen_input_read(s32 unused_0);
void screen_input_update_dialog_state(void);
void menu_input_update_with_message_state(void);
void screen_draw_sliding_cursor(point16_t* pos, u16* state, s32 mode);
void menu_update_and_draw_bouncing_cursor(point16_t* pos, u16* state, s32 mode);
void menu_draw_pressable_button(s16 index, s32 x, s16 y, s16 pressed, s16 disabled, u16 sound_effect_id);

/* grid menus */
/* One cell of a directional grid menu: a description text id (base plus
 * offset), the cell index reached by each d-pad direction, and the cursor
 * position. A non-zero `vertical` draws the vertical cursor. */
typedef struct screen_grid_cell {
    s16* text_id;
    u16 text_offset;
    u8 next_up;
    u8 next_down;
    u8 next_right;
    u8 next_left;
    point16_t cursor;
    s16 vertical;
} screen_grid_cell_t;

extern screen_grid_cell_t g_screen_grid_default_cells[];
extern screen_grid_cell_t g_screen_grid_shop_cells[];
extern screen_grid_cell_t g_screen_grid_shop_scrolled_cells[];
extern screen_grid_cell_t* g_screen_grid_active_cells;
extern u8 g_screen_grid_selected_cell; /* selected cell */
extern u16 g_screen_grid_cursor_anim;  /* grid cursor animation state */
extern u8 g_screen_grid_initialized;   /* grid menu initialized */
extern s32 g_screen_grid_id;           /* menu_grid_help_id_e */
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

extern s16* g_screen_entry_text_id_tables[]; /* 16-bit tables selected by entry select_text_table */
extern s32 g_screen_number_entry_digit_steps[];
extern u32* g_screen_list_controller_input;
void screen_number_entry_thread(void);
void screen_labeled_number_entry_thread(void);
void screen_help_text_thread(void);
void screen_fit_parent_entry_to_digits(void);

/* unit panels */
/* The status panels of the selected unit and of the unit it is compared with, and the stat preview panel. */
extern menu_unit_status_identity_t g_formation_selected_unit_identity;
extern struct menu_status_panel_slot_storage g_formation_selected_unit_stat_detail;
extern menu_text_binding_t g_formation_selected_unit_text_binding;
extern menu_number_entry_t g_formation_selected_unit_number_entries[];
/* Panel scratch aliases inside g_thread_contexts[0] and [14], at
 * +0x100 and +0x200 in each slot's stack. These stay separate symbols;
 * the palette areas hold one slot per page. */
extern menu_status_panel_editor_packet_t g_formation_selected_unit_summary_packets[2];
extern menu_status_panel_packet_t g_formation_selected_unit_status_packets[2];
extern u8 g_formation_selected_unit_stat_text_image[];
extern u8 g_formation_selected_unit_name_text_image[];
extern menu_unit_number_images_t g_formation_selected_unit_number_text_images;
extern palette_t g_formation_selected_unit_portrait_palettes[2];
extern menu_unit_status_record_t g_formation_comparison_unit_stat_summary;
extern menu_unit_status_identity_t g_formation_comparison_unit_identity;
extern menu_number_entry_t g_formation_comparison_unit_number_entries[];
extern menu_status_panel_editor_packet_t g_formation_comparison_unit_summary_packets[2];
extern menu_status_panel_packet_t g_formation_comparison_unit_status_packets[2];
extern u8 g_formation_comparison_unit_stat_text_image[];
extern u8 g_formation_comparison_unit_name_text_image[];
extern menu_unit_number_images_t g_formation_comparison_unit_number_text_images;
extern palette_t g_formation_comparison_unit_portrait_palettes[2];
extern menu_status_panel_buffer_t g_formation_stat_preview_panel_frames[2];
extern menu_status_thread_t g_formation_stat_preview_panel_thread_params;
extern s32 g_formation_preview_stats_window_active;
extern menu_status_thread_t g_formation_change_banner_thread_params;
s32 formation_get_selected_unit_name_id(void);
s32 formation_item_get_hp_bonus(s32 unused_row);
s32 formation_item_get_mp_bonus(s32 unused_row);
void formation_redraw_panels(void);
s32 formation_banner_countdown(s32 message_id, s32 frames);
void formation_toggle_preview_stats_window(s32 flag);
void formation_toggle_stat_preview_panel(s32 enable);
void formation_toggle_change_panel(s32 enable);
void formation_toggle_ability_panel(s32 enable);
void formation_open_equipment_panel(s32 enabled);

/* formation menus */
extern s8 g_formation_order_open;
extern s16 g_formation_order_cursor;
extern s16 g_formation_order_entries[];
void formation_order_menu(void);
void formation_ability_menu(void);
s32 formation_ability_learn_job_list(void);
s32 formation_ability_learn_job_jp(s32 index);
s32 formation_has_items(s32 mode);

#endif
