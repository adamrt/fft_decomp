#ifndef FFT_EVENT_ATTACK_H
#define FFT_EVENT_ATTACK_H

/* EVENT/ATTACK.OUT: menu screen overlay that runs with BATTLE. */

#include "fft/battle.h"

/* character */
extern battle_menu_status_panel_offset_pair_t g_attack_character_status_frame_rect[];
extern RECT g_attack_character_status_draw_area_rect;
extern s32 g_attack_character_status_redraw_request;
extern u8 g_attack_character_status_frame_config[];

/* editor */
extern u8 g_attack_editor_numeric_descriptor_a[];
extern u8 g_attack_editor_numeric_descriptor_b[];
extern RECT g_attack_editor_numeric_entries_a;
extern RECT g_attack_editor_numeric_entries_b;
extern RECT g_attack_editor_numeric_entries_c;
extern RECT g_attack_editor_numeric_entries_d;
extern RECT g_attack_editor_numeric_entries_e;
extern RECT g_attack_editor_numeric_entries_f;
extern battle_image_location_t g_attack_editor_numeric_geometry[];
extern battle_menu_status_panel_numeric_buffer_t g_attack_editor_numeric_state[2];
extern battle_menu_status_panel_numeric_geometry_t g_attack_editor_numeric_table;
extern u8 g_attack_editor_numeric_text_a[];
extern u8 g_attack_editor_numeric_text_b[];
extern u8 g_attack_editor_numeric_texture[];
extern battle_menu_status_panel_gauges_t g_attack_editor_status_gauges;
extern s16 g_attack_editor_team_state;
extern s16 g_attack_editor_job_id;
extern s16 g_attack_editor_brave;
extern s16 g_attack_editor_faith;
extern s16 g_attack_editor_zodiac;
extern s16 g_attack_editor_selected_unit_id;
extern u8 g_attack_editor_third_ability_bytes[];
extern u8 g_attack_editor_fourth_ability_bytes[];

/* Five name ids and five ability/skillset ids of the unit loaded into the
 * editor (g_attack_unit_editor_state + 0x24). The target walks them from this
 * standalone address: indexing the editor record instead folds 0x24 into each
 * load and allocates the walk from the record base. */
extern s16 g_attack_editor_item_and_ability_ids[10];
extern struct battle_stats g_attack_editor_party_unit_stats;
void attack_editor_run_numeric_thread(void);

/* formation */
extern job_data_t* g_attack_formation_job_data;
extern RECT g_attack_formation_sheet_rect;
extern RECT g_attack_formation_sheet_source_rect;
extern u8 g_attack_formation_sprite_staging[];
extern u8 g_attack_formation_sprite_tall_cell[];

/* gfx */
/* Sprite-cell geometry shared by ATTACK.OUT's scaled POLY_FT4 builders. */
typedef struct attack_gfx_texture_page_position {
    s16 x;
    u16 y;
} attack_gfx_texture_page_position_t;
typedef char attack_texture_page_position_size_must_be_4[(sizeof(attack_gfx_texture_page_position_t) == 4) ? 1 : -1];

typedef struct attack_gfx_point {
    s16 x;
    s16 y;
} attack_gfx_point_t;
typedef char attack_point_size_must_be_4[(sizeof(attack_gfx_point_t) == 4) ? 1 : -1];

typedef struct attack_gfx_sprite_rect {
    u8 u; /* 0x00 */
    u8 _unused_01;
    u8 v;           /* 0x02 */
    u8 _padding_03; /* aligns width */
    s16 width;      /* 0x04 */
    s16 height;     /* 0x06 */
    s16 offset_x;   /* 0x08 */
    s16 offset_y;   /* 0x0A */
} attack_gfx_sprite_rect_t;
typedef char attack_sprite_rect_size_must_be_0x0c[(sizeof(attack_gfx_sprite_rect_t) == 0x0C) ? 1 : -1];

