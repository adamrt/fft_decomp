#ifndef MENU_INPUT_H
#define MENU_INPUT_H

/* Menu library input: controller state, selections, cursors and blocked buttons. */

#include "fft/gfx.h"
#include "fft/menu.h"
#include "psx/types.h"

/* modal state */
/* Nonzero while a modal selection, panel or companion overlay owns the input. */
extern u16 g_menu_modal_open;
void menu_set_modal_open(void);
void menu_clear_modal_open(void);
/* Refused-command thread: queue the invalid sound and exit. */
void menu_run_invalid_choice_thread(void);

/* menu selection */
void menu_set_selection_record(s32 index, s32 selected_index, s32 scroll_base_index, s16* entries);
extern menu_unit_selection_record_t g_menu_unit_selection_records[];
void menu_reset_unit_record(s32 record_index);

/* selection state */
extern u8 g_menu_restore_pending;
extern s16 g_menu_pending_selection[4];
/* The record count differs per module (14 in BUNIT). */
extern menu_selection_record_t g_menu_selection_records[];
extern u8 g_menu_selected_slot;
void menu_mini_menu_cancel_delay_thread(void);
void menu_confirm_sound_delay_thread(void);
void menu_check_action_restrictions(void);
void menu_check_action_slot_restrictions(s32 menu, s32 value, s32 check_unit);
void menu_reset_unit_records(void);
void menu_reset_unit_records_wrapper(void);
void menu_restore_selection_for_selected_unit(s32 menu_id);
void menu_run_skillset_thread(void);
void menu_select_unit_action_slots_thread(void);
void menu_clear_selection_record(s32 index);
void menu_clear_selection_records(void);
void menu_get_selection_record(s32 index, s16* selected_index, s16* scroll_base_index, s16* entries);
void world_menu_get_selection_record(s32 index, s16* selected_index, s16* scroll_base_index, s16* entries);
void menu_store_sort_key_list(s32 list_index, const s16* source);
void menu_load_sort_key_list(s32 list_index, s16* destination);
void menu_clear_selection_values(void);
s32 menu_set_selection_value(u8 index, s16 value);
s32 menu_init_selection_value(u8 index, s16 value);

/* horizontal selection */
/* Stands for "no option" or "no item" in a resolved menu selection. */
enum { MENU_SELECTION_NONE = 0xFFFE };

extern s16 g_menu_selection_values[];
s16 menu_update_wrapped_horizontal_selection(u16 entry_count, u8 selection_index, s32 input_mask);
s32 menu_update_wrapped_grid_selection(s32 columns, s32 rows, s32 last_index, s32 old_index, s32 input);
extern menu_command_map_t g_menu_command_maps[31];
extern s32 g_menu_active_menu; /* active menu */
extern s16 g_menu_result;      /* menu result: -4 pending, -1 canceled */
extern s16 g_menu_selected_option;
extern s32 g_menu_used_skillset_id;
extern s32 g_menu_used_item_id;
extern u16 g_menu_anything_ability_id;
extern s32 g_menu_dead_unit_action;
extern s32 g_menu_dead_unit_result;
s32 menu_resolve_selection(void);
s32 menu_update_horizontal_selection_and_mark_change(
    u16 entry_count, u8 selection_index, s32 input_mask, u8 changed_state);
s16 menu_update_wrapped_vertical_selection(u16 entry_count, u8 selection_index, s32 input_mask);
s32 menu_update_vertical_selection_and_mark_change(s32 entry_count, s32 selection_index, s32 input_mask, s8 sound_id);

