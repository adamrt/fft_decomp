#ifndef MENU_PANEL_H
#define MENU_PANEL_H

/* Menu library status panels: the unit status, equipment, ability and numeric editor panels, portraits and thread
 * indicators. */

#include "fft/data.h"
#include "fft/gfx.h"
#include "fft/map.h"
#include "fft/menu.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* ability callbacks */
extern s16 g_menu_ability_category;  /* menu_ability_category_e; BUNIT and JOBSTTS tab selection */
extern s16 g_menu_ability_entries[]; /* Packed ability IDs/flags; -1 terminates the list. */
extern s32 g_menu_ability_type;
extern ability_data_t* g_menu_selected_ability_data;
extern main_ability_secondary_pointer_t g_menu_selected_ability_secondary_data;
s32 menu_ability_is_action(s32 unused_row);
s32 menu_ability_is_action_tab(s32 unused_row);
s32 menu_ability_is_movement_tab(s32 unused_row);
s32 menu_ability_is_non_action_learned(s32 index);
s32 menu_ability_is_non_action_unlearned(s32 index);
s32 menu_ability_is_reaction_tab(s32 unused_row);
s32 menu_ability_is_support_tab(s32 unused_row);
s32 menu_ability_get_mp_cost_display_value(s32 index);
s32 menu_ability_get_ct_display_value(s32 index);
s32 menu_ability_get_jp_cost_display_value(s32 index);
s32 menu_ability_is_learned(s32 index);

/* job level callbacks */
extern s32 g_menu_job_current_level;
extern u32 g_menu_job_next_level_jp_requirement;
extern s16 g_menu_job_ids[];
s32 menu_job_calculate_next_level_jp_requirement(s32 index);
s32 menu_job_get_current_level(s32 unused_row);
s32 menu_job_get_next_level_jp_requirement(s32 unused_row);

/* portrait */
extern RECT g_menu_portrait_origin;
extern u8 g_menu_portrait_transition_offsets[];
extern RECT g_menu_portrait_pixel_rect;
extern RECT g_menu_portrait_palette_rect;
/* Unreferenced in both scenes: uploads one EVTFACE.BIN portrait and its palettes. */
void menu_load_portrait_image(s32 portrait_id);
void menu_build_portrait_poly_variant(s32 portrait_id, POLY_FT4* poly);

/* status panels */
extern menu_gradient_panel_prims_t g_menu_panel_fade_frames[];
void menu_init_panel_fade_tiles(void);
void menu_update_panel_fade(void);
void menu_init_primitive_colors_normal(menu_palette_primitives_t* menu);
void menu_init_primitive_colors_dimmed(menu_palette_primitives_t* menu);
void menu_init_numeric_display_frame_primitives(RECT* bounds, menu_palette_primitives_t* menu);
void menu_submit_numeric_display_frame_primitives(menu_palette_primitives_t* display);
void menu_init_sprite_array(SPRT* sprites, s32 count, s32 clut);
/* The selected panel unit can differ from the unit whose turn is active. */
extern menu_active_unit_t g_menu_selected_unit;
/* The unit shown beside the selected one in the comparison panels; REQUIRE
 * holds the same record. */
extern menu_active_unit_t g_menu_comparison_unit_data;
extern menu_status_panel_slot_storage_t g_menu_selected_unit_fields;
/* Target preview copies only generic_monster and the reaction ability here. */
extern menu_status_panel_slot_storage_t g_menu_comparison_unit_fields;
extern menu_unit_status_record_t g_menu_selected_unit_status;
extern menu_unit_status_record_t g_menu_comparison_unit_status;
extern RECT g_menu_hovered_unit_stats_rect;
extern RECT g_menu_attack_caster_stats_rect;
extern RECT g_menu_attack_target_stats_rect;
extern RECT g_menu_right_unit_stats_rect;
extern RECT g_menu_unit_summary_panel_rect;

