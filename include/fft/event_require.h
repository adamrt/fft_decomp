#ifndef FFT_EVENT_REQUIRE_H
#define FFT_EVENT_REQUIRE_H

/* EVENT/REQUIRE.OUT: menu screen overlay that runs with BATTLE. */

#include "fft/battle.h"

/* character */
extern battle_menu_status_panel_offset_pair_t g_require_character_status_frame_rect[];
extern RECT g_require_character_status_draw_area_rect;
extern s32 g_require_character_status_frame_config;
extern s32 g_require_character_status_redraw_request;

/* editor */
extern s16 g_require_editor_brave;
extern s16 g_require_editor_faith;
extern u8 g_require_editor_first_ability_bytes[];
extern u8 g_require_editor_fourth_ability_bytes[];
extern u8 g_require_editor_numeric_descriptor_a[];
extern u8 g_require_editor_numeric_descriptor_b[];
extern RECT g_require_editor_numeric_entries_a;
extern RECT g_require_editor_numeric_entries_b;
extern RECT g_require_editor_numeric_entries_c;
extern RECT g_require_editor_numeric_entries_d;
extern RECT g_require_editor_numeric_entries_e;
extern RECT g_require_editor_numeric_entries_f;
extern battle_image_location_t g_require_editor_numeric_geometry[];
extern battle_menu_status_panel_numeric_buffer_t g_require_editor_numeric_state[2];
extern battle_menu_status_panel_numeric_geometry_t g_require_editor_numeric_table;
extern u8 g_require_editor_numeric_text_a[];
extern u8 g_require_editor_numeric_text_b[];
extern u8 g_require_editor_numeric_texture[];
extern u8 g_require_editor_second_ability_bytes[];
extern s16 g_require_editor_selected_unit_id;
extern u8 g_require_editor_third_ability_bytes[];
extern battle_menu_status_panel_slot_storage_t g_require_editor_unit_fields;
extern s16 g_require_editor_unit_type;
extern s16 g_require_editor_zodiac;
extern s16 g_require_editor_team_state;
extern struct battle_stats g_require_editor_party_unit_stats;
void require_editor_load_selected_unit(void);
void require_editor_run_numeric_thread(void);

/* gfx */
/* Record types hoisted out of the defining .c files so that callers and
 * definitions agree on one spelling. */
/* Texture-page position pair; the y halfword is masked with 0xf00 by GetTPage. */
typedef struct require_texture_page {
    s16 x;
    u16 y;
} require_texture_page_t;

typedef struct require_gfx_texture_page_position {
    s16 x;
    u16 y;
} require_gfx_texture_page_position_t;

typedef struct require_gfx_point {
    s16 x;
    s16 y;
} require_gfx_point_t;

typedef struct require_gfx_sprite_rect {
    u8 u; /* 0x00 */
    u8 _unused_01;
    u8 v;           /* 0x02 */
    u8 _padding_03; /* aligns width */
    s16 width;      /* 0x04 */
    s16 height;     /* 0x06 */
    s16 offset_x;   /* 0x08 */
    s16 offset_y;   /* 0x0a */
} require_gfx_sprite_rect_t;

/* One display-condition sprite: texture rectangle and screen offset, shared by
 * the general- and special-case display-condition renderers. */
typedef struct require_display_condition_quad {
    s16 u;      /* 0x00 */
    s16 v;      /* 0x02 */
    s16 width;  /* 0x04 */
    s16 height; /* 0x06 */
    s16 x;      /* 0x08 */
    s16 y;      /* 0x0a */
} require_display_condition_quad_t;

extern const u16 g_require_gfx_draw_area_scale_percent[12];
extern RECT g_require_gfx_draw_area_template;
extern s16 g_require_gfx_draw_offset_y;
extern u8 g_require_gfx_fade_rgb[3];
extern u8 g_require_gfx_formation_sprite_groups[][3];
extern u8 g_require_gfx_poly_ft4_banks[];
extern s16 g_require_gfx_portrait_origin[];
extern u8 g_require_gfx_portrait_transition_offsets[];
extern u8 g_require_gfx_scaled_sprite_count;
extern require_gfx_point_t g_require_gfx_scaled_sprite_scale[];
extern require_gfx_sprite_rect_t g_require_gfx_scaled_sprite_uv_rects[];
extern require_texture_page_t g_require_gfx_scaled_sprite_texture_page;
extern u8 g_require_portrait_cell_pixels[];
extern RECT g_require_portrait_cell_rect;
extern u16 g_require_portrait_index_by_sprite_set[];
extern RECT g_require_portrait_palette_rect;
extern RECT g_require_portrait_sheet_rect_0;
extern RECT g_require_portrait_sheet_rect_1;
extern RECT g_require_portrait_sheet_rect_2;
extern RECT g_require_portrait_sheet_rect_3;
extern POLY_GT4 g_require_gfx_poly_gt4_array_32[];
extern POLY_GT4 g_require_gfx_poly_gt4_array_8[];
extern POLY_GT4 g_require_gfx_poly_gt4_banks[2][40];

