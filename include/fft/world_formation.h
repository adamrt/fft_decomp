#ifndef WORLD_FORMATION_H
#define WORLD_FORMATION_H

/* WORLD's formation screen: the unit grid, job wheel, ability and equipment menus and roster changes. */

#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/menu_script.h"
#include "fft/unit.h"
#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* ability */
extern s16 g_world_ability_category; /* menu_ability_category_e */
extern RECT g_world_ability_panel_origin;
extern RECT g_world_ability_panel_draw_area;
extern menu_status_thread_t g_world_ability_panel_thread_params;
extern s8 g_world_ability_menu_initialized;
extern s8 g_world_remove_ability_initialized;
extern s8 g_world_set_ability_initialized;
extern s8 g_world_ability_view_list_initialized;
extern u8 g_world_ability_menu_slots_changed;
extern menu_entry_t g_world_ability_menu;
extern u8 g_world_ability_submenu_running;
extern u8 g_world_ability_menu_unit_index;
extern s8 g_world_remove_ability_unit_index;
extern u16 g_world_remove_ability_cursor_anim;
extern point16_t g_world_remove_ability_cursor_point;
extern point16_t g_world_clear_abilities_cursor_point;
extern menu_instruction_t g_world_set_ability_window_script;
extern s8 g_world_set_ability_slot;               /* ability slot under the cursor: 0..4 */
extern s8 g_world_set_ability_list_close_pending; /* close the ability list this frame */
extern s8 g_world_set_ability_show_stat_preview;  /* show the stat preview window */
extern u16 g_world_set_ability_cursor_anim;
extern point16_t g_world_set_ability_cursor_point;
extern menu_instruction_t g_world_ability_list_script;
extern menu_list_page_t g_world_ability_list_pages[][2];
extern u8 g_world_ability_menu_saved_browse_enabled;
extern s8 g_world_remove_ability_cursor;
extern s8 g_world_clear_abilities_prompt_active;
extern s16 g_world_remove_ability_slots[5];
extern volatile u16 g_world_clear_abilities_blink_counter;
extern s8 g_world_set_ability_list_open;   /* ability list open */
extern s8 g_world_set_ability_entry_count; /* ability list entry count */
extern u8 g_world_set_ability_unit_index;
extern u8 g_world_set_ability_cursor;
void world_ability_find_stat_changes_due_to_equipped(
    menu_status_panel_slot_storage_t* stat_diff, s32 old_ability, s32 new_ability);
s32 world_ability_has_secret_hunt(s32 unit_id);
s32 world_ability_has_two_hands(s16 unit_id);
s32 world_ability_has_two_swords(s16 unit_id);
void world_ability_learn(s16 unit_id, s16 job_id, s16 ability_id);
s32 world_ability_build_equippable_rsm_list(s16 unit_index, s32 slot, s16* out);
s32 world_ability_run_learn_list(void);

/* equipment */
extern RECT g_world_equipment_panel_origin;
extern RECT g_world_equipment_ability_panel_origin;
extern RECT g_world_equipment_panel_draw_area;
extern RECT g_world_equipment_ability_panel_draw_area;
extern RECT g_world_equipment_panel_item_text_rect;
extern RECT g_world_equipment_panel_ability_text_rect;
extern image_load_parameters_t g_world_equipment_panel_sprite_params_0[19];
extern image_load_parameters_t g_world_equipment_panel_sprite_params_1[19];
extern image_load_parameters_t g_world_equipment_panel_sprite_params_2[19];
extern image_load_parameters_t g_world_equipment_panel_sprite_params_3[19];
extern image_load_parameters_t g_world_equipment_panel_icon_params[WORLD_EQUIPMENT_SLOT_COUNT];
extern menu_status_thread_t g_world_equipment_ability_panel_thread_params;
extern s8 g_world_equip_menu_initialized;
extern u16 g_world_equip_item_cursor_anim;
extern point16_t g_world_equip_item_cursor_point;
extern point16_t g_world_remove_all_equipment_cursor_point;
extern volatile u16 g_world_remove_all_equipment_blink_counter;

