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
/* Ordinary outfitter screens use a town's world_location_id_e. These values
 * identify the other screens that reuse the formation menu framework. */
typedef enum world_menu_context {
    WORLD_MENU_CONTEXT_NAME_ENTRY = -2,
    WORLD_MENU_CONTEXT_FORMATION = -1,
    WORLD_MENU_CONTEXT_FUR_SHOP = 100,
    WORLD_MENU_CONTEXT_SOLDIER_OFFICE = 101,
} world_menu_context_e;

extern s16 g_world_menu_context_id; /* town location ID or world_menu_context_e */
extern s8 g_world_menu_music_slot;  /* formation music slot */

/* main menu */
/* A 0x24-byte menu thread group (g_world_menu_thread_groups, by request index 0-3): up to three menu threads the
 * main menu starts together. */
typedef struct world_menu_thread_group {
    void (*function[3])(void); /* 0x00; -1 when unused */
    s32 thread_id[3];          /* 0x0c; -1 when unused */
    s32 task_id[3];            /* 0x18 */
} world_menu_thread_group_t;

/* Partial image view used by world_menu_load_common_graphics. Upload rectangles determine the second page's size;
 * it begins at byte +0x8000 in both resource layouts. */
typedef struct world_menu_image_pages {
    u32 first_page[8192];
    u32 second_page[1];
} world_menu_image_pages_t;

extern world_menu_thread_group_t g_world_menu_thread_groups[];
extern s32 g_world_menu_thread_group_parameter;
extern s32 g_world_menu_thread_result;
extern s32 g_world_menu_event_state_flag;
extern s16 g_world_menu_screen_open_request;
extern s16 g_world_menu_open_entry_index;
extern s32 g_world_menu_main_scroll_direction;
extern s32 g_world_menu_main_scroll_threshold;
extern s32 g_world_menu_use_alternate_main_palette;
extern menu_entry_t g_world_menu_default_entries[];
extern u32 g_world_menu_default_entry_0_text_values[2]; /* text index + 0xa000, value */
extern s32 g_world_menu_default_entry_1_text_values[2]; /* value, 0 */
extern u8 g_world_menu_submenu_page_counts[];
extern u8* g_world_menu_submenu_page_text_ids[];
extern menu_scroll_text_layout_t g_world_menu_submenu_page_layout;
extern s16 g_world_menu_submenu_page_row_text_ids[];
extern s32 g_world_menu_number_entry_value;
extern s32 g_world_menu_text_file_buffer[];
extern s32 g_world_menu_location_id;
extern s32 g_world_menu_location_text_id;
extern s32 g_world_menu_location_window_x;
extern s32 g_world_menu_location_window_y;
extern u16 g_world_menu_location_list_text_ids[];
extern s16 g_world_menu_location_list_row_colors[];
extern menu_scroll_text_layout_t g_world_menu_location_list_layout;
extern RECT g_world_screen_clear_rect;
extern s16 g_world_debug_variable_list_request_state;
extern s16 g_world_fixed_number_panel_request_state;
void world_menu_run_main_mode(s32 mode);
void world_main_menu_text_window_thread(void);
void world_menu_open_entry_window(s32 index, s32 unused_x, s32 unused_y);
void world_menu_set_default_entry_0_text_values(u32 text_index, u32 value);
void world_menu_set_default_entry_1_text_values(s32 value);
void world_menu_set_slot_4_cursor_index(s32 index);
s32 world_menu_get_event_state_flag(void);
void world_menu_build_location_list(void);
/* Set up the WORLD menus with EVENT/WORLD.LZW from the main thread. */
void world_menu_init_with_world_text(void);
/* Set up the WORLD menus with EVENT/WORLD.LZW and the WLDFACE portraits. */
void world_menu_init_with_world_text_and_faces(void);
/* Set up the menus with EVENT/OPEN.LZW for the new-game prologue. */
void world_menu_init_with_open_text(void);
void world_menu_run_game_options(void);
void world_screen_build_at_list(void);

