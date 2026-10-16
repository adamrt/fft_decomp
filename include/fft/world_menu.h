#ifndef WORLD_MENU_H
#define WORLD_MENU_H

/* WORLD's menu screens: panels, grids, the main menu and the selected and comparison unit displays. */

#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/menu_script.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* State of a screen request raised from the world map menu (the
 * g_world_*_request_state globals stepped by world_formation_run_menu_frame):
 * fade out, run the screen, fade back in. States 4 to 7 only count frames. */
typedef enum world_screen_request_state {
    WORLD_SCREEN_REQUEST_IDLE = 0,
    WORLD_SCREEN_REQUEST_START_FADE_OUT = 1,
    WORLD_SCREEN_REQUEST_WAIT_FADE_OUT = 2,
    WORLD_SCREEN_REQUEST_RUN = 3,
    WORLD_SCREEN_REQUEST_START_FADE_IN = 8,
    WORLD_SCREEN_REQUEST_WAIT_FADE_IN = 9,
} world_screen_request_state_e;

/* Ordinary outfitter screens use a town's world_location_id_e. These values
 * identify the other screens that reuse the formation menu framework. */
typedef enum world_menu_context {
    WORLD_MENU_CONTEXT_NAME_ENTRY = -2,
    WORLD_MENU_CONTEXT_FORMATION = -1,
    WORLD_MENU_CONTEXT_FUR_SHOP = 100,
    WORLD_MENU_CONTEXT_SOLDIER_OFFICE = 101,
} world_menu_context_e;

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

/* 0x24-byte menu thread-group record at 0x80189ecc, indexed by
 * the 32-bit request index (0-3). Each group names up to three menu threads. */
typedef struct world_menu_thread_group {
    void (*function[3])(void); /* 0x00; -1 when unused */
    s32 thread_id[3];          /* 0x0c; -1 when unused */
    s32 task_id[3];            /* 0x18 */
} world_menu_thread_group_t;

/* The builder walks the table with a byte offset stepping by 8 (the
 * target's induction variable), so it addresses these fields by offset. */
enum world_menu_line_layout_offset {
    WORLD_MENU_LINE_LAYOUT_X0 = 0,
    WORLD_MENU_LINE_LAYOUT_Y0 = 1,
    WORLD_MENU_LINE_LAYOUT_X1 = 2,
    WORLD_MENU_LINE_LAYOUT_Y1 = 3,
    WORLD_MENU_LINE_LAYOUT_ADD_W0 = 4,
    WORLD_MENU_LINE_LAYOUT_ADD_H0 = 5,
    WORLD_MENU_LINE_LAYOUT_ADD_W1 = 6,
    WORLD_MENU_LINE_LAYOUT_ADD_H1 = 7,
    WORLD_MENU_LINE_LAYOUT_SIZE = 8
};

/* Partial image view used by 0x801061e0. Upload rectangles determine the
 * second page's size; it begins at byte +0x8000 in both resource layouts. */
typedef struct world_menu_image_pages {
    u32 first_page[8192];
    u32 second_page[1];
} world_menu_image_pages_t;

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

enum { WORLD_MENU_SCRIPT_END = 0x1c };

/* dead unit */
extern RECT g_world_screen_menu_thin_panel_texture_windows[7];
/* One libgpu texture window per DR_MODE packet (the seven RECTs after
 * g_world_screen_menu_thin_panel_texture_windows). The static table holds each
 * window's w/h; world_screen_menu_window_frame_thread fills in the runtime x/y.
 * Indexing the array reproduces the target's symbol-plus-byte-offset addressing. */
extern RECT g_world_screen_menu_panel_texture_windows[7];

/* locations */
extern u16 g_world_menu_location_list_text_ids[];
extern s16 g_world_menu_location_list_row_colors[];
extern menu_scroll_text_layout_t g_world_menu_location_list_layout;
void world_menu_build_location_list(void);

/* screen */
extern RECT g_world_screen_clear_rect;
extern fade_overlay_t g_world_screen_tint_tiles[]; /* one tint tile per packet buffer */

/* turn */
void world_screen_build_at_list(void);

/* comparison */
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

/* gfx */
extern menu_status_panel_buffer_t g_world_change_banner_panel_frames[2];
extern menu_status_thread_t g_world_change_banner_panel_thread_params;
extern textured_quad_source_t g_world_vertical_cursor_quad;
extern u8 g_world_change_banner_text_image[];
void world_panel_gfx_init_scaled_draw_area_packets(scaled_draw_area_pair_t* pair);