/* job */
extern s32 g_world_job_cached_total_jp;
extern s16 g_world_job_wheel_job_count;
extern s16 g_world_job_wheel_name_text;
extern s8 g_world_learn_menu_initialized;
extern s8 g_world_job_wheel_initialized;
extern s8 g_world_learn_job_list_initialized;
extern s8 g_world_learn_ability_list_open;
extern s8 g_world_view_abilities_initialized;
extern s16 g_world_job_wheel_unit_index;
/* The current and the new job's portrait; separate objects, since
 * world_formation_run_job_wheel_menu addresses each with its own lui/addiu. */
extern textured_rect_source_t g_world_job_wheel_portrait_quad;
extern textured_rect_source_t g_world_job_wheel_new_portrait_quad;
extern menu_instruction_t g_world_job_wheel_script;
extern u8 g_world_job_wheel_job_selected;
extern point16_t g_world_job_wheel_unit_sprite_position;
extern u8 g_world_job_wheel_center_x;
extern u8 g_world_job_wheel_center_y;
extern u8 g_world_job_wheel_animation_counter;
extern RECT g_world_job_change_old_sprite_rect;
extern RECT g_world_job_change_new_sprite_rect;
extern u8 g_world_job_change_active;
extern s16 g_world_job_change_frame;
extern rgb24_t g_world_job_change_wave_colors[3];
extern u8 g_world_job_wheel_close_frame;
extern u8 g_world_job_wheel_open_frame;
extern u8 g_world_job_wheel_closing;
extern u8 g_world_job_wheel_opening;
extern oriented_quad_t g_world_job_wheel_mastered_icon_sprite;
extern menu_instruction_t g_world_learn_job_list_script;
extern menu_entry_t g_world_learn_confirm_menu[2];
/* The prompt chained after g_world_learn_confirm_menu. */
extern menu_entry_t g_world_learn_confirm_submenu;
extern s16 g_world_learn_job_name_list[];
extern s16 g_world_view_job_name_list[];
extern s8 g_world_job_wheel_spin_progress;
extern s8 g_world_job_wheel_spin_direction;
extern s8 g_world_job_change_anim_state; /* world_job_change_anim_e */
extern u8 g_world_job_change_confirmed;
extern u8 g_world_job_wheel_saved_browse_enabled;
extern rgb24_t g_world_job_change_flash_colors[4];
extern s16* g_world_job_change_dissolve_order;
extern u8* g_world_job_change_old_sprite_image;
extern u8* g_world_job_change_new_sprite_image;
extern u8* g_world_job_change_new_sprite_source;
extern s16 g_world_job_change_skip_requested;
extern s16 g_world_job_wheel_cursor_index;
extern s16 g_world_job_wheel_jobs[];
extern s16 g_world_job_wheel_mastered_jobs[];
extern RECT g_world_job_wheel_name_rect;
extern s8 g_world_learn_menu_step; /* menu_job_list_step_e */
/* Nonzero while the learn-confirmation menu thread (thread 6) is running. */
extern u8 g_world_learn_confirm_prompt_state;
extern s8 g_world_view_abilities_step; /* menu_job_list_step_e */
extern u8 g_world_view_abilities_confirm_prompt_state;
extern s16 g_world_learn_selected_job;
extern s16 g_world_learn_selected_job_index;
s32 world_job_build_unit_list(u8 unit_index, s16* jobs, s32 mode);
s32 world_job_get_cached_level(s32 unused_row);
s32 world_job_get_cached_level_jp_requirement(s32 unused_row);
s32 world_job_get_cached_skillset_empty_flag(s32 unused_row);
s32 world_job_get_cached_total_jp(s32 unused_row);
s32 world_job_get_selected_unit_job_points_entry(s32 unused_row);
s32 world_job_is_locked_for_all_units(s16 job_id);