extern const u16 g_attack_gfx_draw_area_scale_percent[12];
extern POLY_FT4 g_attack_gfx_frame_ft4[];
extern s16 g_attack_gfx_portrait_origin[];
extern u8 g_attack_gfx_portrait_transition_offsets[];
extern u8 g_attack_gfx_scaled_sprite_count;
extern attack_gfx_point_t g_attack_gfx_scaled_sprite_scale[];
extern attack_gfx_texture_page_position_t g_attack_gfx_scaled_sprite_texture_page[];
extern attack_gfx_sprite_rect_t g_attack_gfx_scaled_sprite_uv_rects[];
extern RECT g_attack_gfx_draw_area_template;
extern u16 g_attack_gfx_draw_offset_y;
extern u16 g_attack_portrait_index_by_sprite_set[];
extern RECT g_attack_portrait_palette_upload_rect;
extern RECT g_attack_portrait_sheet_rect_0;
extern RECT g_attack_portrait_sheet_rect_1;
extern RECT g_attack_portrait_sheet_rect_2;
extern RECT g_attack_portrait_sheet_rect_3;
extern u8 g_attack_portrait_staging[];
extern RECT g_attack_portrait_upload_rect;

void attack_gfx_apply_menu_palette_for_mode(
    world_menu_palette_primitives_t* primitives, const battle_menu_status_panel_frame_config_t* thread);

void attack_gfx_build_formation_sprites(void);
void attack_gfx_build_portrait_poly_ft4(s32 portrait_id, POLY_FT4* poly);

void attack_gfx_build_scaled_draw_area_packets(battle_menu_status_panel_portrait_primitive_tail_t* packet,
    const RECT* source, s32 scale_index, s32 lower_half, const s16* offset);

void attack_gfx_build_scaled_sprite_cell_poly_ft4(POLY_FT4* poly, attack_gfx_texture_page_position_t* texture_page,
    const RECT* position, attack_gfx_sprite_rect_t* sprite_rect, attack_gfx_point_t* scale, const s16* offset);

void attack_gfx_clip_portrait_poly_from_left(POLY_FT4* poly, s32 amount);
void attack_gfx_clip_portrait_poly_from_right(POLY_FT4* poly, s32 amount);
void attack_gfx_init_menu_tile_and_line_primitives(battle_menu_status_panel_buffer_t* menu);
void attack_gfx_init_scaled_draw_area_packets(battle_menu_status_panel_portrait_primitive_tail_t* data);
void attack_gfx_load_portraits(void);
void attack_gfx_set_clut_rect_from_id(RECT* rect, s32 clut_id);
void attack_gfx_set_formation_icon_uv(POLY_FT4* primitive, s32 icon_id);

void attack_gfx_set_scaled_poly_ft4_geometry(POLY_FT4* poly, attack_gfx_texture_page_position_t* texture_page,
    attack_gfx_point_t* position, attack_gfx_sprite_rect_t* sprite_rect, attack_gfx_point_t* scale, POLY_FT4* base);

void attack_gfx_prepare_formation_sprites(void);

void attack_gfx_build_portrait_transition_primitives(const RECT* texture_rect, s32* transition,
    const s32* first_portrait, const s32* second_portrait, u8* image, POLY_FT4* poly, s32 direction);

void attack_gfx_build_shaded_panel_strips(POLY_GT4* polys, s32 width, s32 shade, s32 reverse);
void attack_gfx_build_status_group_primitives(POLY_FT4* poly);
void attack_gfx_init_frame_ft4_array(void);

/* deploy */
typedef enum attack_deployment_orientation {
    ATTACK_DEPLOYMENT_ORIENTATION_NORMAL = 0,
    ATTACK_DEPLOYMENT_ORIENTATION_ROTATE_90 = 1,
    ATTACK_DEPLOYMENT_ORIENTATION_ROTATE_180 = 2,
    ATTACK_DEPLOYMENT_ORIENTATION_ROTATE_270 = 3,
} attack_deployment_orientation_e;

enum {
    ATTACK_DEPLOYMENT_ORIENTATION_MASK = 0x0f,
};

/* Four-byte placement record per deployed unit at
 * g_attack_deployed_unit_map_coordinates: party ID, X, Y, flags. */
