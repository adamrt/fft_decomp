#ifndef FFT_DEBUGCHR_H
#define FFT_DEBUGCHR_H

#include "fft/battle.h"
#include "fft/battle_menu_status_panel.h"
#include "fft/geometry.h"
#include "fft/main_zodiac.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Record types hoisted out of the defining .c files so that callers and
 * definitions agree on one spelling. */

/* Texture-page position pair; the y halfword is masked with 0xf00 by GetTPage. */
typedef struct debug_chr_texture_page {
    s16 x;
    u16 y;
} debug_chr_texture_page_t;

/* Inclusive value range for one editable field row. */
typedef struct debug_range {
    s32 lo; /* 0x00 */
    s32 hi; /* 0x04 */
} debug_range_t;

extern s32 g_debugchr_editor_active;
extern s16 g_debugchr_editor_display_mode;
extern u8 g_debugchr_editor_first_ability_bytes[];
extern s16 g_debugchr_editor_first_maximum;
extern u8 g_debugchr_editor_fourth_ability_bytes[];
extern u8 g_debugchr_editor_jump;
extern u8 g_debugchr_editor_move;
extern u8 g_debugchr_editor_numeric_descriptor_a[];
extern u8 g_debugchr_editor_numeric_descriptor_b[];
extern u8 g_debugchr_editor_numeric_entries_a[];
extern u8 g_debugchr_editor_numeric_entries_b[];
extern u8 g_debugchr_editor_numeric_entries_c[];
extern u8 g_debugchr_editor_numeric_entries_d[];
extern u8 g_debugchr_editor_numeric_entries_e[];
extern u8 g_debugchr_editor_numeric_entries_f[];
extern battle_image_location_t g_debugchr_editor_numeric_geometry[];
extern battle_menu_status_panel_numeric_buffer_t g_debugchr_editor_numeric_state_a[2];
extern battle_menu_status_panel_numeric_buffer_t g_debugchr_editor_numeric_state_b[2];
extern u8 g_debugchr_editor_numeric_table[];
extern u8 g_debugchr_editor_numeric_text_a[];
extern u8 g_debugchr_editor_numeric_text_b[];
extern u8 g_debugchr_editor_numeric_texture[];
extern u8 g_debugchr_editor_second_ability_bytes[];
extern s16 g_debugchr_editor_second_maximum;
extern s16 g_debugchr_editor_selected_unit_id;
extern s32 g_debugchr_editor_selected_unit_slot;
extern u8 g_debugchr_editor_speed;
/* 0x801c5e34; per-row value ranges, copied from the _normal or _special table. */
extern debug_range_t g_debugchr_editor_template[21];
extern u8 g_debugchr_editor_template_normal[];
extern u8 g_debugchr_editor_template_special[];
extern u8 g_debugchr_editor_third_ability_bytes[];
extern s16 g_debugchr_editor_third_maximum;
extern s16 g_debugchr_editor_unit_fields[];
extern u8 g_debugchr_editor_unit_ids[];
extern s16 g_debugchr_editor_unit_type;
/* Views of g_debugchr_panel_selected_billboard.bars[i].value (values[3 * i])
 * and bars[i].limit (the *_maximum words): debugchr_editor_apply_values_to_battle_unit
 * reaches the values from their own base register and the limits by absolute
 * address, which the member spelling does not reproduce. */
