#ifndef EVENT_REQUIRE_H
#define EVENT_REQUIRE_H

/* EVENT/REQUIRE.OUT: menu screen overlay that runs with BATTLE. */

#include "fft/battle.h"

/* post battle */
extern job_data_t* g_require_formation_job_data;
extern menu_entry_t g_require_join_menu_descriptor[];
extern u8 g_require_work[];

s32 require_party_update_brave_faith_and_collect_warnings(void);
void require_party_apply_post_battle_changes(void);
void require_overlay_open_jobstts(void);
void require_overlay_run_helpmenu(void);
void require_party_report_brave_departure_stub(
    s32 unused_battle_index, s32 unused_unit_id, s32 unused_character_identity);

/* unit */
/* Three unit ids whose presence selects the event variable at +6. */
typedef struct require_party_bio_update_record {
    u16 unit_ids[3];
    u16 variable;
} require_party_bio_update_record_t;

extern menu_active_unit_t g_require_active_unit_data;
extern s32 g_require_party_affected_battle_unit_indices[];
extern s32 g_require_party_affected_unit_count;
extern s32 g_require_party_affected_unit_message_args[];
extern s32 g_require_party_affected_unit_message_ids[];
extern require_party_bio_update_record_t g_require_party_bio_update_records[11];
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
extern menu_entry_t g_require_party_removal_menus[6];
extern s16 g_require_party_removal_result;

void require_gfx_prepare_party_portrait_textures(void);
void require_party_report_brave_faith_changes(void);
s32 require_party_find_low_brave_high_faith_departures(void);
s32 require_party_get_free_slot_status(s32 unit_id);
void require_party_remove_low_brave_high_faith_units(void);
s32 require_party_restore_item_counts(s32 party_index);
s32 require_party_save_battle_units_and_find_join_candidates(void);
s32 require_party_sell_equipment_over_limit(s32 unit_id);
s32 require_party_store_equipment_or_sell_overflow(s32 unit_id);
void require_party_update_bio_variable_for_unit_class(s32 unit_class, s32 value);
s32 require_party_select_unit_to_remove(void);

/* conditions */
/* One color record per display-condition quad; the fourth byte of each
 * CVECTOR is not consumed by the banner renderer. */
typedef struct require_display_condition_colors {
    u8 _unused_00[4];    /* 0x00 */
    CVECTOR vertices[4]; /* 0x04 */
} require_display_condition_colors_t;
typedef char
    require_display_condition_colors_size_must_be_20[sizeof(require_display_condition_colors_t) == 20 ? 1 : -1];

typedef struct require_render_condition_tpage {
    u8 _unused_00;
    u8 _unused_01;
    u8 page; /* Bits 5 and 6 select the texture page. */
    u8 _unused_03;
} require_render_condition_tpage_t;

extern u8 g_require_condition_general_quad_ranges[];
extern sprite_sheet_frame_t g_require_condition_general_quads[];
extern require_render_condition_tpage_t g_require_condition_general_tpages[];
extern require_display_condition_colors_t g_require_condition_general_vertex_colors[];
/* Per-condition first-quad indices; entry n+1 bounds entry n. */
extern u8 g_require_condition_special_quad_ranges[];
extern sprite_sheet_frame_t g_require_condition_special_quads[];
/* 0x14-byte vertex-color records, one per quad. */
extern require_display_condition_colors_t g_require_condition_special_vertex_colors[];

/* REQUIRE 0x801cafd4, invoked by both BATTLE and WORLD event dispatch. */
void require_condition_dispatch(void);
void require_condition_show_ready_bugged(void);
void require_condition_show_general(s32 value);
void require_condition_run_special_thread(void);

/* rewards */
/* Double-buffered war-trophy primitives, copied when each page opens. */
typedef struct require_reward_war_trophy_primitives {
    POLY_FT4 quads[16]; /* 0x000 */
    SPRT banner;        /* 0x280 */
    DR_MODE draw_mode;  /* 0x294 */
} require_reward_war_trophy_primitives_t;
typedef char require_reward_war_trophy_primitives_size_must_be_672[sizeof(require_reward_war_trophy_primitives_t) == 672
        ? 1
        : -1];