/* status panel rendering */
extern RECT g_character_status_draw_area_rect;
extern RECT g_menu_status_panel_frame_rect[];
extern menu_status_panel_buffer_t g_panel_frames_a[];
extern RECT g_panel_origin_offsets[];
extern u8 g_panel_text_image_a[];
extern u8 g_panel_text_image_b[];
extern RECT g_panel_text_upload_rect_a;
extern RECT g_panel_text_upload_rect_b;
void menu_run_status_panel_thread(void);
void menu_run_equipment_ability_panel_thread(void);
void menu_run_numeric_editor_thread(void);
void menu_run_numeric_editor_frame_thread_variant(void);
void menu_load_selected_unit_variant(void);
void menu_refresh_hovered_unit_stats_display(void);
void menu_refresh_right_unit_stats_display(void);
void menu_run_unit_summary_panel_thread(void);
void menu_preview_attack_caster_stats(void);
void menu_preview_attack_target_stats(void);
void menu_copy_billboard_data_out(menu_unit_status_record_t* banner, menu_active_unit_t* unit,
    menu_status_panel_slot_storage_t* billboard, map_selected_tile_t* cursor_tile);
void menu_copy_billboard_data_in(const menu_unit_status_record_t* banner, const menu_active_unit_t* unit,
    const menu_status_panel_slot_storage_t* billboard);
void menu_clear_stat_preview(menu_unit_status_record_t* summary, menu_status_panel_slot_storage_t* detail);
void menu_calculate_swap_stat_delta(menu_status_panel_slot_storage_t* output, menu_unit_status_record_t* delta,
    s16 item_before, s16 item_after, s32 slot);
void menu_init_status_panel_primitives(menu_status_panel_buffer_t* menu);
void menu_select_status_primitive_color_palette(
    menu_palette_primitives_t* primitives, const menu_status_thread_t* thread);
void menu_set_status_panel_primitive_colors(menu_status_panel_buffer_t* primitives, const menu_status_thread_t* state);

/* thread status indicators */
extern s32 g_menu_indicator_brightness[2];
extern s32 g_menu_indicator_fade_work[2];
extern s32 g_menu_indicator_packet_index[2];
extern s32 g_menu_indicator_state[2];
extern menu_gradient_panel_prims_t g_menu_thread_indicator_packets[2][2];
void menu_draw_thread_status_indicators_variant(void);
s32 menu_thread_is_running_variant(s32 thread_id);

/* panel drawing */
extern const u16 g_menu_draw_area_scale_percent[12];
extern RECT g_menu_draw_area_template;
extern u8 g_menu_gfx_scaled_sprite_count;
extern point16_t g_menu_gfx_scaled_sprite_scale[];
extern point16_t g_menu_gfx_scaled_sprite_texture_page;
extern point16_t g_menu_texture_origin; /* VRAM origin for menu image placement */
extern sprite_sheet_frame_t g_menu_gfx_scaled_sprite_uv_rects[];
extern u8 g_menu_panel_status_animation[];
extern u8 g_menu_panel_status_group_bounds[];
extern u8 g_menu_panel_status_group_count;
extern u8 g_menu_panel_status_group_ids[];
extern sprite_draw_offset_t g_menu_panel_status_offsets[];
extern point16_t g_menu_panel_status_scales[];
extern const point16_t g_menu_panel_status_texture;
extern sprite_sheet_frame_t g_menu_panel_status_uv_rects[];
extern const u16 g_menu_panel_status_y_offsets[];
void menu_build_scaled_draw_area_packets(scaled_draw_area_pair_t* packet, const RECT* source, s32 scale_index,
    s32 lower_half, const menu_status_thread_t* thread);
void menu_build_status_group_primitives_variant(POLY_FT4* poly);
void menu_build_zodiac_primitives(zodiac_draw_context_t* context, POLY_FT4* poly);
void menu_init_scaled_draw_area_packets(scaled_draw_area_pair_t* data);