typedef struct attack_deployed_unit_coordinate {
    u8 roster_id;  /* 0x00 */
    u8 x;          /* 0x01 */
    u8 y;          /* 0x02 */
    u8 facing : 2; /* 0x03 */
    u8 unk_3_2 : 3;
    u8 unk_3_5 : 2;
    u8 upper_level : 1;
} attack_deployed_unit_coordinate_t;

/* Twelve-byte deployment-area record used by EVENT/ATTACK.OUT. The exact
 * roster-fieldability routine proves the stride, unit limit, and map ID;
 * valid-deployment-tile accessors establish the 5-by-5 bitmap. */
typedef struct attack_deployment_squad_data {
    u32 valid_tile_bitmap; /* 0x00; 5-by-5 deployment mask */
    s8 center_x;           /* 0x04 */
    s8 center_y;           /* 0x05 */
    u8 _unknown_06;        /* 0x06 */
    u8 orientation_flags;  /* 0x07; original and rotated orientations */
    u8 unit_limit;         /* 0x08 */
    u8 map_id;             /* 0x09 */
    u16 placement_id;      /* 0x0a */
} attack_deployment_squad_data_t;
typedef char attack_squad_data_size_must_be_0x0c[(sizeof(attack_deployment_squad_data_t) == 0x0c) ? 1 : -1];

/* One 0xa2c-byte half of the double-buffered deployment-screen primitive
 * block at 0x801db650 (g_attack_deploy_render_buffers). The 5-by-5 valid-tile
 * sprites occupy its head; the two cursors each own a marker quad ([n][0],
 * bobbing above the tile) and a tile-highlight diamond ([n][1]), positioned
 * from g_attack_deploy_cursor_row/column[n]. */
typedef struct attack_deploy_render_buffer {
    SPRT tiles[5][5];                   /* 0x000 */
    POLY_FT4 portraits[10];             /* 0x1f4 */
    POLY_FT4 status[30];                /* 0x384 */
    u8 _unknown_834[0x28];              /* 0x834 */
    POLY_F3 arrow;                      /* 0x85c */
    u8 _unused_870[0x14];               /* 0x870 */
    u8 menu_cursor[0xa0];               /* 0x884 */
    POLY_FT4 cursor[2][2];              /* 0x924 */
    DR_MODE draw_mode_9c4;              /* 0x9c4 */
    DR_MODE draw_mode_9d0;              /* 0x9d0 */
    DR_MODE draw_mode_9dc;              /* 0x9dc */
    DR_MODE draw_mode_9e8;              /* 0x9e8 */
    DR_MODE draw_mode_9f4;              /* 0x9f4 */
    DR_MODE menu_draw_mode;             /* 0xa00 */
    DR_OFFSET world_offset;             /* 0xa0c */
    attack_gfx_point_t world_position;  /* 0xa18 */
    DR_OFFSET screen_offset;            /* 0xa1c */
    attack_gfx_point_t screen_position; /* 0xa28 */
} attack_deploy_render_buffer_t;
typedef char attack_deploy_render_buffer_size_must_be_0xa2c[(sizeof(attack_deploy_render_buffer_t) == 0xa2c) ? 1 : -1];

extern s32 g_attack_deploy_arrow_position_mode;
extern s32 g_attack_deploy_current_squad;
extern s32 g_attack_deploy_current_squad_id;
extern s32 g_attack_deploy_cursor_column[2];
extern s32 g_attack_deploy_cursor_row[2];
extern s32 g_attack_deploy_fieldable_unit_count;
extern s32 g_attack_deploy_menu_state;
extern s32 g_attack_deploy_pending_roster_index;
extern u8 g_attack_deploy_roster_id_by_tile[5][5];
extern s32 g_attack_deploy_roster_navigation_direction;
extern u8 g_attack_deploy_roster_unit_deployable[20];
extern s32 g_attack_deploy_second_pass;
extern s32 g_attack_deploy_selected_roster_index;
extern s32 g_attack_deploy_selected_unit_deployed;
extern attack_deployment_squad_data_t* g_attack_deploy_squad_data;
extern attack_deploy_render_buffer_t g_attack_deploy_render_buffers[2];
extern u8 g_attack_deploy_units_by_squad[4][25];
extern u8 g_attack_deploy_valid_tiles[5][5];
extern attack_deployed_unit_coordinate_t g_attack_deployed_unit_map_coordinates[12];
extern u32 g_attack_deploy_cursor_texture_data[];
extern s32 g_attack_deploy_active_cursor;
extern s32 g_attack_deploy_grid_slide_x;
extern s32 g_attack_deploy_grid_slide_y;
extern s32 g_attack_deploy_tiles_only_mode;
extern RECT g_attack_deploy_arrow_clut_rect;
extern u8 g_attack_deploy_texture_location[];
extern u8 g_attack_deploy_menu_cursor_cells[];
extern help_navigation_record_t g_attack_deploy_select_menu_nodes[];
extern u8* g_attack_deploy_unit_text_section; /* g_attack_scenario_table string arena, installed as text section 26 */