/* One 12x24 burst tile: a double-buffered flat quad plus its motion state. */
typedef struct require_reward_burst_tile {
    POLY_F4 poly[2];   /* 0x00 */
    s32 delay;         /* 0x30 */
    s32 radius;        /* 0x34 */
    s32 angle;         /* 0x38 */
    s32 x;             /* 0x3C */
    s32 y;             /* 0x40 */
    s32 initial_delay; /* 0x44 */
} require_reward_burst_tile_t;
typedef char require_reward_burst_tile_size_must_be_72[sizeof(require_reward_burst_tile_t) == 72 ? 1 : -1];

/* BONUS.BIN stores a 256x200 4bpp image, followed by eight 16-color palette slots. */
typedef struct require_reward_bonus_image_file {
    u32 pixels[200][32];   /* 0x0000; eight 4bpp pixels per 32-bit value */
    palette_t palettes[8]; /* 0x6400; four palettes per uploaded VRAM row */
    u8 _unknown_6500[0x300];
} require_reward_bonus_image_file_t;
typedef char
    require_reward_bonus_image_file_size_must_be_26624[sizeof(require_reward_bonus_image_file_t) == 26624 ? 1 : -1];

/* g_require_reward_war_trophy_entry_types[]: what a war trophy entry's
 * value in g_require_reward_war_trophy_entry_values is. */
typedef enum require_war_trophy_entry {
    REQUIRE_WAR_TROPHY_ENTRY_ITEM = 0,        /* an item id */
    REQUIRE_WAR_TROPHY_ENTRY_BONUS_MONEY = 1, /* the gil amount */
} require_war_trophy_entry_e;

/* The mode byte of the bonus-image burst parameters: the distance that sets
 * each tile's start delay. Any other value measures along a rotated x axis. */
typedef enum require_reward_burst_mode {
    REQUIRE_REWARD_BURST_FROM_CENTER = 0,
    REQUIRE_REWARD_BURST_FROM_ORIGIN = 1, /* the top-left corner */
    REQUIRE_REWARD_BURST_HORIZONTAL = 2,  /* x only */
} require_reward_burst_mode_e;

extern RECT g_require_reward_bonus_image_rect;
extern RECT g_require_reward_bonus_palette_rect;
extern DR_MODE g_require_reward_burst_draw_modes[2];
extern POLY_F4 g_require_reward_burst_flash_polys[2];
extern MATRIX g_require_reward_burst_matrix;
extern VECTOR g_require_reward_burst_offset;
extern VECTOR g_require_reward_burst_rotated_offset;
extern SVECTOR g_require_reward_burst_rotation;
extern require_reward_burst_tile_t g_require_reward_burst_tiles[12][24];
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
extern s32 g_require_reward_war_trophy_entry_count;
extern s32 g_require_reward_war_trophy_entry_types[];
extern s32 g_require_reward_war_trophy_entry_values[];
/* Upload rectangle for the current entry's text image. */
extern RECT g_require_reward_war_trophy_image_rect;
extern u8 g_require_reward_war_trophy_item_count;
extern require_reward_war_trophy_primitives_t g_require_reward_war_trophy_primitives[2];
extern battle_war_result_t g_require_reward_war_trophy_search_result;

void require_reward_run_gil_ticker(void);
void require_reward_run_war_trophies(void);
void require_reward_animate_bonus_image_burst(void);
void require_reward_collect_war_trophies_and_bonus_money(void);
s32 require_reward_get_war_trophy_column_x(s32 value, s32 index);
s32 require_reward_get_war_trophy_entry_offset(s32 value, s32 index);
void require_reward_load_bonus_image(s32 image_id);

/* character status */
extern RECT g_require_character_status_draw_area_rect;
extern menu_status_thread_t g_require_character_status_frame_config;
extern RECT g_require_character_status_frame_rect[];
/* The scalar x alias preserves the retail transition-store scheduling. */
extern s32 g_require_character_status_origin_x;
extern s32 g_require_character_status_redraw_request;