/* unit */
void menu_run_unit_editor_panel_thread(void);
extern menu_unit_view_thread_set_t g_menu_unit_view_thread_sets[];
extern s32 g_menu_unit_view_mode;
extern u16 g_menu_unit_view_cached_battle_id;
extern u16 g_menu_unit_view_cached_comparison_id;
void menu_start_unit_view_thread(s32 mode, s32 unit_a, s32 unit_b);

/* unit panel data */
extern menu_unit_number_images_t g_menu_unit_summary_digit_images;
extern u8 g_menu_billboard_job_name[];                /* job name text window */
extern u8 g_menu_billboard_unit_name[];               /* name text window */
extern u8 g_menu_billboard_secondary_skillset_name[]; /* secondary skillset name text window */
extern u8 g_menu_billboard_skillset_name[];           /* primary skillset name text window */
/* Entries 5..7 of g_menu_sprite_page_image_params; a separate alias preserves the original
 * template base. */
extern image_load_parameters_t g_menu_sprite_page_image_param_bases[];
extern menu_number_entry_t g_menu_attack_caster_stats_setup[8];
extern menu_number_entry_t g_menu_attack_target_stats_setup[8];
extern menu_number_entry_t g_menu_hovered_unit_stats_setup[10];
extern menu_number_entry_t g_menu_right_unit_stats_setup[10];
extern struct action_data* g_menu_preview_target_action;
extern menu_quad_page_t g_menu_height_display_pages[2];
extern RECT g_menu_tile_info_image_location;
extern image_load_parameters_t g_menu_tile_info_image_params[2];
extern menu_palette_slot_t g_menu_unit_summary_palettes[];
extern image_load_parameters_t g_menu_unit_summary_sprite_image_params[];

/* numeric display data */
/* Twelve world_menu_line_layout_t records, walked by byte offset. */
extern s8 g_menu_numeric_display_frame_offsets[96];
extern menu_panel_record_t g_menu_numeric_display_panel_records[2];
extern menu_number_entry_t g_menu_numeric_display_entries[15];
extern RECT g_menu_numeric_display_frame_rect;
extern image_load_parameters_t g_menu_numeric_display_image_params[18];
extern menu_palette_slot_t g_menu_numeric_display_palettes[];
extern RECT g_menu_numeric_display_text_rect_a;
extern RECT g_menu_numeric_display_text_rect_b;
extern RECT g_menu_numeric_display_text_rect_c;
extern RECT g_menu_numeric_display_panel_texture_window;
extern RECT g_menu_numeric_display_texture_window; /* texture window for the menu page */

/* unit editor panel data */
extern menu_unit_editor_frame_t g_menu_unit_editor_frames[2];
extern RECT g_menu_unit_editor_cursor_rect; /* cursor source */
extern image_load_parameters_t g_menu_unit_editor_item_icon_image_params[5];
extern u8 g_menu_unit_editor_line_colors[8 * MENU_UNIT_EDITOR_LINE_SIZE];
extern menu_palette_primitives_t g_menu_unit_editor_palette;
extern RECT g_menu_unit_editor_panel_rect; /* panel box */
extern image_load_parameters_t g_menu_unit_editor_row_image_params_a[4];
extern image_load_parameters_t g_menu_unit_editor_row_image_params_b[4];
extern u16 g_menu_unit_editor_sprite_clut_pairs[19][2];
extern image_load_parameters_t g_menu_unit_editor_sprite_image_params_a[19];
extern image_load_parameters_t g_menu_unit_editor_sprite_image_params_b[19];
extern RECT g_menu_unit_editor_text_rect_a; /* first text image */
extern RECT g_menu_unit_editor_text_rect_b; /* second text image */