extern u8*
    g_attack_deploy_help_text_section; /* g_attack_event_condition_blocks help text, installed as text section 27 */

extern u32 g_attack_deploy_arrow_clut_data[];
extern s16 g_attack_deploy_confirm_result;
extern attack_deployment_squad_data_t* g_attack_deploy_current_squad_data;
extern POLY_FT4* g_attack_deploy_cursor_0_poly;
extern s32 g_attack_deploy_cursor_0_submitted;
extern POLY_FT4* g_attack_deploy_cursor_1_poly;
extern s32 g_attack_deploy_cursor_1_submitted;
extern u16 g_attack_deploy_cursor_clut[16];
extern RECT g_attack_deploy_cursor_clut_rect;
extern u16 g_attack_deploy_cursor_palette_cycle[];
extern u8 g_attack_deploy_cursor_palette_source[];
extern s32 g_attack_deploy_deployed_unit_count;
extern world_menu_entry_t g_attack_deploy_menu_entries[];
extern RECT g_attack_deploy_rect_09c;
extern RECT g_attack_deploy_rect_0ac;
extern RECT g_attack_deploy_rect_0cc;
extern s32 g_attack_deploy_selected_unit_state[];
extern u16 g_attack_deploy_semitrans_clut_buffer[0x10];
extern s32 g_attack_deploy_slot_offsets[5];
extern world_gfx_image_load_parameters_t g_attack_deploy_text_graphic_params[9];
extern POLY_FT4 g_attack_deploy_text_polys[2][9];
extern s32 g_attack_deploy_text_reveal_heights[9];
extern s32 g_attack_deploy_text_reveal_steps[9];
extern battle_image_location_t g_attack_deploy_text_screen_origin;
extern u16 g_attack_deploy_tile_clut_buffer[0x30];
extern u32 g_attack_deploy_tile_clut_data[];
extern RECT g_attack_deploy_tile_clut_rect;
extern u8 g_attack_deploy_unit_fieldable[PARTY_ROSTER_SLOT_COUNT];
extern RECT g_attack_deploy_vram_copy_rect;
extern s16 g_attack_deploy_vram_copy_x;
extern s16 g_attack_deploy_vram_copy_y;

/* The brightness byte of g_attack_deploy_zodiac_draw_context (+4) as its own s32 symbol: the fade decrements the whole
 * word (lw/addiu/sw), which a u8 member access would not reproduce. The array form lets GCC hoist its address out of
 * the fade loop, as the target does. */
extern s32 g_attack_deploy_zodiac_brightness_word[1];
extern zodiac_draw_context_t g_attack_deploy_zodiac_draw_context;
void attack_deploy_build_menu_cursor_primitives(s32 frame, u32 mode, u8* render_buffer);
void attack_deploy_build_screen_arrow(void);
void attack_deploy_find_fieldable_units(void);
s32 attack_deploy_is_roster_unit_deployed(s32 roster_id);
void attack_deploy_mark_other_squad_units_undeployable(void);
void attack_deploy_select_next_roster_unit(void);
void attack_deploy_select_previous_roster_unit(void);
void attack_deploy_run_screen(void);
void attack_deploy_run_render_thread(void);
void attack_deploy_build_portrait_quads(s32 column, s32 row, s32 index, POLY_FT4* poly);
void attack_deploy_handle_roster_navigation_input(void);
s32 attack_deploy_has_roster_id_on_tiles(s32 roster_id);
void attack_deploy_remove_roster_unit_from_tiles(s32 roster_id);
void attack_deploy_reset_tables(void);
void attack_deploy_run_select_menu(void);
void attack_deploy_select_unit_for_stats_display(void);
void attack_deploy_swap_roster_unit_tile(s32 column, s32 row, s32 roster_id);
void attack_deploy_update_screen_graphics(void);
void attack_init_deployment_cursor_primitives(void);
void attack_update_deployment_cursor_primitives(s32 frame, attack_deploy_render_buffer_t* buffer);
void* attack_deploy_get_coordinate_slots(void);