/* unit panels */
extern menu_unit_status_record_t g_require_panel_comparison_billboard;
extern menu_status_panel_editor_packet_t g_require_panel_comparison_editor_packets[2];
extern u8 g_require_panel_comparison_large_number_image[];
extern u8 g_require_panel_comparison_name_image[];
extern u8 g_require_panel_comparison_number_image[];
extern menu_number_entry_t g_require_panel_comparison_numeric_entries[];
extern menu_status_panel_packet_t g_require_panel_comparison_packets[2];
extern palette_t g_require_panel_comparison_portrait_palettes[2];
extern RECT g_require_panel_comparison_portrait_rect;
extern menu_active_unit_t g_require_panel_comparison_unit_data;
extern s32 g_require_panel_dim_a;
extern s32 g_require_panel_dim_b;
extern s32 g_require_panel_dim_c;
extern image_load_parameters_t g_require_panel_editor_label_cells[];
extern u16 g_require_panel_editor_label_cluts[][2];
extern image_load_parameters_t g_require_panel_editor_mode_cell[];
extern image_load_parameters_t g_require_panel_editor_mode_cells[];
extern image_load_parameters_t g_require_panel_editor_value_cells[];
extern u16 g_require_panel_editor_value_cluts[][2];
extern RECT g_require_panel_frame_rect;
extern menu_status_panel_buffer_t g_require_panel_frames_a[];
extern CVECTOR g_require_panel_gauge_bar_colors[];
extern image_load_parameters_t g_require_panel_item_icon_layouts[];
extern point16_t g_require_panel_item_icon_texture[];
extern image_load_parameters_t g_require_panel_label_layouts_mode0[];
extern image_load_parameters_t g_require_panel_label_layouts_mode1[];
extern image_load_parameters_t g_require_panel_label_layouts_mode2[];
extern image_load_parameters_t g_require_panel_label_layouts_mode3[];
extern RECT g_require_panel_origin_offsets[];
extern image_load_parameters_t g_require_panel_portrait_cell[];
extern menu_unit_status_record_t g_require_panel_selected_billboard;
extern menu_status_panel_editor_packet_t g_require_panel_selected_editor_packets[2];
extern u8 g_require_panel_selected_large_number_image[];
extern u8 g_require_panel_selected_name_image[];
extern u8 g_require_panel_selected_number_image[];
extern menu_number_entry_t g_require_panel_selected_numeric_entries[];
extern menu_status_panel_packet_t g_require_panel_selected_packets[2];
extern palette_t g_require_panel_selected_portrait_palettes[2];
extern RECT g_require_panel_selected_portrait_rect;
extern image_load_parameters_t g_require_panel_sprite_cells[];
extern u8 g_require_panel_text_image_a[];
extern u8 g_require_panel_text_image_b[];
extern RECT g_require_panel_text_upload_rect_a;
extern RECT g_require_panel_text_upload_rect_b;
/* The scalar x alias preserves the retail transition-store scheduling. */
extern s32 g_require_status_display_origin_x;
extern s32 g_require_status_display_redraw_request;
extern menu_status_thread_t g_require_status_display_thread_params;

void require_panel_copy_stats_to_gauges(struct unit_stats* unit, menu_unit_status_record_t* output);
void require_panel_run_character_status_thread(void);
void require_panel_set_transition_value(s32 value);
void require_panel_run_unit_status_thread(void);

/* numeric editor */
extern u8 g_require_editor_job_name[];
extern menu_number_entry_t g_require_editor_numeric_descriptor_a[];
extern menu_number_entry_t g_require_editor_numeric_descriptor_b[];
extern RECT g_require_editor_numeric_entries_a;
extern RECT g_require_editor_numeric_entries_b;
extern RECT g_require_editor_numeric_entries_c;
extern RECT g_require_editor_numeric_entries_d;
extern RECT g_require_editor_numeric_entries_e;
extern RECT g_require_editor_numeric_entries_f;
extern point16_t g_require_editor_numeric_geometry[];
extern menu_status_panel_numeric_buffer_t g_require_editor_numeric_state[2];
extern battle_menu_status_panel_numeric_geometry_t g_require_editor_numeric_table;
extern menu_unit_stat_number_images_t g_require_editor_numeric_text_a;
extern menu_unit_stat_number_images_t g_require_editor_numeric_text_b;
extern image_load_parameters_t g_require_editor_numeric_texture[];
extern struct unit_stats g_require_editor_party_unit_stats;
extern u8 g_require_editor_primary_skillset_name[];
extern u8 g_require_editor_secondary_skillset_name[];
extern menu_status_panel_slot_storage_t g_require_editor_unit_fields;
extern u8 g_require_editor_unit_name[];
/* The scalar x alias preserves the retail transition-store scheduling. */
extern s32 g_require_numeric_editor_origin_x;
extern s32 g_require_numeric_editor_redraw_request;
extern menu_status_thread_t g_require_numeric_editor_thread_params;