/* unit status panel data */
extern u16 g_menu_unit_status_bounce_step;
extern u16 g_menu_unit_status_bounce_offsets[];
extern u8 g_menu_unit_status_first_page_by_mode[];
extern CVECTOR g_menu_unit_status_gauge_colors[];
extern image_load_parameters_t g_menu_unit_status_icon_image_params[];
extern menu_unit_status_page_t g_menu_unit_status_panel_pages[];
extern u16 g_menu_unit_status_quad_clut_pairs[][2];
extern image_load_parameters_t* g_menu_unit_status_quad_image_params_by_mode[];
extern u16 g_menu_unit_status_sprite_clut_pairs[][2];
extern image_load_parameters_t* g_menu_unit_status_sprite_image_params_by_mode[];
extern RECT g_menu_status_display_image_rects[];
extern menu_unit_status_record_t g_menu_selected_unit_stat_summary;
s32 menu_get_preview_hp_bonus_display(s32 unused_row);
s32 menu_get_preview_mp_bonus_display(s32 unused_row);

/* other */
extern s32 g_character_status_redraw_request;
extern menu_number_entry_t g_editor_numeric_descriptor_a[];
extern menu_number_entry_t g_editor_numeric_descriptor_b[];
extern RECT g_editor_numeric_entries_a;
extern RECT g_editor_numeric_entries_b;
extern RECT g_editor_numeric_entries_c;
extern RECT g_editor_numeric_entries_d;
extern RECT g_editor_numeric_entries_e;
extern RECT g_editor_numeric_entries_f;
extern menu_status_panel_numeric_buffer_t g_editor_numeric_state[2];
extern menu_status_panel_numeric_buffer_t g_editor_numeric_state_a[2];
extern menu_status_panel_numeric_buffer_t g_editor_numeric_state_b[2];
extern battle_menu_status_panel_numeric_geometry_t g_editor_numeric_table;
extern menu_unit_stat_number_images_t g_editor_numeric_text_a;
extern menu_unit_stat_number_images_t g_editor_numeric_text_b;
extern struct unit_stats g_editor_party_unit_stats;
extern u8 g_editor_primary_skillset_name[];
extern u8 g_editor_secondary_skillset_name[];
extern s32 g_numeric_editor_redraw_request;
extern menu_unit_status_record_t g_panel_comparison_billboard;
extern menu_status_panel_editor_packet_t g_panel_comparison_editor_packets[2];
extern u8 g_panel_comparison_large_number_image[];
extern u8 g_panel_comparison_name_image[];
extern u8 g_panel_comparison_number_image[];
extern menu_number_entry_t g_panel_comparison_numeric_entries[];
extern menu_status_panel_packet_t g_panel_comparison_packets[2];
extern RECT g_panel_comparison_portrait_rect;
extern u16 g_panel_editor_label_cluts[][2];
extern u16 g_panel_editor_value_cluts[][2];
extern RECT g_panel_frame_rect;
extern CVECTOR g_panel_gauge_bar_colors[];
extern menu_unit_status_record_t g_panel_selected_billboard;
extern menu_status_panel_editor_packet_t g_panel_selected_editor_packets[2];
extern u8 g_panel_selected_large_number_image[];
extern u8 g_panel_selected_name_image[];
extern u8 g_panel_selected_number_image[];
extern menu_number_entry_t g_panel_selected_numeric_entries[];
extern menu_status_panel_packet_t g_panel_selected_packets[2];
extern RECT g_panel_selected_portrait_rect;
extern s16 g_panel_slide_down_y[];
extern s16 g_panel_slide_up_y[];
extern u16 g_portrait_index_by_sprite_set[];
extern RECT g_portrait_sheet_rect_0;
extern RECT g_portrait_sheet_rect_1;
extern RECT g_portrait_sheet_rect_2;
extern RECT g_portrait_sheet_rect_3;
extern s8 g_remove_all_equipment_prompt_active;
extern s8 g_remove_item_previewed_slot;
extern RECT g_special_portrait_palette_rect;
extern RECT g_special_portrait_rect;
extern u16 g_remove_all_equipment_ids[5]; /* weapon, shield, head, body, accessory */
extern s16 g_remove_item_equipment[5];

#endif
