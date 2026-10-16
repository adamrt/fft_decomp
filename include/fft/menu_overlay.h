#ifndef MENU_OVERLAY_H
#define MENU_OVERLAY_H

/* Menu library screens and overlays: companion overlays, screen starters, help menus, options and the dead unit menu.
 */

#include "fft/gfx.h"
#include "fft/map.h"
#include "fft/menu.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* scene lifecycle */
/* 0: normal, 1: companion/editor transition, 2: AT-list selection. */
extern u16 g_menu_overlay_state;
void menu_init_palette_and_text(s32 unused_0);
void menu_init_subsystem_pointers(void);
s32 menu_init_attack_resources_and_threads(void);
void menu_reset_subsystems(void);
s32 menu_poll_small_load(void);

/* companion overlays */
void menu_run_companion_overlay_stub(void);
extern u8* g_menu_companion_data_file_buffer;
extern s32 g_menu_companion_data_file_loading;
extern s32 g_menu_companion_data_file_lbas[];
extern s32 g_menu_companion_data_file_sizes[];
extern s32 g_menu_companion_executable_loading;
extern s32 g_menu_companion_executable_lbas[];
extern s32 g_menu_companion_executable_sizes[];
extern s32 g_menu_companion_executable_offsets[];
void menu_open_companion_data_file(void);
void menu_open_companion_executable(void);
extern s32 g_menu_companion_overlay_state;
extern char g_menu_wait_for_file_allocation_message[];
extern s32 g_menu_companion_executable_request_index;
extern s32 g_menu_companion_data_file_request_index;
void menu_open_option_at_list(void);
void menu_open_option_dead_unit_panel(void);
void menu_open_option_game_options(void);
void menu_request_open_companion_executable(s32 request_index);
void menu_request_open_small(void);

/* menu screen starters */
void menu_open_debugchr(void);
void menu_open_debugchr_unit_editor(void);
void menu_open_bunit(void);
void menu_start_mini_menu_display_thread(void);
void menu_run_triangle_selection(void);
void menu_run_equip_thread(void);
void menu_run_jobstts_thread(void);
void menu_start_at_list_thread(void);

/* help menus */
extern menu_unit_status_record_t g_menu_help_active_banner;
extern menu_active_unit_t g_menu_help_active_unit_data;
extern menu_status_panel_slot_storage_t g_menu_help_billboard;
extern map_selected_tile_t g_menu_help_menu_cursor_tile;
extern u8 g_menu_help_data_base[];
extern help_navigation_record_t g_menu_help_require_navigation[];
extern u32* g_menu_help_controller_state_ptr;
extern u16* const g_menu_help_menu_text_id_tables[23];
extern u16* g_menu_help_topic_text_ids;
extern help_navigation_record_t g_menu_help_terrain_navigation[];
extern help_navigation_record_t g_menu_help_unit_box_navigation[];
extern help_navigation_record_t g_menu_help_attack_preview_navigation[];
void menu_run_battle_help(void);
void menu_run_require_help(void);
extern s32 g_menu_help_message_text_id;
extern u32 g_help_text_table[];

/* per-module bodies: each module links its own copy under the shared name. */
s32 menu_get_thread_activity(void);
void menu_run_ability_list_thread(void);
void menu_run_equipment_list_thread(void);
s32 bunit_menu_ability_update_list_menu(void);
s32 jobstts_menu_ability_update_list_menu(void);
menu_vertical_scroll_direction_e bunit_menu_input_read_page_scroll_direction(void);
menu_vertical_scroll_direction_e world_menu_input_read_page_scroll_direction(void);
s32 bunit_menu_run_job_list(void);
s32 jobstts_menu_run_job_list(void);
void equip_menu_input_update_with_message_state(void);
void jobstts_menu_input_update_with_message_state(void);
void world_menu_input_update_with_message_state(void);
s32 menu_update_equipment(void);
void menu_run_at_list(void);
void bunit_menu_update_and_draw_bouncing_cursor(point16_t* pos, u16* state, s32 mode);
void world_menu_update_and_draw_bouncing_cursor(point16_t* pos, u16* state, s32 mode);
void option_menu_run_game_options(void);
void world_menu_run_game_options(void);
/* Scrollable list core: BUNIT, EQUIP, JOBSTTS and WORLD each link their own body. */
void menu_init_scrollable_list_core(const s16* entries, s32 selected_index, u8* text_table);

/* overlay */
extern menu_entry_t g_menu_option_icon_thread_param;
extern s32 g_menu_option_choice_widths[];
extern s32 g_menu_option_choice_counts[];
extern s32 g_menu_option_values[GAME_OPTION_UNCHANGED + 1];
extern s32 g_menu_option_value_text_id_bases[];
extern menu_icon_record_t g_menu_option_picker_icon_records[2];
extern s32 g_menu_option_reference_values[];
extern options_menu_page_t g_menu_option_pages[2];
extern menu_text_layout_t g_menu_option_entry_table[1];
extern RECT g_menu_option_title_image_rect;
extern RECT g_menu_option_entry_panel_rect;
extern RECT g_menu_option_value_image_rect;
void menu_run_option_choice(void);
extern u32* g_dead_unit_controller_input;
extern menu_entry_t g_dead_unit_menu[DEAD_UNIT_MENU_ENTRY_COUNT];
extern menu_text_layout_t g_dead_unit_menu_entries;
extern s16 g_dead_unit_menu_flags[];

#endif