/* formation */
extern oriented_quad_t g_world_formation_cursor_shadow_sprite;
extern world_formation_background_tile_t g_world_formation_background_tiles[2][4];
extern oriented_quad_t g_world_formation_cursor_sprite;
extern s32 g_world_formation_current_menu; /* world_formation_menu_e */
extern point16_t g_world_formation_cursor_position;
extern u8 g_world_formation_generic_human_graphic_index_table[];
extern texture_source_t g_world_formation_graphic_entries[];
extern menu_input_state_t g_world_formation_menu_input_state;
/* Alias for the first 32-bit field retained for the target's separate absolute store. */
extern s32 g_world_formation_menu_new_buttons;
extern u32* g_world_formation_ordering_table;
/* g_main_system_flags as saved by world_formation_save_and_set_system_flags (main_system_flags_e). */
extern u32 g_world_formation_saved_system_flags;
/* Set while the selected unit is in the upper rows of the grid: the unit banner
 * then sits at the bottom of the screen and the stat bar at the top (BUNIT's
 * g_bunit_status_banner_at_bottom). The latch is the value the banner was last
 * placed for. Majority spelling: five readers sign-extend as u8. Two files
 * (world_formation_init_selection_cursor, world_formation_step_grid_transition)
 * need s8 to match and keep their own local extern. */
extern u8 g_world_formation_unit_banner_at_bottom;
extern u8 g_world_formation_unit_banner_at_bottom_latch;
extern s16 g_world_formation_selected_unit_index_latch;
extern struct world_unit* g_world_formation_temp_unit;
extern u8 g_world_formation_unit_banner_enabled;
extern u8 g_world_formation_unit_browse_enabled;
extern void (*g_world_formation_menu_handlers[])(void);
extern s16 g_world_formation_open_request_state;   /* world_screen_request_state_e */
extern u32 g_world_formation_menu_allowed_buttons; /* pad mask applied to the new presses of each menu frame */
extern s32 g_world_formation_name_string_revision;
extern zodiac_draw_context_t g_world_formation_view;
/* Portrait code per sprite set: low byte is the portrait index. */
extern u16 g_world_formation_sprite_by_portrait[];
extern u8 g_world_formation_menu_text_table[];
extern clut_t g_world_formation_menu_palettes;
extern rgb24_t g_world_formation_saved_sprite_color;
extern rgb24_t g_world_formation_dimmed_sprite_color;
extern rgb24_t g_world_formation_normal_sprite_color;
extern u8 g_world_formation_unit_cycle_mode;
extern u8 g_world_formation_cursor_ot_override;
extern s8 g_world_formation_main_menu_initialized;
extern u8 g_world_formation_saved_banner_enabled;
extern u8 g_world_formation_screen_running;
extern u32 g_world_formation_status_label_image[];
extern RECT g_world_formation_status_label_image_rect;
extern menu_entry_t g_world_formation_egg_triangle_menu;
/* Chained after the egg triangle menu; world_formation_process_frame only resets it. */
extern menu_entry_t g_world_formation_egg_submenu;
extern u8 g_world_formation_unit_slide_pending;
extern s8 g_world_formation_scroll_position;
extern s8 g_world_formation_scroll_velocity;
extern s8 g_world_formation_selection_cursor_ready;
extern u8 g_world_formation_triangle_menu_open;
extern u8 g_world_formation_triangle_menu_running;
extern volatile u8 g_world_formation_unit_slide_frame;
extern s8 g_world_formation_banner_slide_step;
extern u8 g_world_formation_stat_display_mode; /* menu_unit_readout_e, HP to BRAVE_FAITH */
extern menu_draw_number_command_t g_world_formation_gauge_value_number;
extern menu_draw_number_command_t g_world_formation_level_number;
extern menu_draw_number_command_t g_world_formation_experience_number;
extern menu_draw_number_command_t g_world_formation_brave_number;
extern menu_draw_number_command_t g_world_formation_faith_number;
extern menu_draw_number_command_t g_world_formation_brave_number_wide;
extern menu_draw_number_command_t g_world_formation_faith_number_wide;
extern menu_draw_number_command_t g_world_formation_gauge_max_number;
extern menu_instruction_t g_world_formation_stat_bar_window_top;
extern menu_instruction_t g_world_formation_stat_bar_window_bottom;
/* Three consecutive sprite records at 0x8018c704 / 0x8018c718 / 0x8018c72c. */
extern oriented_quad_t g_world_formation_shadow_egg_sprites[3];
extern s8 g_world_formation_gauge_bar_offsets[4];
extern rgb24_t g_world_formation_hp_gauge_colors[4];
extern rgb24_t g_world_formation_mp_gauge_colors[4];
extern rgb24_t g_world_formation_ct_gauge_colors[4];
extern oriented_quad_t g_world_formation_label_cant_equip;
extern oriented_quad_t g_world_formation_label_equipping;
extern oriented_quad_t g_world_formation_label_leaving;
extern oriented_quad_t g_world_formation_unit_dot_sprite;
extern oriented_quad_t g_world_formation_label_missing;
extern menu_sprite_cell_t g_world_formation_label_tiles[];
extern menu_sprite_cell_t g_world_formation_label_tiles_bottom[];
extern s8 g_world_formation_dot_glow_timer;
extern s8 g_world_formation_dot_glow_direction;
extern u8 g_world_formation_cursor_trail_brightness[8];
extern u8 g_world_formation_grid_transition_frame;
extern u8 g_world_formation_grid_return_frame;
extern u8 g_world_formation_grid_return_done;
extern u8 g_world_formation_portrait_slide_frame;
extern rgb24_t g_world_formation_away_unit_color;
extern menu_entry_t g_world_formation_triangle_menu;
extern menu_entry_t g_world_formation_dismiss_prompt;
extern menu_entry_t g_world_formation_dismiss_confirm_menu[4];
extern menu_entry_t g_world_formation_dismiss_confirm_menu_scrolled[4];
extern world_formation_dismiss_entry_t g_world_formation_dismiss_entries[];
extern s16 g_world_formation_dismiss_prompt_text_id;
extern s16 g_world_formation_dismiss_prompt_kind;
extern s16 g_world_formation_dismiss_anim_timer;
extern s8 g_world_formation_sort_field_indices[];
extern u8 g_world_formation_thread_status_snapshot[16];
extern POLY_FT4 g_world_formation_view_quads[2][30];
extern point16_t g_world_formation_cursor_target;
/* Cursor trail position history at 0x801c8344, reset by
 * world_formation_reset_slots_and_frame_tpages; entry 7 is the newest and x is
 * -1 in an unused entry. */