/* find (BATTLE) */
void require_gfx_apply_menu_palette_for_mode(
    world_menu_palette_primitives_t* output, const battle_menu_status_panel_frame_config_t* context);

void require_gfx_build_gradient_grid_primitives(POLY_GT4* poly);
void require_gfx_build_portrait_poly_ft4(s32 flags, void* output);

void require_gfx_build_scaled_draw_area_packets(battle_menu_status_panel_portrait_primitive_tail_t* packet,
    const RECT* source, s32 scale_index, s32 lower_half, const s16* offset);

void require_gfx_build_status_group_primitives(POLY_FT4* poly);
void require_gfx_clip_portrait_poly_from_left(POLY_FT4* poly, s32 amount);
void require_gfx_clip_portrait_poly_from_right(POLY_FT4* poly, s32 amount);
void require_gfx_fade_rgb_31_frames(s32 target_0, s32 target_1, s32 target_2);
void require_gfx_init_menu_tile_and_line_primitives(battle_menu_status_panel_buffer_t* menu);
void require_gfx_init_scaled_draw_area_packets(battle_menu_status_panel_portrait_primitive_tail_t* packet);
void require_gfx_set_clut_rect_from_id(RECT* rect, s32 packed);

void require_gfx_set_scaled_poly_ft4_geometry(POLY_FT4* poly, require_gfx_texture_page_position_t* texture_page,
    require_gfx_point_t* position, require_gfx_sprite_rect_t* sprite_rect, require_gfx_point_t* scale, POLY_FT4* base);

void require_gfx_set_scaled_poly_ft4_geometry_and_uv(POLY_FT4* poly, const require_texture_page_t* texture,
    const RECT* position, const require_gfx_sprite_rect_t* uv_rect, const require_gfx_point_t* scale,
    const s16* offset);

void require_gfx_set_formation_icon_uv(POLY_FT4* poly, s32 index);
void require_render_display_condition_general_cases(s32 value);
void require_render_display_condition_special_cases_thread(void);

void require_gfx_build_portrait_transition_primitives(const RECT* texture_rect, s32* transition,
    const s32* first_portrait, const s32* second_portrait, u8* image, POLY_FT4* poly, s32 direction);

void require_gfx_init_poly_gt4_array_32(s32 x, s32 y);
void require_gfx_init_poly_gt4_array_8(s32 x, s32 y);
void require_gfx_run_suspended_thread_transition(void);
void require_gfx_scale_poly_gt4_vertex_colors(const u8* vertex_colors, s32 scale, POLY_GT4* poly);

/* condition */
extern u8 g_require_condition_general_quad_ranges[];
extern require_display_condition_quad_t g_require_condition_general_quads[];
extern u8 g_require_condition_general_vertex_colors[];

/* Per-condition first-quad indices; entry n+1 bounds entry n. */
extern u8 g_require_condition_special_quad_ranges[];
extern require_display_condition_quad_t g_require_condition_special_quads[];

/* 0x14-byte vertex-colour records, one per quad. */
extern u8 g_require_condition_special_vertex_colors[];
void require_condition_show_ready_bugged(void);

/* REQUIRE 0x801cafd4, invoked by both BATTLE and WORLD event dispatch. */
void require_condition_dispatch(void);

/* menu */
extern s32 g_require_menu_indicator_brightness[2];
extern s32 g_require_menu_indicator_fade_work[2];
extern s32 g_require_menu_indicator_packet_index[2];
extern s32 g_require_menu_indicator_state[2];
extern s16 g_require_menu_selection_result;
void require_menu_run_simple_selection_thread(void);
void require_menu_wait_selection_threads(void);