extern s16 g_debugchr_editor_values[];
extern s16 g_debugchr_editor_zodiac;
extern const u16 g_debugchr_gfx_draw_area_scale_percent[12];
extern RECT g_debugchr_gfx_draw_area_template;
extern u16 g_debugchr_gfx_draw_offset_y;
extern u32* g_debugchr_input_controller;
extern s16 g_debugchr_gfx_portrait_origin[];
extern u8 g_debugchr_gfx_portrait_transition_offsets[];
extern u8 g_debugchr_gfx_render_buffers[2][0x4B0];
extern u8 g_debugchr_gfx_scaled_sprite_count;
extern point16_t g_debugchr_gfx_scaled_sprite_scale[];
extern main_zodiac_sprite_frame_t g_debugchr_gfx_scaled_sprite_uv_rects[];
extern s32 g_debugchr_menu_indicator_brightness[2];
extern s32 g_debugchr_menu_indicator_fade_work[2];
extern s32 g_debugchr_menu_indicator_packet_index[2];
extern s32 g_debugchr_menu_indicator_state[2];
extern battle_menu_status_panel_editor_state_t g_debugchr_panel_comparison_billboard;
extern s16 g_debugchr_panel_comparison_unit_id;
extern s32 g_debugchr_panel_refresh_a;
extern s32 g_debugchr_panel_refresh_b;
extern s32 g_debugchr_panel_refresh_c;
extern battle_menu_status_panel_editor_state_t g_debugchr_panel_selected_billboard;
extern u8 g_debugchr_panel_status_animation[];
extern u8 g_debugchr_panel_status_group_bounds[];
extern u8 g_debugchr_panel_status_group_count;
extern u8 g_debugchr_panel_status_group_ids[];
extern const s16 g_debugchr_panel_status_offsets[][6];
extern u16 g_debugchr_panel_status_position_y;
extern const s16 g_debugchr_panel_status_scales[][2];
extern const s16 g_debugchr_panel_status_uv_rects[][6];
extern const u16 g_debugchr_panel_status_y_offsets[];
extern battle_menu_status_panel_indicator_prims_t g_debugchr_thread_indicator_packets[2][2];
extern s32 g_debugchr_thread_refresh_a;
extern s32 g_debugchr_thread_refresh_b;
extern s32 g_debugchr_thread_refresh_c;

/* Unnamed data, in address order. */
/* Selected-unit panel halfwords at 0x801c5324 ([0] unit id, [1] job, [2]
 * brave, [3] faith, [4] portrait cell, [5] zodiac and editor unit slot); the
 * comparison panel's twin follows at 0x801c5334. */
extern s16 g_debugchr_panel_selected_unit_data[];
extern s16 g_debugchr_panel_comparison_unit_data[];
extern s16 g_debugchr_panel_slide_up_y[];
extern battle_image_location_t g_debugchr_panel_item_icon_texture[];
extern s16 g_debugchr_panel_slide_down_y[];
extern u8 g_debugchr_panel_editor_label_cells[];
extern u8 g_debugchr_panel_editor_mode_cell[];
extern u8 g_debugchr_panel_editor_value_cells[];
extern u8 g_debugchr_panel_frame_rect[];
extern u8 g_debugchr_panel_sprite_cells[];
extern u8 g_debugchr_panel_portrait_cell[];
extern u16 g_debugchr_panel_editor_value_cluts[];
extern u16 g_debugchr_panel_editor_label_cluts[];
extern u8 g_debugchr_panel_editor_mode_cells[];
extern u8 g_debugchr_panel_selected_portrait_rect[];
extern u8 g_debugchr_panel_comparison_portrait_rect[];
extern battle_image_location_t g_debugchr_panel_origin_offsets[];
extern battle_menu_status_panel_offset_pair_t g_debugchr_character_status_frame_rect[];
extern u8 g_debugchr_character_status_draw_area_rect[];
extern u8 g_debugchr_panel_text_upload_rect_a[];
extern u8 g_debugchr_panel_text_upload_rect_b[];
extern world_gfx_image_load_parameters_t g_debugchr_panel_label_layouts_mode0[];
extern world_gfx_image_load_parameters_t g_debugchr_panel_label_layouts_mode1[];
extern world_gfx_image_load_parameters_t g_debugchr_panel_label_layouts_mode2[];
extern world_gfx_image_load_parameters_t g_debugchr_panel_label_layouts_mode3[];
extern world_gfx_image_load_parameters_t g_debugchr_panel_item_icon_layouts[];
extern u8 g_debugchr_panel_selected_editor_packets[];
extern u8 g_debugchr_panel_selected_packets[];
extern u8 g_debugchr_panel_comparison_editor_packets[];
extern u8 g_debugchr_panel_comparison_packets[];
extern u8 g_debugchr_panel_selected_number_image[];
extern u8 g_debugchr_panel_selected_name_image[];
extern u8 g_debugchr_panel_selected_large_number_image[];
extern u8 g_debugchr_panel_comparison_number_image[];
extern u8 g_debugchr_panel_comparison_name_image[];
extern u8 g_debugchr_panel_comparison_large_number_image[];
extern u8 g_debugchr_panel_selected_portrait_image[];
extern u8 g_debugchr_panel_comparison_portrait_image[];
extern battle_menu_status_panel_buffer_t g_debugchr_panel_frames_a[];
extern u8 g_debugchr_panel_text_image_a[];
extern u8 g_debugchr_panel_text_image_b[];