/* input */
extern menu_icon_source_t g_menu_item_type_icon_source;
void menu_build_item_icon_source(menu_icon_source_t* output, u32 item_id);
menu_icon_source_t* menu_build_item_type_icon_source(s32 index);
void menu_draw_weapon_hand_icons(s16 weapon_id);
extern s32 g_menu_hide_numeric_values;   /* non-zero: draw digits as blanks (0x48, 0x10) */
extern glyph_blit_t g_menu_number_glyph; /* source rectangle in the number/sign font */
extern glyph_blit_t g_menu_stacked_glyph_lower;
extern glyph_blit_t g_menu_stacked_glyph_upper;
void menu_draw_number_glyphs(s32 value, s32 digits, u8* pixels, glyph_blit_t* pos);
void menu_draw_stacked_glyph_pair(u8* pixels, glyph_blit_t* position);
void menu_draw_numeric_display_entries(u8* buffer, menu_number_entry_t* entries, glyph_blit_t* position, s32 count);
void menu_display_text_string(u8* image, glyph_blit_t* origin, u8* text);
void menu_display_text_entry_past_choice_marks(s32 text_id, u8* image, glyph_blit_t* origin);
void menu_display_text_entry(s32 text_id, u8* image, glyph_blit_t* origin);
void menu_display_number_entry(s32 value, u8 digit_count, u8* image, glyph_blit_t* origin);
extern menu_text_state_t g_menu_text_state;
/* Added to glyph palette indices while menu text is drawn. */
extern s32 g_menu_text_palette_offset;
extern menu_input_timing_profile_t g_menu_option_input_repeat_timings[];
extern u16 g_menu_input_block_frames;
void menu_set_text_origin(s32 x, s32 y);
void menu_input_update_event_state(u32* otag, u32 input, s32 count_frame);
void menu_input_clear_state(void);
void world_menu_input_update_dialog_state(void);
extern u8 g_input_activation_timer;
extern u32* g_input_controller;
extern u32 g_input_current;
extern u32 g_input_current_buttons;
extern u32 g_input_event;
extern u32 g_input_initial_repeat_counter;
extern u8 g_input_lock_timer;
extern u8 g_input_page_scroll_disabled;
extern u16 g_input_page_scroll_hold_frames;
extern u32 g_input_polled_buttons;
extern u32 g_input_repeat_counter;
extern s32 g_input_repeat_counters[16];
extern u32 g_input_secondary_repeat_counter;
extern s32 g_menu_blocked_buttons;
extern u16 g_menu_button_clut;
extern u8 g_menu_button_piece_starts[];
extern s8 g_menu_button_press_timers[];
extern u32 g_previous_input;
extern u32 g_input_previous_buttons;

/* cursor animation */
extern s8 g_menu_cursor_forward_offset_keyframes[];
extern s8 g_menu_cursor_reverse_offset_keyframes[];
extern u16 g_menu_cursor_texture_page;
void menu_update_and_draw_animated_cursor(const point16_t* anchor, u16* state, s32 mode);
void menu_update_and_draw_sliding_cursor(point16_t* pos, u16* state, s32 mode);
void world_menu_update_and_draw_sliding_cursor(point16_t* pos, u16* state, s32 mode);

/* input and palette state */
extern u16 g_menu_active_clut_back;
extern u16 g_menu_active_clut_front;
extern u16 g_menu_alternate_number_clut;
extern u16 g_menu_cursor_mode0_background_clut;
extern u16 g_menu_cursor_mode0_foreground_clut;
extern u16 g_menu_cursor_mode1_background_clut;
extern u16 g_menu_cursor_mode1_foreground_clut;
extern u16 g_menu_disabled_number_clut;
extern u8 g_menu_input_activation_timer;
extern u16 g_menu_input_latched_button;
extern u16 g_menu_input_newly_pressed;
extern u16 g_menu_input_previous;
extern u32 g_menu_input_primary_repeat;
extern s32 g_menu_input_repeat_counters[16];
extern u32 g_menu_input_secondary_repeat;
extern u16 g_menu_palette_clut;
extern u16 g_menu_text_clut_0_mode0;
extern u16 g_menu_text_clut_0_mode1;
extern u16 g_menu_text_clut_1_mode0;
extern u16 g_menu_text_clut_1_mode1;
extern u16 g_menu_text_clut_2_mode0;
extern u16 g_menu_text_clut_2_mode1;
extern u16 g_menu_text_clut_3_mode0;
extern u16 g_menu_text_clut_3_mode1;
extern u16 g_menu_window_clut;
void menu_input_update_controller(void);
void menu_set_palette_mode(s32 mode);

/* command palette state */
extern u16 g_menu_gfx_clut_id;
extern u16 g_menu_gfx_texture_page;
extern u8 g_menu_use_alternate_palette;

#endif