/* input */
s32 world_menu_input_read_tutorial_or_controller(s32 unused_0);

/* unit */
extern menu_status_panel_buffer_t g_world_stat_preview_panel_frames[2];
extern struct menu_status_panel_slot_storage g_world_selected_unit_stat_detail;
extern menu_unit_status_identity_t g_world_selected_unit_identity;
extern menu_number_entry_t g_world_selected_unit_number_entries[];
extern RECT g_world_selected_unit_portrait_rect;
extern menu_status_thread_t g_world_stat_preview_panel_thread_params;
extern menu_text_binding_t g_world_selected_unit_text_binding;
/* Panel scratch aliases inside g_thread_contexts[0] and [14], at
 * +0x100 and +0x200 in each slot's stack. These stay separate symbols;
 * the palette areas hold one slot per page. */
extern menu_status_panel_editor_packet_t g_world_selected_unit_summary_packets[2];
extern menu_status_panel_packet_t g_world_selected_unit_status_packets[2];
extern u8 g_world_selected_unit_stat_text_image[];
extern u8 g_world_selected_unit_name_text_image[];
extern menu_unit_number_images_t g_world_selected_unit_number_text_images;
extern palette_t g_world_selected_unit_portrait_palettes[2];
extern u8 g_world_stat_preview_text_image[];

/* text */
extern s16 g_world_fixed_number_panel_request_state;

/* menu context */
extern s16 g_world_menu_context_id; /* town location ID or world_menu_context_e */

/* menu */
extern u16 g_world_menu_clut;
extern s32 g_world_menu_color_blue;
extern s32 g_world_menu_color_green;
extern s32 g_world_menu_color_red;
extern s32 g_world_menu_event_state_flag;
extern s32 g_world_menu_main_scroll_direction;
extern s32 g_world_menu_main_scroll_threshold;
extern s16 g_world_menu_order_unit_cursor;
extern s16 g_world_menu_order_unit_entries[];
extern s8 g_world_menu_order_unit_open;
extern u8 g_world_menu_prompt_active;
extern const RECT g_world_option_gfx_draw_area_template;
extern s16 g_world_menu_row_height;
extern s16 g_world_menu_screen_open_request;
extern s32 g_world_menu_scroll_row_offset;
extern rgb24_t g_world_menu_sprite_color;
extern s32 g_world_menu_text_file_buffer[];
/* Provisional: menu text origin/stride fields at 0x80153280 (the WORLD
 * script variable region; menu_set_text_origin writes +0x04/+0x06).
 * The text-image builders address these through one base register, so they
 * are modeled as one aggregate; the two 32-bit parameters at 0x80153304 and
 * 0x80153308 schedule as plain scalars and stay separate globals. */