/* editor */
void debugchr_editor_load_selected_unit(void);
void debugchr_render_unit_status_panel_thread(void);
void debugchr_editor_run_numeric_thread(void);
void debugchr_editor_run_unit_thread(s32 unit_id);

/* gfx */
void debugchr_gfx_build_portrait_poly_ft4(s32 portrait_id, POLY_FT4* poly);
void debugchr_gfx_build_scaled_draw_area_packets(battle_menu_status_panel_portrait_primitive_tail_t* packet,
    const void* source, s32 scale_index, s32 lower_half, const s16* offset);
void debugchr_gfx_build_status_group_primitives(POLY_FT4* poly);
void debugchr_gfx_init_menu_tile_and_line_primitives(battle_menu_status_panel_menu_primitives_t* menu);
void debugchr_gfx_init_scaled_draw_area_packets(battle_menu_status_panel_portrait_primitive_tail_t* packet);
void debugchr_gfx_set_scaled_poly_ft4_geometry(POLY_FT4* poly, const debug_chr_texture_page_t* texture,
    const s16* position, const s16* uv_rect, const s16* scale, const s16* offset);
void debugchr_gfx_set_scaled_poly_ft4_geometry_and_uv(POLY_FT4* poly, const debug_chr_texture_page_t* texture,
    const RECT* position, const main_zodiac_sprite_frame_t* uv_rect, const point16_t* scale, const s16* offset);
void debugchr_gfx_clip_portrait_poly_from_left(POLY_FT4* poly, s32 amount);
void debugchr_gfx_clip_portrait_poly_from_right(POLY_FT4* poly, s32 amount);
void debugchr_gfx_set_clut_rect_from_id(RECT* rect, s32 packed);

/* panel */
void debugchr_panel_copy_unit_data_to_billboard(battle_stats_t* unit, u8* output);
void debugchr_panel_set_primitive_colors(
    battle_menu_status_panel_primitives_t* primitives, const battle_menu_status_panel_frame_config_t* state);
void debugchr_panel_run_character_status_thread(void);

/* text */
void debugchr_text_clear_string_buffer(u8* output);
void debugchr_text_render_decimal_entry_list(s32 pixels, battle_menu_status_panel_gauge_entry_t* entries,
    battle_menu_status_panel_text_position_t* output, s32 count);
void debugchr_text_render_decimal_value(
    s32 value, s32 flags, void* pixels, battle_menu_status_panel_text_position_t* position);
void debugchr_text_render_signed_decimal_entries(s32 pixels, battle_menu_status_panel_gauge_entry_t* entries,
    battle_menu_status_panel_text_position_t* output, s32 count);

/* thread */
s32 debugchr_thread_is_running(s32 thread_id);

extern s16 g_debugchr_editor_brave;
extern help_navigation_record_t* g_debugchr_editor_current_field;
extern POLY_FT4 g_debugchr_editor_cursor_polys[];
extern POLY_FT4 g_debugchr_editor_cursor_shade_polys[];
extern s16 g_debugchr_editor_faith;
extern s32 g_debugchr_editor_field_edit_active;
extern help_navigation_record_t* g_debugchr_editor_field_table;
extern help_navigation_record_t g_debugchr_editor_fields[];
extern debug_chr_texture_page_t g_debugchr_gfx_scaled_sprite_texture_page;
extern battle_menu_status_panel_numeric_entry_t g_debugchr_panel_comparison_numeric_entries[];
extern CVECTOR g_debugchr_panel_gauge_bar_colors[];
extern battle_menu_status_panel_numeric_entry_t g_debugchr_panel_selected_numeric_entries[];
extern const debug_chr_texture_page_t g_debugchr_panel_status_texture;
extern battle_menu_status_panel_glyph_t g_debugchr_text_decimal_glyph;

void debugchr_editor_apply_values_to_battle_unit(s32 unit_id);
void debugchr_gfx_apply_menu_palette_for_mode(void* output, u8* context);
void debugchr_gfx_build_portrait_transition_primitives(const RECT* texture_rect, s32* transition,
    const s32* first_portrait, const s32* second_portrait, u8* image, POLY_FT4* poly, s32 direction);

#endif