extern point16_t g_world_formation_cursor_trail[8];
extern u8 g_world_formation_dismiss_step;
extern menu_entry_t* g_world_formation_dismiss_menu_data;
extern s16 g_world_formation_record_order[];
extern struct world_unit g_world_unit_records[WORLD_FORMATION_UNIT_RECORD_COUNT];
extern u8 g_world_formation_menu_text_buffer[];
/* Menu callers cache the builder's return here; rebuilding the record list
 * does not itself refresh this byte. */
extern u8 g_world_formation_unit_count;
extern world_unit_t* g_world_unit_pointers[];
void world_formation_run_screen(s32 tutorial, s32 unused);
s32 world_formation_is_unit_list_scroll_needed(void);
s16 world_formation_build_record_list(s32 item_id, world_unit_t** unused_list, s32 mode);
void world_formation_build_unit_graphic_entry(s16 formation_index, texture_source_t* destination, s32 unused);
void world_formation_build_unit_record(
    const struct unit_stats* unit, struct world_unit* record, const struct party_unit* party);
s32 world_formation_can_dismiss_unit(s16 formation_idx);
s32 world_formation_can_equip_item_in_slot(s16 unit_index, s16 slot, s16 item_id);
s32 world_formation_change_unit_job(s16 unit_id, s16 job_id, s32 update_equipment);
void world_formation_copy_unit_to_temp(s16 index, s32 unused_slot);
void world_formation_cycle_selected_unit(s16 pad_buttons);
void world_formation_dismiss_unit(s16 formation_idx);
void world_formation_draw_background_tiles(s32 y, s32 unused_1);
void world_formation_draw_sprite_with_shadow(const oriented_quad_t* pos, const rgb24_t* rgb, s32 ot_index);
void world_formation_draw_unit_sprite(s16 unit_index, point16_t* pos, rgb24_t* rgb, s32 ot_index);
s32 world_formation_equip_item_to_unit_slot(s16 unit, s16 slot, s16 item_id);
s32 world_formation_find_index_by_roster_slot(s16 roster_slot);
s32 world_formation_generate_unit(s16 unit_type);
void world_formation_get_generic_human_graphic_entry(s32 job_id, s16 is_female, texture_source_t* destination);
void world_formation_init_cursor_and_scroll_state(s32 formation_index);
void world_formation_init_menu_display(void);
void world_formation_init_menu_state(void);
void world_formation_init_selection_cursor(s16 formation_index);
void world_formation_item_menu(void);
s32 world_formation_get_unit_lock(s32 unit_id);
void world_formation_place_unit_portrait_at_rest(s16 unit_index);
void world_formation_rebuild_unit_list(void);
void world_formation_recalculate_selected_unit_stats(void);
void world_formation_remove_invalid_unit_loadout(s16 unit_id);
void world_formation_reset_menu_context(void);
void world_formation_restore_menu_after_core_exit(void);
void world_formation_run_dismiss_unit_menu_step(void);
void world_formation_run_job_wheel_menu(void);
s16 world_formation_run_menu_frame(u32* otag, u32 input);
void world_formation_run_screen_for_menu_index(s32 menu_index);
void world_formation_save_records_to_party(void);
void world_formation_set_display_mode(s32 mode);
void world_formation_set_menu_allowed_buttons(u32 mask);
void world_formation_set_ordering_table_pointer(u32* ordering_table);
s32 world_formation_set_unit_ability_slot(s16 unit_id, s16 slot, s16 ability, s32 update_equipment);
void world_formation_set_unit_name(s16 formation_index, const u8* name);
void world_formation_stage_selected_unit(void);
s32 world_formation_step_grid_return_transition(s32 mode);
s32 world_formation_step_grid_transition(s32 mode);
u8 world_formation_step_unit_portrait_slide(void);
void world_formation_stop_menu_threads(void);
/* get_mode and get_shade take the unit slot index; NULL selects the default. */
s32 world_formation_update_and_draw_unit_grid(s16 background_y, s16 toggle_banner, s16 allow_browse, s16 input,
    s16 locked, s32 (*get_mode)(s32), s32 (*get_shade)(s32));