/* window frames */
extern RECT g_world_menu_frame_image_rect;
extern RECT g_world_menu_frame_palette_rect;
extern RECT g_world_menu_frame_palette_tail_rect;
extern RECT g_world_menu_item_image_rect;
extern RECT g_world_menu_item_palette_rect;
extern RECT g_world_menu_unit_image_rect_a;
extern RECT g_world_menu_unit_image_rect_b;
extern RECT g_world_menu_common_clut_rect;
extern clut_t g_world_menu_common_clut_data;
extern RECT g_world_screen_menu_thin_panel_texture_windows[7];
/* One libgpu texture window per DR_MODE packet (the seven RECTs after
 * g_world_screen_menu_thin_panel_texture_windows). The static table holds each
 * window's w/h; world_screen_menu_window_frame_thread fills in the runtime x/y.
 * Indexing the array reproduces the target's symbol-plus-byte-offset addressing. */
extern RECT g_world_screen_menu_panel_texture_windows[7];
extern fade_overlay_t g_world_screen_tint_tiles[];  /* one tint tile per packet buffer */
extern const u16 g_world_menu_zoom_percentages[12]; /* twelve crop percentages, indexed by step */
s32 world_menu_build_window_frame_image(s32 width, s32 height, u16* destination);
void world_screen_menu_build_window_image_row(const u16* source, u16* dest, s32 count, s32 offset);
void world_screen_menu_window_frame_thread(void);
void world_panel_gfx_init_scaled_draw_area_packets(scaled_draw_area_pair_t* pair);
void world_option_gfx_init_scaled_draw_area_packets(scaled_draw_area_pair_t* pair);
void world_menu_add_gradient_line_primitive(point16_t* endpoints, rgb24_t* colors, s32 semi_trans, s32 ot_index);
void world_menu_set_brightness(s32 red, s32 green, s32 blue);

/* palettes and colors */
extern u16 g_world_menu_clut;
extern u16 g_world_menu_window_tpage;
extern u16 g_world_menu_window_clut;
extern u16 g_world_menu_window_clut_normal;
extern u16 g_world_menu_window_clut_alt;
extern u16 g_world_menu_palette_clut_normal;
extern u16 g_world_menu_palette_clut_alt;
extern u16 g_world_menu_alternate_number_clut;
extern u16 g_world_menu_alternate_number_clut_normal;
extern u16 g_world_menu_alternate_number_clut_alt;
extern u16 g_world_menu_disabled_number_clut;
extern u16 g_world_menu_disabled_number_clut_normal;
extern u16 g_world_menu_disabled_number_clut_alt;
extern u16 g_world_menu_active_clut_front;
extern u16 g_world_menu_active_clut_back;
extern u16 g_world_menu_hp_gauge_clut;
extern u16 g_world_menu_mp_gauge_clut;
extern u16 g_world_menu_ct_gauge_clut;
extern u16 g_world_menu_background_clut;
extern u8 g_world_menu_use_alternate_palette;
extern s32 g_world_menu_color_red;
extern s32 g_world_menu_color_green;
extern s32 g_world_menu_color_blue;
extern rgb24_t g_world_menu_sprite_color;
void world_menu_set_palette_mode(s32 mode);
void world_menu_set_sprite_color(const rgb24_t* src);
rgb24_t* world_menu_gfx_get_draw_color(void);

/* display script commands */
enum { WORLD_MENU_SCRIPT_END = 0x1c };

typedef char world_menu_line_f2_size_must_be_16[(sizeof(LINE_F2) == 16) ? 1 : -1];

extern menu_callback_t g_world_menu_script_callbacks[16];
extern menu_sprite_command_t g_world_menu_script_sprite_command;
extern menu_window_command_t g_world_menu_window_interior_command;
extern s16 g_world_menu_window_scale_table[];
extern RECT g_world_menu_window_rect;
extern s16 g_world_menu_window_right_x;
extern u16 g_world_menu_texture_page;
extern s16 g_world_menu_row_height;
extern s32 g_world_menu_scroll_row_offset;
extern oriented_quad_t g_world_menu_number_blank_sprite;
extern s8 g_world_menu_cursor_bob_thresholds[];
extern s8 g_world_menu_cursor_bob_active_thresholds[];
extern textured_quad_source_t g_world_vertical_cursor_quad;
extern const RECT g_world_option_gfx_draw_area_template;
/* WORLD's display-script command handlers (menu_script.h has the shared ones). The CARD and JOBSTTS overlays call
 * world_menu_opcode_00_draw_window_frame directly. */
