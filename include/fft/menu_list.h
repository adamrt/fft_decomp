#ifndef MENU_LIST_H
#define MENU_LIST_H

/* Menu library lists: menu entries, scrollable lists, list threads and the AT list. */

#include "fft/menu.h"
#include "psx/types.h"

/* menu entries */
/* Each scene keeps its own current table; event screens save and restore it. */
extern menu_entry_t* g_menu_thread_menu_data;
extern s32 g_menu_managed_thread_state;
extern s16 g_menu_wait_row_actions[4];
extern u16 g_menu_at_list_selection_finished;
extern s32 g_menu_help_open;
extern s32 g_menu_building_thread_started;
s32 menu_start_managed_thread(s32 thread_id, menu_entry_t* descriptor);
void menu_request_thread_stop(s32 thread_id);
void menu_stop_managed_thread(s32 thread_id);
void menu_init_stub(void);
void menu_wait_thread(void);
void menu_input_thread(void);
void menu_build_main(void);
void menu_start_help_menu_thread(void);
/* BATTLE and WORLD each link their own bodies of the next four. */
void menu_handle_action(menu_entry_t* entry, s32 row_index);
void menu_handle_cancel_input(menu_entry_t* context);
void menu_step_wrapping_cursor_on_scroll_buttons(menu_entry_t* bounds, s32* cursor);
void menu_record_selection_for_selected_unit(void);
void menu_clear_entry_selections(void);
void menu_finish_at_list_selection(void);
s32 menu_get_entry_item_count(s32 row);
s32 menu_get_entry_item_equipped_count(s32 row);
void menu_draw_item_status_list(s16 item_id);

/* list threads */
extern u32* g_menu_input_controller;
extern s16 g_menu_confirm_stored_unit_row_actions;
extern s16 g_menu_set_auto_battle_row_actions;
extern menu_status_thread_t g_menu_comparison_display_thread_params;
extern menu_status_thread_t g_menu_status_display_thread_params;
extern menu_status_thread_t g_menu_numeric_panel_params;
extern menu_status_thread_t g_menu_preview_stats_thread_params;
extern menu_entry_t g_menu_at_list_state;
void menu_draw_text_columns(menu_entry_t* entry, s32* row_offset, u8* buffer);
void menu_icon_linked_entry_thread(void);
void menu_confirm_move_command_thread(void);
void menu_confirm_auto_battle_selection_thread(void);
void menu_icon_list_thread(void);
void menu_icon_strip_thread(void);
void menu_icon_timed_thread(void);
void menu_text_entry_wait_thread(void);
void menu_stop_thread_and_wait(s32 thread_id);
void menu_start_or_poll_thread(s32 thread_id, menu_entry_t* descriptor);
void menu_toggle_comparison_status_thread(s32 enable);
void menu_toggle_unit_status_thread(s32 enable);
void menu_toggle_numeric_editor_thread(s32 enable);
void menu_thin_window_frame_thread(void);
void menu_window_frame_thread(void);

/* menu state */
extern menu_scroll_text_layout_t g_menu_at_list_layout;

/* scrollable lists */
extern s16 g_menu_list_row_group_count;
extern s16 g_menu_list_row_height;
extern s16 g_menu_list_visible_rows;
extern u8 g_menu_list_glyph_width;
extern u8 g_menu_list_text_columns;
extern u16 g_menu_list_vram_width;
extern s16 g_menu_list_entry_count;
extern u8* g_menu_list_text_table;
extern s16* g_menu_list_entries;
extern s8 g_menu_list_scroll_direction; /* read with lb */
extern s8 g_menu_list_redraw_pending;   /* read with lb (menu_cmd_draw_scrollable_list_body_handler_variant) */
extern s16 g_menu_list_selected_index;
extern s16 g_menu_scroll_base_index;
void menu_init_scrollable_list(const s16* entries, s32 selected_index, s32 scroll_base_index, u8* text_table);
extern u16 g_menu_list_scroll_progress;

/* list */
extern u8 g_menu_at_list_text[];
extern menu_text_layout_t g_menu_at_list_entry_table[1];
extern menu_list_page_t g_at_list_pages[2];
extern s16 g_menu_at_list_indices[];
extern s16 g_menu_at_list_primary_values[];
extern s16 g_menu_at_list_secondary_values[];
extern s16 g_menu_at_list_flags[];
void menu_preview_ability_turn_at_list(void);

#endif