/* map */
extern s32 g_attack_map_title_image_rect;
extern s32 g_attack_map_title_palette_data;
extern s32 g_attack_map_title_palette_rect;
extern s32 g_attack_map_title_closing_past_midpoint;

/* Two buffers of the four shaded title-panel strips. */
extern POLY_GT4 g_attack_map_title_sprites[2][4];

/* Scenario interpreter interfaces shared by battle_script_execute_event and
 * world_script_execute_event. Signatures are as the interpreters bind them;
 * unverified names stay provisional. */
void attack_map_load_title_graphic(void);
void attack_map_show_title(void);

/* menu */
extern s32 g_attack_menu_indicator_brightness[2];
extern s32 g_attack_menu_indicator_fade_work[2];
extern s32 g_attack_menu_indicator_packet_index[2];
extern s32 g_attack_menu_indicator_state[2];

/* panel */
extern battle_menu_status_panel_gauges_t g_attack_panel_comparison_billboard;
extern battle_image_location_t g_attack_panel_item_icon_texture[];
extern battle_image_location_t g_attack_panel_origin_offsets[];
extern RECT g_attack_panel_text_upload_rect_a;
extern RECT g_attack_panel_text_upload_rect_b;
extern world_gfx_image_load_parameters_t g_attack_panel_label_layouts_mode0[];
extern world_gfx_image_load_parameters_t g_attack_panel_label_layouts_mode1[];
extern world_gfx_image_load_parameters_t g_attack_panel_label_layouts_mode2[];
extern world_gfx_image_load_parameters_t g_attack_panel_label_layouts_mode3[];
extern world_gfx_image_load_parameters_t g_attack_panel_item_icon_layouts[];
extern u8 g_attack_panel_status_animation[];
extern battle_menu_status_panel_buffer_t g_attack_panel_frames_a[];
extern u8 g_attack_panel_text_image_a[];
extern u8 g_attack_panel_text_image_b[];
extern u16 g_attack_panel_status_y_offsets[][2];
extern battle_menu_status_panel_editor_packet_t g_attack_panel_comparison_editor_packets[2];
extern u8 g_attack_panel_comparison_large_number_image[];
extern u8 g_attack_panel_comparison_name_image[];
extern u8 g_attack_panel_comparison_number_image[];
extern battle_menu_status_panel_numeric_entry_t g_attack_panel_comparison_numeric_entries[];
extern battle_menu_status_panel_packet_t g_attack_panel_comparison_packets[2];
extern u8 g_attack_panel_comparison_portrait_image[];
extern RECT g_attack_panel_comparison_portrait_rect;
extern s16 g_attack_panel_comparison_unit_data[];
extern u8 g_attack_panel_editor_label_cells[];
extern u16 g_attack_panel_editor_label_cluts[];
extern u8 g_attack_panel_editor_mode_cell[];
extern u8 g_attack_panel_editor_mode_cells[];
extern u8 g_attack_panel_editor_value_cells[];
extern u16 g_attack_panel_editor_value_cluts[];
extern u8 g_attack_panel_frame_rect[];
extern CVECTOR g_attack_panel_gauge_bar_colors[];
extern u8 g_attack_panel_portrait_cell[];
extern battle_menu_status_panel_editor_packet_t g_attack_panel_selected_editor_packets[2];
extern u8 g_attack_panel_selected_large_number_image[];
extern u8 g_attack_panel_selected_name_image[];
extern u8 g_attack_panel_selected_number_image[];
extern battle_menu_status_panel_numeric_entry_t g_attack_panel_selected_numeric_entries[];
extern battle_menu_status_panel_packet_t g_attack_panel_selected_packets[2];
extern u8 g_attack_panel_selected_portrait_image[];
extern RECT g_attack_panel_selected_portrait_rect;
extern s16 g_attack_panel_slide_down_y[];
extern s16 g_attack_panel_slide_up_y[];
extern u8 g_attack_panel_sprite_cells[];