menu_instruction_t* world_menu_opcode_00_draw_window_frame(menu_instruction_t* instruction);
menu_instruction_t* world_menu_opcode_01_if_group(menu_instruction_t* instruction);
menu_instruction_t* world_menu_opcode_02_if_else_group(menu_instruction_t* instruction);
menu_instruction_t* world_menu_opcode_03_draw_tile(menu_instruction_t* instruction);
menu_instruction_t* world_menu_opcode_04_draw_textured_quad(menu_instruction_t* instruction);
menu_instruction_t* world_menu_opcode_06_draw_callback_quad(menu_instruction_t* instruction);
menu_instruction_t* world_menu_opcode_07_draw_line(menu_instruction_t* instruction);
menu_instruction_t* world_menu_opcode_09_set_fixed_clut(menu_instruction_t* instruction);
menu_instruction_t* world_menu_opcode_0a_set_clut(menu_instruction_t* instruction);
menu_instruction_t* world_menu_opcode_0b_set_draw_color(menu_instruction_t* instruction);
menu_instruction_t* world_menu_opcode_0c_set_texture_page(menu_instruction_t* instruction);
menu_instruction_t* world_menu_opcode_0e_set_semi_trans(menu_instruction_t* instruction);
menu_instruction_t* world_menu_opcode_0f_set_draw_priority(menu_instruction_t* instruction);
menu_instruction_t* world_menu_opcode_11_draw_tiled_rectangle(menu_instruction_t* instruction);
menu_instruction_t* world_menu_opcode_12_draw_opening_window_frame(menu_instruction_t* instruction);
menu_instruction_t* world_menu_opcode_13_clear_temporary_draw_area(menu_instruction_t* instruction);
menu_instruction_t* world_menu_opcode_14_set_draw_area(menu_instruction_t* instruction);
menu_instruction_t* world_menu_opcode_18_add_blend_draw_mode(menu_instruction_t* instruction);
menu_instruction_t* world_menu_opcode_0d_draw_formatted_number(menu_instruction_t* instruction);
menu_instruction_t* world_menu_opcode_17_draw_right_aligned_number(menu_instruction_t* instruction);
menu_instruction_t* world_menu_opcode_10_draw_scrollable_list(menu_instruction_t* instruction);
menu_instruction_t* world_menu_opcode_15_repeat_draw_area(menu_instruction_t* instruction);

/* scrollable lists */
extern const s16* g_world_menu_list_entry_ids;
extern u8* g_world_menu_list_text_table;
extern s16 g_world_menu_list_row_text_ids[];
extern s16 g_world_menu_list_visible_rows;
extern s16 g_world_menu_list_row_record_count;
extern s8 g_world_menu_list_layout_pending;
extern u16 g_world_menu_list_image_width;
extern u8 g_world_menu_list_glyph_width;
extern u8 g_world_menu_list_text_columns;
extern s8 g_world_menu_list_scroll_direction;
extern u16 g_world_menu_list_cursor_anim;
extern s16 g_world_menu_use_scroll_position;
extern u16 g_world_menu_scroll_hold_counter;
extern u16 g_world_menu_scroll_repeat_timer;
extern s32 g_world_menu_scrolling_list_depth;
extern s16 g_world_menu_text_redraw_request;
void world_menu_init_and_load_scrollable_list(
    const s16* values, s32 selected_index, u8* text_table, menu_instruction_t* script);
void world_menu_load_scrollable_list_layout(menu_instruction_t* script);
void world_menu_init_and_draw_scrollable_list(
    const s16* entries, s32 selected_index, s32 scroll_base_index, u8* text_table, const menu_instruction_t* script);