extern u16 g_world_menu_texture_page;
extern s32 g_world_menu_thread_result;
extern s32 g_world_menu_use_alternate_main_palette;
extern u8 g_world_menu_use_alternate_palette;
extern s16 g_world_menu_use_scroll_position;
extern s32 g_world_menu_scrolling_list_depth;
extern s16 g_world_menu_open_entry_index;
extern world_menu_thread_group_t g_world_menu_thread_groups[];
extern s32 g_world_menu_thread_group_parameter;
extern s32 g_world_menu_location_id;
extern s32 g_world_menu_location_window_x;
extern s32 g_world_menu_location_window_y;
extern u32 g_world_menu_default_entry_0_text_values[2]; /* text index + 0xa000, value */
extern s32 g_world_menu_default_entry_1_text_values[2]; /* value, 0 */
extern s32 g_world_menu_location_text_id;
extern u8 g_world_menu_submenu_page_counts[];
extern u8* g_world_menu_submenu_page_text_ids[];
extern menu_scroll_text_layout_t g_world_menu_submenu_page_layout;
extern s32 g_world_menu_number_entry_value;
extern menu_entry_t g_world_menu_default_entries[];
extern RECT g_world_menu_frame_palette_rect;
extern RECT g_world_menu_frame_palette_tail_rect;
extern RECT g_world_menu_frame_image_rect;
extern RECT g_world_menu_item_image_rect;
extern RECT g_world_menu_common_clut_rect;
extern RECT g_world_menu_item_palette_rect;
extern RECT g_world_menu_unit_image_rect_a;
extern RECT g_world_menu_unit_image_rect_b;
extern clut_t g_world_menu_common_clut_data;
extern const u16 g_world_menu_zoom_percentages[12]; /* twelve crop percentages, indexed by step */
extern s16* g_world_menu_entry_text_id_tables[];    /* 16-bit tables selected by entry select_text_table */
extern s32 g_world_menu_number_entry_digit_steps[];
/* Per-piece 4.12 fixed-point x/y scale; a negative factor flips that axis. */
extern menu_button_piece_t g_world_menu_button_pieces[];
extern point16_t g_world_menu_button_piece_scales[];
extern u16 g_world_menu_window_tpage;
extern u16 g_world_menu_palette_clut_normal;
extern u16 g_world_menu_palette_clut_alt;
extern u16 g_world_menu_alternate_number_clut_alt;
extern u16 g_world_menu_alternate_number_clut_normal;
extern u16 g_world_menu_disabled_number_clut_normal;
extern u16 g_world_menu_disabled_number_clut_alt;
extern u16 g_world_menu_hp_gauge_clut;
extern u16 g_world_menu_mp_gauge_clut;
extern u16 g_world_menu_ct_gauge_clut;
extern u16 g_world_menu_background_clut;
extern u16 g_world_menu_window_clut_alt;
extern u16 g_world_menu_window_clut_normal;
extern menu_sprite_command_t g_world_menu_script_sprite_command;
extern menu_window_command_t g_world_menu_window_interior_command;
extern s16 g_world_menu_window_scale_table[];
extern oriented_quad_t g_world_menu_number_blank_sprite;
extern s8 g_world_menu_cursor_bob_thresholds[];
extern s8 g_world_menu_cursor_bob_active_thresholds[];
extern s16 g_world_menu_list_row_text_ids[];
extern u16 g_world_menu_list_cursor_anim;
extern u16 g_world_menu_scroll_hold_counter;
extern s16 g_world_menu_text_redraw_request;
extern world_menu_grid_cell_t g_world_grid_menu_default_cells[];
extern world_menu_grid_cell_t g_world_grid_menu_shop_cells[];
extern world_menu_grid_cell_t g_world_grid_menu_shop_scrolled_cells[];
extern world_menu_grid_cell_t* g_world_active_grid_menu_cells;
extern u16 g_world_grid_menu_cursor_anim; /* grid cursor animation state */
extern u8 g_world_grid_menu_initialized;  /* grid menu initialized */
extern battle_at_entry_t g_world_action_target_list_slots[BATTLE_AT_LIST_CAPACITY];
extern s16 g_world_menu_submenu_page_row_text_ids[];
extern u32* g_world_menu_list_controller_input;
extern u16 g_world_menu_scroll_repeat_timer;
extern s16 g_world_menu_list_visible_rows;
extern s16 g_world_menu_list_row_record_count;
extern s16 g_world_menu_window_right_x;
extern s8 g_world_menu_list_layout_pending;
extern const s16* g_world_menu_list_entry_ids;
extern u8* g_world_menu_list_text_table;
extern u16 g_world_menu_list_image_width;
extern u8 g_world_menu_list_glyph_width;
extern u8 g_world_menu_list_text_columns;
extern s8 g_world_menu_list_scroll_direction;
extern RECT g_world_menu_window_rect;
extern u8 g_world_grid_menu_selected_cell; /* selected cell */
extern u16 g_world_menu_alternate_number_clut;
extern s8 g_world_menu_music_slot; /* formation music slot */
extern u16 g_world_menu_window_clut;
extern u16 g_world_menu_active_clut_front;
extern u16 g_world_menu_active_clut_back;
/* Result slot per menu (-1 while none); world_menu_reset_selection_results clears all 16. */
extern s16 g_world_menu_selection_results[16];
extern s32 g_world_grid_menu_id; /* menu_grid_help_id_e */
extern menu_callback_t g_world_menu_script_callbacks[16];
extern u16 g_world_menu_disabled_number_clut;
/*
 * WORLD.BIN routines shared by the world, wldcore, and menu-overlay
 * reconstructions.  Each module's linker table maps these names to its own
 * copy of the routine.
 */