/* panel */
extern u8 g_require_panel_active_unit_banner[];
extern u8 g_require_panel_billboard_data[];
extern battle_menu_status_panel_gauges_t g_require_panel_comparison_billboard;
extern s16 g_require_panel_comparison_unit_id;
extern s32 g_require_panel_dim_a;
extern s32 g_require_panel_dim_b;
extern s32 g_require_panel_dim_c;
extern battle_menu_status_panel_gauges_t g_require_panel_selected_billboard;
extern u8 g_require_panel_status_animation[];
extern u8 g_require_panel_status_group_bounds[];
extern u8 g_require_panel_status_group_count;
extern u8 g_require_panel_status_group_ids[];
extern const s16 g_require_panel_status_offsets[][6];
extern u16 g_require_panel_status_position_y;
extern const s16 g_require_panel_status_scales[][2];
extern const s16 g_require_panel_status_uv_rects[][6];
extern const u16 g_require_panel_status_y_offsets[];
extern battle_image_location_t g_require_panel_item_icon_texture[];
extern battle_image_location_t g_require_panel_origin_offsets[];
extern RECT g_require_panel_text_upload_rect_a;
extern RECT g_require_panel_text_upload_rect_b;
extern world_gfx_image_load_parameters_t g_require_panel_label_layouts_mode0[];
extern world_gfx_image_load_parameters_t g_require_panel_label_layouts_mode1[];
extern world_gfx_image_load_parameters_t g_require_panel_label_layouts_mode2[];
extern world_gfx_image_load_parameters_t g_require_panel_label_layouts_mode3[];
extern world_gfx_image_load_parameters_t g_require_panel_item_icon_layouts[];
extern battle_menu_status_panel_buffer_t g_require_panel_frames_a[];
extern u8 g_require_panel_text_image_a[];
extern u8 g_require_panel_text_image_b[];
extern battle_menu_status_panel_editor_packet_t g_require_panel_comparison_editor_packets[2];
extern u8 g_require_panel_comparison_large_number_image[];
extern u8 g_require_panel_comparison_name_image[];
extern u8 g_require_panel_comparison_number_image[];
extern battle_menu_status_panel_numeric_entry_t g_require_panel_comparison_numeric_entries[];
extern battle_menu_status_panel_packet_t g_require_panel_comparison_packets[2];
extern u8 g_require_panel_comparison_portrait_image[];
extern RECT g_require_panel_comparison_portrait_rect;
extern s16 g_require_panel_comparison_unit_data[];
extern u8 g_require_panel_editor_label_cells[];
extern u16 g_require_panel_editor_label_cluts[];
extern u8 g_require_panel_editor_mode_cell[];
extern u8 g_require_panel_editor_mode_cells[];
extern u8 g_require_panel_editor_value_cells[];
extern u16 g_require_panel_editor_value_cluts[];
extern u8 g_require_panel_frame_rect[];
extern CVECTOR g_require_panel_gauge_bar_colors[];
extern u8 g_require_panel_portrait_cell[];
extern battle_menu_status_panel_editor_packet_t g_require_panel_selected_editor_packets[2];
extern u8 g_require_panel_selected_large_number_image[];
extern u8 g_require_panel_selected_name_image[];
extern u8 g_require_panel_selected_number_image[];
extern battle_menu_status_panel_numeric_entry_t g_require_panel_selected_numeric_entries[];
extern battle_menu_status_panel_packet_t g_require_panel_selected_packets[2];
extern u8 g_require_panel_selected_portrait_image[];
extern RECT g_require_panel_selected_portrait_rect;
extern u8 g_require_panel_sprite_cells[];
extern const require_texture_page_t g_require_panel_status_texture;
void require_panel_copy_battle_stats_to_gauges(struct battle_stats* unit, battle_menu_status_panel_gauges_t* output);

void require_panel_set_primitive_colors(
    battle_menu_status_panel_buffer_t* primitives, const battle_menu_status_panel_frame_config_t* state);

void require_panel_run_character_status_thread(void);
void require_panel_set_transition_value(s32 value);

/* reward */
extern RECT g_require_reward_bonus_image_rect;
extern RECT g_require_reward_bonus_palette_rect;
extern s32 g_require_reward_war_trophy_entry_count;
extern s32 g_require_reward_war_trophy_entry_types[];
extern s32 g_require_reward_war_trophy_entry_values[];
extern u8 g_require_reward_war_trophy_item_count;
extern battle_war_result_t g_require_reward_war_trophy_search_result;
extern DR_MODE g_require_reward_burst_draw_modes[2];
extern POLY_F4 g_require_reward_burst_flash_polys[2];
extern MATRIX g_require_reward_burst_matrix;
extern VECTOR g_require_reward_burst_offset;
extern VECTOR g_require_reward_burst_rotated_offset;
extern SVECTOR g_require_reward_burst_rotation;
extern RECT g_require_reward_money_clip_rect_0;
extern RECT g_require_reward_money_clip_rect_1;
extern POLY_FT4 g_require_reward_money_comma_polys[2];
extern u32 g_require_reward_money_digit_initial_speeds[6];
extern POLY_FT4 g_require_reward_money_digit_polys[2][6];
extern u32 g_require_reward_money_digit_positions[6];
extern s32 g_require_reward_money_digit_settling[6];
extern u32 g_require_reward_money_digit_speeds[6];
extern s32 g_require_reward_money_digit_targets[6];
extern POLY_FT4 g_require_reward_money_next_digit_polys[2][6];
extern POLY_FT4 g_require_reward_money_sign_polys[2];
extern RECT g_require_reward_screen_clip_rect_0;
extern RECT g_require_reward_screen_clip_rect_1;
extern RECT g_require_reward_war_trophy_clear_rect;