/* Per-sprite first/end cell indices (interleaved pairs), the visible-entry
 * count, each entry's sprite id and fade/blink state, and a 4-byte record per
 * entry whose first halfword biases the draw point's y. */
extern u8 g_attack_panel_status_group_bounds[];
extern u8 g_attack_panel_status_group_count;
extern u8 g_attack_panel_status_group_ids[];
extern u16 g_attack_panel_status_position_y;
extern attack_gfx_point_t g_attack_panel_status_scales[];
extern attack_gfx_sprite_rect_t g_attack_panel_status_uv_rects[];

void attack_panel_set_primitive_colors(
    battle_menu_status_panel_buffer_t* primitives, const battle_menu_status_panel_frame_config_t* state);

void attack_panel_run_character_status_thread(void);

/* text */
extern s32 g_attack_numeric_editor_redraw_request;

/* Entries [1] and [2] of the unit-name text file's section-offset table (0x801cd2b0, s32[32], ending at
 * g_attack_text_data). They stay separate volatile symbols: as elements of the table GCC addresses them from the
 * g_attack_text_data base, and volatile keeps their loads in target order in attack_deploy_find_fieldable_units. */
extern volatile s32 g_attack_text_section_offset_1;
extern volatile s32 g_attack_text_section_offset_2;
extern u8 g_attack_text_data[];
extern u8 g_attack_numeric_editor_thread_params[];
extern battle_menu_status_panel_glyph_t g_attack_text_decimal_glyph;
void attack_text_build_deployment_strings(void);

void attack_text_render_decimal_entry_list(s32 pixels, battle_menu_status_panel_gauge_entry_t* entries,
    battle_menu_status_panel_text_position_t* out, s32 count);

void attack_text_render_decimal_value(
    s32 value, s32 flags, void* pixels, battle_menu_status_panel_text_position_t* position);

void attack_text_render_signed_decimal_entries(s32 pixels, battle_menu_status_panel_gauge_entry_t* entries,
    battle_menu_status_panel_text_position_t* out, s32 count);

void attack_text_init_battle_pointers(s32* offsets);

/* unit */
extern s16 g_attack_active_unit_data;

/* Status editor at 0x801ca0e8 (monster-skillset and two-hands flags at +0x0e/+0x10). */
extern battle_menu_status_panel_slot_storage_t g_attack_unit_editor_state;
void attack_load_party_unit_into_editor(s32 mode, s32 roster_index);
void attack_prepare_party_portrait_textures(void);
void attack_render_unit_status_panel_thread(void);

/* other */
extern u32* g_attack_input_controller;
extern battle_menu_status_panel_indicator_prims_t g_attack_thread_indicator_packets[2][2];
extern s32 g_attack_status_display_thread_params;
extern RECT attack_D_801CD0A4;
extern u8 g_attack_event_condition_blocks[];
extern u8 g_attack_monster_formation_sprite_table[];
extern u8 g_attack_scenario_table[];
extern RECT g_attack_special_portrait_palette_rect;
extern RECT g_attack_special_portrait_rect;
extern s32 g_attack_status_display_offset_y[];
s32 attack_load_scenario_conditionals(void);
void attack_sound_play_scenario_music(s32 primary_track, s32 alternate_track);
void attack_sound_wait_music_idle(void);
s32 attack_thread_is_running(s32 thread_id);
void attack_out_prepare_valid_deployment_tiles(SPRT* sprites);
void attack_file_load_sync(s32 lba, s32 size, void* destination);
s16 attack_load_unaligned_s16(const u8* ptr);
void attack_thread_wait_forever(void);
void attack_sound_resync_scenario_music_and_apply_map_darkness(void);

#endif