void world_menu_scroll_list_by_page(menu_vertical_scroll_direction_e direction, const menu_instruction_t* list);
menu_vertical_scroll_direction_e world_menu_input_read_page_scroll_direction(void);
void world_menu_draw_row_window_and_cursor(s32 wide, s32 row, s32 mode);
void world_screen_menu_draw_text_columns(menu_entry_t* entry, s32* row_offset, u8* buffer);
/* Also used by the event library's icon selection and the scrolling ability list thread. */
u8* world_menu_redraw_text_page_on_scroll_in_rect(
    menu_entry_t* entry, s32* row_offset, s32* redraw, s32* unused, RECT* area);

/* cursors, selections and input */
/* Result slots the shop menus share. */
enum {
    WORLD_MENU_SELECTION_SHOP_QUANTITY = 3, /* chosen purchase/sale quantity */
    WORLD_MENU_SELECTION_PROMPT_RESULT = 4, /* yes/no prompt result (0 confirms) */
};

/* Index of a saved cursor in g_menu_selection_values, as the formation
 * and shop menus use them. */
typedef enum world_menu_cursor {
    WORLD_MENU_CURSOR_SHOP_ITEM_TAB = 0,
    WORLD_MENU_CURSOR_EQUIP_SLOT = 2,
    WORLD_MENU_CURSOR_REMOVE_SLOT = 3, /* equipment slot of the remove and sell menus */
    WORLD_MENU_CURSOR_SET_ABILITY_SLOT = 4,
    WORLD_MENU_CURSOR_REMOVE_ABILITY_SLOT = 5,
    WORLD_MENU_CURSOR_ITEM_TAB = 7,
    WORLD_MENU_CURSOR_ABILITY_TAB = 8,
} world_menu_cursor_e;

/* Result slot per menu (-1 while none); world_menu_reset_selection_results clears all 16. */
extern s16 g_world_menu_selection_results[16];
extern u8 g_world_menu_prompt_active;
/* Per-piece 4.12 fixed-point x/y scale; a negative factor flips that axis. */
extern menu_button_piece_t g_world_menu_button_pieces[];
extern point16_t g_world_menu_button_piece_scales[];
void world_menu_reset_selection_results(void);
s16 world_menu_init_cursor_position(u8 index, s16 value);
void world_menu_get_selection_record(s32 index, s16* selected_index, s16* scroll_base_index, s16* entries);
s32 world_menu_input_read_tutorial_or_controller(s32 unused_0);
void world_menu_input_update_dialog_state(void);
void world_menu_input_update_with_message_state(void);
void world_menu_update_and_draw_sliding_cursor(point16_t* pos, u16* state, s32 mode);
void world_menu_update_and_draw_bouncing_cursor(point16_t* pos, u16* state, s32 mode);
void world_menu_draw_pressable_button(s16 index, s32 x, s16 y, s16 pressed, s16 disabled, u16 sound_effect_id);

/* grid menus */
/* One cell of a directional grid menu: a description text id (base plus
 * offset), the cell index reached by each d-pad direction, and the cursor
 * position. A non-zero `vertical` draws the vertical cursor. */
typedef struct world_menu_grid_cell {
    s16* text_id;
    u16 text_offset;
    u8 next_up;
    u8 next_down;
    u8 next_right;
    u8 next_left;
    point16_t cursor;
    s16 vertical;
} world_menu_grid_cell_t;

extern world_menu_grid_cell_t g_world_grid_menu_default_cells[];
extern world_menu_grid_cell_t g_world_grid_menu_shop_cells[];
extern world_menu_grid_cell_t g_world_grid_menu_shop_scrolled_cells[];
extern world_menu_grid_cell_t* g_world_active_grid_menu_cells;
extern u8 g_world_grid_menu_selected_cell; /* selected cell */
extern u16 g_world_grid_menu_cursor_anim;  /* grid cursor animation state */
extern u8 g_world_grid_menu_initialized;   /* grid menu initialized */
extern s32 g_world_grid_menu_id;           /* menu_grid_help_id_e */
void world_menu_update_grid_cursor(s16 alternate);
void world_menu_start_description_text_thread(s32 position);