void require_editor_load_party_unit(s32 unused, s32 party_index);
void require_editor_load_selected_unit(void);
void require_editor_run_numeric_thread(void);

/* menus */
extern s16 g_require_menu_selection_result;
extern s16 g_require_message_menu_result;

/* find (BATTLE) */
void require_menu_wait_selection_threads(void);

/* portraits */
extern u8 g_require_portrait_cell_pixels[];
extern RECT g_require_portrait_cell_rect;
extern u16 g_require_portrait_index_by_sprite_set[];
extern RECT g_require_portrait_palette_rect;
extern RECT g_require_portrait_sheet_rect_0;
extern RECT g_require_portrait_sheet_rect_1;
extern RECT g_require_portrait_sheet_rect_2;
extern RECT g_require_portrait_sheet_rect_3;
extern RECT g_require_special_portrait_palette_rect;
extern RECT g_require_special_portrait_rect;

/* graphics */
extern u16 g_require_gfx_draw_offset_y;
extern u8 g_require_gfx_fade_rgb[3];
extern u8 g_require_gfx_formation_sprite_groups[][3];
extern POLY_FT4 g_require_gfx_poly_ft4_buffers[2][30];
extern POLY_GT4 g_require_gfx_poly_gt4_array_32[];
extern POLY_GT4 g_require_gfx_poly_gt4_array_8[];
extern POLY_GT4 g_require_gfx_poly_gt4_buffers[2][40];

void require_gfx_build_gradient_grid_primitives(POLY_GT4* poly);
void require_gfx_build_portrait_poly_ft4(s32 flags, POLY_FT4* output);
void require_gfx_build_scaled_draw_area_packets(scaled_draw_area_pair_t* packet, const RECT* source, s32 scale_index,
    s32 lower_half, const menu_status_thread_t* thread);
void require_gfx_fade_rgb_31_frames(s32 target_0, s32 target_1, s32 target_2);
void require_gfx_init_poly_gt4_array_32(s32 x, s32 y);
void require_gfx_init_poly_gt4_array_8(s32 x, s32 y);
void require_gfx_scale_poly_gt4_vertex_colors(
    const require_display_condition_colors_t* colors, s32 scale, POLY_GT4* poly);

/* text */
/* Section offsets of the REQUIRE text file, relative to g_require_text_data (0x80 bytes before it). */
/* REQUIRE's text, which its post-battle routines bind over the resident
 * sections; it binds its job names as TEXT_SECTION_JOB_NAMES. */
typedef enum require_text_section {
    REQUIRE_TEXT_SECTION_BRAVE_FAITH_WARNINGS = 1,   /* a unit's Brave is low or its Faith high */
    REQUIRE_TEXT_SECTION_BRAVE_FAITH_DEPARTURES = 2, /* the unit leaves the party for it */
    REQUIRE_TEXT_SECTION_DISMISS_UNIT_REPLIES = 3,   /* what a unit turned away or dismissed says */
} require_text_section_e;

extern u8 g_require_text_data[];
extern s32 g_require_text_section_offsets[32];
extern char g_require_text_wait_for_file_allocation_message[];

void require_text_build_battle_nicknames(void);
void require_text_render_decimal_entry_list(
    u8* pixels, const menu_number_entry_t* entries, glyph_blit_t* output, s32 count);
void require_text_render_signed_decimal_entries(
    u8* pixels, menu_number_entry_t* entries, glyph_blit_t* output, s32 count);
void require_text_show_battle_congratulations(void);

/* input */

void require_input_wait_frames_or_skip(s32 frames);

/* threads */
/* Slots of the post-battle screens' own threads; thread_screen_slot_e
 * has the shared ones. */
typedef enum require_thread_slot {
    REQUIRE_THREAD_SLOT_MENU = 5,        /* selection menus, prompts and the special-case display */
    REQUIRE_THREAD_SLOT_MESSAGE = 7,     /* the icon strip, message text or menu shown with them */
    REQUIRE_THREAD_SLOT_TRANSITION = 10, /* require_gfx_run_suspended_thread_transition */
} require_thread_slot_e;

extern u8 g_require_saved_thread_block[];
extern s32 g_require_thread_suspended_id;

void require_gfx_run_suspended_thread_transition(void);
void require_thread_exit_current(void);

/* sound */

#endif