menu_instruction_t* world_menu_parse_draw_window_frame(menu_instruction_t* instruction);
menu_instruction_t* world_menu_script_conditional(menu_instruction_t* instruction);
menu_instruction_t* world_menu_script_if_else(menu_instruction_t* instruction);
menu_instruction_t* world_menu_script_draw_tile(menu_instruction_t* instruction);
menu_instruction_t* world_menu_script_draw_sprite(menu_instruction_t* instruction);
menu_instruction_t* world_menu_script_draw_callback_sprite(menu_instruction_t* instruction);
menu_instruction_t* world_menu_script_draw_line(menu_instruction_t* instruction);
menu_instruction_t* world_menu_script_set_fixed_clut(menu_instruction_t* instruction);
menu_instruction_t* world_menu_script_set_clut(menu_instruction_t* instruction);
menu_instruction_t* world_menu_script_set_sprite_color(menu_instruction_t* instruction);
menu_instruction_t* world_menu_script_set_texture_page(menu_instruction_t* instruction);
menu_instruction_t* world_menu_script_set_semi_trans(menu_instruction_t* instruction);
menu_instruction_t* world_menu_script_set_draw_priority(menu_instruction_t* instruction);
menu_instruction_t* world_menu_parse_draw_tiled_rectangle(menu_instruction_t* instruction);
menu_instruction_t* world_menu_handle_window_command_with_scaled_clip(menu_instruction_t* instruction);
menu_instruction_t* world_menu_script_clear_temporary_draw_area(menu_instruction_t* instruction);
menu_instruction_t* world_menu_script_set_draw_area(menu_instruction_t* instruction);
menu_instruction_t* world_menu_script_add_blend_draw_mode(menu_instruction_t* instruction);
void world_menu_number_entry_thread(void);
void world_menu_labeled_number_entry_thread(void);
void world_menu_draw_pressable_button(s16 index, s32 x, s16 y, s16 pressed, s16 disabled, u16 sound_effect_id);
void world_screen_menu_draw_text_columns(menu_entry_t* entry, s32* row_offset, u8* buffer);
s32 world_menu_get_party_unit_name_id(void);
s32 world_menu_get_selected_unit_hp_bonus(s32 unused_row);
s32 world_menu_get_selected_unit_mp_bonus(s32 unused_row);
void world_menu_open_entry_window(s32 index, s32 unused_x, s32 unused_y);
void world_screen_menu_window_frame_thread(void);
void world_menu_run_order_unit(void);
s32 world_menu_run_unit_status_banner_countdown(s32 message_id, s32 frames);
void world_menu_add_gradient_line_primitive(point16_t* endpoints, rgb24_t* colors, s32 semi_trans, s32 ot_index);
void world_menu_announce_entry_value_thread(void);
void world_menu_resize_parent_entry_to_digits(void);
s32 world_menu_build_window_frame_image(s32 width, s32 height, u16* destination);
void world_menu_draw_row_window_and_cursor(s32 wide, s32 row, s32 mode);
void world_menu_request_unit_panel_redraw(void);
void world_screen_menu_build_window_image_row(const u16* source, u16* dest, s32 count, s32 offset);
s32 world_menu_get_event_state_flag(void);
s32 world_menu_get_learn_job_jp(s32 index);
s32 world_menu_has_items(s32 mode);
void world_menu_init_and_load_scrollable_list(
    const s16* values, s32 selected_index, u8* text_table, menu_instruction_t* script);
void world_option_gfx_init_scaled_draw_area_packets(scaled_draw_area_pair_t* pair);
void world_menu_load_scrollable_list_layout(menu_instruction_t* script);
void world_menu_load_text_1bd8(void);
void world_menu_load_text_1bd8_with_face(void);
void world_menu_load_text_1c18(void);
void world_menu_reset_selection_results(void);
void world_menu_run_ability(void);
s32 world_menu_run_learn_job_list(void);
void world_menu_run_main_mode(s32 mode);
void world_menu_set_brightness(s32 red, s32 green, s32 blue);
s16 world_menu_init_cursor_position(u8 index, s16 value);
void world_menu_set_default_entry_0_text_values(u32 text_index, u32 value);
void world_menu_set_default_entry_1_text_values(s32 value);
void world_menu_set_palette_mode(s32 mode);
void world_menu_set_sprite_color(const rgb24_t* src);
void world_menu_start_description_text_thread(s32 position);
void world_main_menu_text_window_thread(void);
void world_menu_toggle_preview_stats_window(s32 flag);
void world_menu_toggle_change_banner_panel_thread(s32 enable);
void world_menu_toggle_ability_panel_thread(s32 enable);
void world_menu_toggle_stat_preview_panel_thread(s32 enable);
void world_menu_start_equipment_and_ability_panel_thread(s32 enabled);
void world_menu_update_grid_cursor(s16 alternate);
void world_input_update_lockout_timer(void);
void world_menu_set_slot_4_cursor_index(s32 index);
extern s16 g_world_debug_variable_list_request_state;
extern s32 g_world_preview_stats_window_active;
/* shared with the per-program event interpreter and dialog thread */
u8* world_menu_redraw_text_page_on_scroll_in_rect(
    menu_entry_t* entry, s32* row_offset, s32* redraw, s32* unused, RECT* area);

#endif