/* number entries */
/* Value range of a numeric menu entry (world_menu_number_entry_thread and
 * world_menu_labeled_number_entry_thread). */
typedef struct world_menu_number_range {
    ranges16_t bounds;       /* 0x00 */
    s16 _unknown_04[2];      /* 0x04: -1 from wldcore_window_init_number_panel_render_thread */
    void (*on_change)(void); /* 0x08 */
} world_menu_number_range_t;

extern s16* g_world_menu_entry_text_id_tables[]; /* 16-bit tables selected by entry select_text_table */
extern s32 g_world_menu_number_entry_digit_steps[];
extern u32* g_world_menu_list_controller_input;
void world_menu_number_entry_thread(void);
void world_menu_labeled_number_entry_thread(void);
void world_menu_announce_entry_value_thread(void);
void world_menu_resize_parent_entry_to_digits(void);

/* unit panels */
/* The status panels of the selected unit and of the unit it is compared with, and the stat preview panel. */
extern menu_unit_status_identity_t g_world_selected_unit_identity;
extern struct menu_status_panel_slot_storage g_world_selected_unit_stat_detail;
extern menu_text_binding_t g_world_selected_unit_text_binding;
extern menu_number_entry_t g_world_selected_unit_number_entries[];
extern RECT g_world_selected_unit_portrait_rect;
/* Panel scratch aliases inside g_thread_contexts[0] and [14], at
 * +0x100 and +0x200 in each slot's stack. These stay separate symbols;
 * the palette areas hold one slot per page. */
extern menu_status_panel_editor_packet_t g_world_selected_unit_summary_packets[2];
extern menu_status_panel_packet_t g_world_selected_unit_status_packets[2];
extern u8 g_world_selected_unit_stat_text_image[];
extern u8 g_world_selected_unit_name_text_image[];
extern menu_unit_number_images_t g_world_selected_unit_number_text_images;
extern palette_t g_world_selected_unit_portrait_palettes[2];
extern menu_unit_status_record_t g_world_comparison_unit_stat_summary;
extern menu_unit_status_identity_t g_world_comparison_unit_identity;
extern menu_number_entry_t g_world_comparison_unit_number_entries[];
extern RECT g_world_comparison_unit_portrait_rect;
extern menu_status_panel_editor_packet_t g_world_comparison_unit_summary_packets[2];
extern menu_status_panel_packet_t g_world_comparison_unit_status_packets[2];
extern u8 g_world_comparison_unit_stat_text_image[];
extern u8 g_world_comparison_unit_name_text_image[];
extern menu_unit_number_images_t g_world_comparison_unit_number_text_images;
extern palette_t g_world_comparison_unit_portrait_palettes[2];
extern menu_status_panel_buffer_t g_world_stat_preview_panel_frames[2];
extern menu_status_thread_t g_world_stat_preview_panel_thread_params;
extern s32 g_world_preview_stats_window_active;
extern menu_status_thread_t g_world_change_banner_panel_thread_params;
s32 world_menu_get_party_unit_name_id(void);
s32 world_menu_get_selected_unit_hp_bonus(s32 unused_row);
s32 world_menu_get_selected_unit_mp_bonus(s32 unused_row);
void world_menu_request_unit_panel_redraw(void);
s32 world_menu_run_unit_status_banner_countdown(s32 message_id, s32 frames);
void world_menu_toggle_preview_stats_window(s32 flag);
void world_menu_toggle_stat_preview_panel_thread(s32 enable);
void world_menu_toggle_change_banner_panel_thread(s32 enable);
void world_menu_toggle_ability_panel_thread(s32 enable);
void world_menu_start_equipment_and_ability_panel_thread(s32 enabled);

/* formation menus */
extern s8 g_world_menu_order_unit_open;
extern s16 g_world_menu_order_unit_cursor;
extern s16 g_world_menu_order_unit_entries[];
void world_menu_run_order_unit(void);
void world_menu_run_ability(void);
s32 world_menu_run_learn_job_list(void);
s32 world_menu_get_learn_job_jp(s32 index);
s32 world_menu_has_items(s32 mode);

#endif