void world_formation_update_unit_selection(void);
void world_formation_view_zoom_thread(void);
void world_formation_sort_roster(s32 index);

/* unit */
extern const world_unit_monster_breed_entry_t g_world_unit_monster_breed_table[WORLD_MONSTER_BREED_ENTRY_COUNT];
extern CVECTOR g_world_unit_panel_bar_colors[];
extern image_load_parameters_t g_world_unit_summary_label_sprite_params[];
extern image_load_parameters_t g_world_unit_summary_value_sprite_params[];
extern RECT g_world_unit_status_panel_origin;
extern image_load_parameters_t g_world_unit_status_sprite_params[];
extern image_load_parameters_t g_world_unit_status_portrait_params;
extern u16 g_world_unit_summary_value_cluts[4][2]; /* [row][dimmed] */
extern u16 g_world_unit_summary_label_cluts[7][2]; /* [row][dimmed] */
extern image_load_parameters_t g_world_unit_summary_mode_row_params[4];
extern u8 g_world_unit_status_banner_active;
extern menu_entry_t g_world_unit_status_banner_menu;
extern s8 g_world_unit_status_banner_state; /* banner state: 0 idle, 1 finishing, else countdown */
s32 world_get_party_unit_formation_sprite(s32 party_index);
void world_unit_build_best_fit_equipment(s16 unit_index, s16* equipment);
s32 world_unit_can_equip_item_id(s16 unit_index, s16 item_id);
s32 world_formation_get_unit_palette_index(s32 roster_slot);
void world_unit_update_monster_breeding(s32 unused);

/* item */
extern s8 g_world_remove_item_unit_index;
extern s16 g_world_remove_item_saved_primary_input;
extern s16 g_world_remove_item_saved_secondary_input;
extern u16 g_world_remove_item_saved_newly_pressed;
extern u16 g_world_remove_item_cursor_anim;
extern point16_t g_world_remove_item_cursor_point;

/* other */
extern s8 g_world_dismiss_unit_prompt_running;
extern menu_entry_t g_world_best_fit_confirm_menu;
extern menu_entry_t g_world_order_unit_menu;
extern u8 g_world_best_fit_slot_categories[WORLD_EQUIPMENT_SLOT_COUNT];
extern u8 g_world_order_unit_saved_browse_enabled;
s32 world_unit_get_facing_sixteenth_by_runtime_id(s32 runtime_id);

/* unnamed */
extern s16 g_world_formation_record_count; /* 0x801c8634 */

#endif