/* Upload rectangle for the current entry's text image. */
extern RECT g_require_reward_war_trophy_image_rect;
void require_reward_collect_war_trophies_and_bonus_money(void);
s32 require_reward_get_war_trophy_column_x(s32 value, s32 index);
s32 require_reward_get_war_trophy_entry_offset(s32 value, s32 index);
void require_reward_load_bonus_image(s32 image_id);
void require_reward_animate_bonus_image_burst(void);

/* text */
typedef struct require_glyph {
    s16 source_x;
    s16 source_y;
    s16 width;
    s16 height;
    s16 source_stride;
} require_glyph_t;

/* Section offsets of the REQUIRE text file, relative to g_require_text_data (0x80 bytes before it). */
extern s32 g_require_text_section_offsets[32];
extern u8 g_require_text_data[];
extern s16 g_require_message_menu_result;
extern s32 g_require_numeric_editor_redraw_request;
extern s32 g_require_numeric_editor_thread_params;
extern require_glyph_t g_require_text_decimal_glyph;
void require_text_clear_string_buffer(u8* data);

void require_text_render_decimal_entry_list(s32 pixels, battle_menu_status_panel_gauge_entry_t* entries,
    battle_menu_status_panel_text_position_t* output, s32 count);

void require_text_render_decimal_value(
    s32 value, s32 flags, void* pixels, battle_menu_status_panel_text_position_t* position);

void require_text_render_signed_decimal_entries(s32 pixels, battle_menu_status_panel_gauge_entry_t* entries,
    battle_menu_status_panel_text_position_t* output, s32 count);

void require_text_show_battle_congratulations(void);
void require_text_build_battle_nicknames(void);

/* thread */
extern battle_menu_status_panel_indicator_prims_t g_require_thread_indicator_packets[2][2];
extern s32 g_require_thread_suspended_id;
void require_thread_exit_current(void);
s32 require_thread_is_running(s32 thread_id);

/* unit */
extern u8 g_require_active_unit_data[];
extern s32 g_require_party_affected_battle_unit_indices[];
extern s32 g_require_party_affected_unit_count;
extern s32 g_require_party_affected_unit_message_args[];
extern s32 g_require_party_affected_unit_message_ids[];

/* x and y of the palette MoveImage destination (a RECT at 0x801d0d44). Read as two scalar symbols: a RECT view lets
   GCC keep the struct address live in a saved register and changes the allocation. */
extern s16 g_require_party_palette_destination;
extern s16 g_require_party_palette_destination_y;
extern RECT g_require_party_palette_move_rect;
extern s16 g_require_party_portrait_destination;
extern RECT g_require_party_portrait_move_rect;
extern u8 g_require_party_removable_flags[];

/* Six consecutive 0x3c-byte menu records; the target derives entry [4]'s
   address from entry [2], so they are one array. */
extern world_menu_entry_t g_require_party_removal_menus[6];
extern s16 g_require_party_removal_result;
void require_party_apply_permanent_brave_faith_changes(void);
s32 require_party_find_low_brave_high_faith_departures(void);
void require_party_remove_low_brave_high_faith_units(void);
s32 require_party_sell_equipment_over_limit(s32 unit_id);
s32 require_party_store_equipment_or_sell_overflow(s32 unit_id);
void require_party_update_bio_variable_for_unit_class(s32 unit_class, s32 value);
void require_load_party_unit_into_editor(s32 unused, s32 party_index);
s32 require_party_find_join_candidates(void);
s32 require_party_get_free_slot_status(s32 unit_id);
s32 require_party_restore_item_counts(s32 party_index);
void require_prepare_party_portrait_textures(void);
void require_render_unit_status_panel_thread(void);
s32 require_select_party_unit_to_remove(void);

/* other */
extern char g_jobstts_text_wait_for_allocation_message[];
extern u32* g_require_input_controller;
extern u8 g_require_work[];
extern job_data_t* g_require_formation_job_data;
extern u8 g_require_join_menu_descriptor[];
extern u8 g_require_saved_thread_block[];
extern RECT g_require_special_portrait_palette_rect;
extern RECT g_require_special_portrait_rect;
extern s32 g_require_status_display_redraw_request;
extern s32 g_require_status_display_thread_params;
s32 require_apply_permanent_brave_faith_changes_and_collect_warnings(void);
void require_apply_post_battle_unit_changes(void);
void require_input_wait_frames_or_skip(s32 frames);
void require_noop_801c43e0(void);
void require_overlay_open_jobstts(void);
void require_render_gil_reward_ticker(void);
void require_render_war_trophies(void);
void require_sound_wait_music_idle(void);
void require_overlay_run_helpmenu(void);

#endif
