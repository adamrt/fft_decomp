#ifndef FFT_EVENT_BUNIT_H
#define FFT_EVENT_BUNIT_H

/* EVENT/BUNIT.OUT: menu screen overlay that runs with BATTLE. */

#include "fft/battle.h"

/* ability */
extern s16 g_bunit_ability_category;
extern u8* g_bunit_ability_selected_data_1;
extern u8* g_bunit_ability_selected_specific;
extern s32 g_bunit_ability_type;
extern s32 g_bunit_ability_list_menu_script;
extern s8 g_bunit_ability_list_confirm_prompt_state;
extern s16 g_bunit_ability_entries[];
extern u16 g_bunit_ability_list_job_name_row[];
extern u8 g_bunit_ability_list_ready;
s32 bunit_ability_get_ct_display_value(s32 index);
u32 bunit_ability_get_jp_cost_display_value(s32 index);
u32 bunit_ability_get_mp_cost_display_value(s32 index);
s32 bunit_ability_is_action(void);
s32 bunit_ability_is_action_tab(void);
s32 bunit_ability_is_learned(s32 index);
s32 bunit_ability_is_movement_tab(void);
s32 bunit_ability_is_non_action_learned(s32 index);
s32 bunit_ability_is_non_action_unlearned(s32 index);
s32 bunit_ability_is_reaction_tab(void);
s32 bunit_ability_is_support_tab(void);
s32 bunit_create_ability_list(s16 unit_index, s16 job_id, s32 ability_category, s16* ability_list, s32 mode);
s32 bunit_run_ability_list_menu(void);

/* bits */
extern const u8* g_bunit_bits_cursor;
extern s32 g_bunit_bits_index;
extern u8 g_bunit_bits_reader_reset;
void bunit_bits_init_reader(const u8* data);
s32 bunit_bits_read(s32 bit_count);

/* character */
extern u8 g_bunit_character_status_frame_config;
extern s32 g_bunit_character_status_redraw_request;
extern s32 g_bunit_character_status_highlight;
extern u8 g_bunit_character_status_ability_text_image[];
extern RECT g_bunit_character_status_ability_text_rect;
extern RECT g_bunit_character_status_draw_area_rect;
extern u8 g_bunit_character_status_equipment_text_image[];
extern RECT g_bunit_character_status_equipment_text_rect;
extern RECT g_bunit_character_status_frame_rect;
extern battle_menu_status_panel_buffer_t g_bunit_character_status_frames[2];
extern world_gfx_image_load_parameters_t g_bunit_character_status_item_icon_layout[5];
extern world_gfx_image_load_parameters_t g_bunit_character_status_layout_mode0[19];
extern world_gfx_image_load_parameters_t g_bunit_character_status_layout_mode1[19];
extern world_gfx_image_load_parameters_t g_bunit_character_status_layout_mode2[19];
extern world_gfx_image_load_parameters_t g_bunit_character_status_layout_mode3[19];
extern RECT g_bunit_character_status_sprite_origin;

/* command */
extern u8 g_bunit_cmd_row_quad[];
extern u8* (*g_bunit_cmd_handlers[])(u8*);
extern u32 g_bunit_cmd_stream_input;
extern world_menu_window_command_t g_bunit_cmd_window_interior_command;
world_menu_window_command_t* bunit_cmd_draw_window_frame_handler(world_menu_window_command_t* command);
u8* bunit_cmd_draw_background_tiles_handler(u8* desc);
u8* bunit_cmd_draw_textured_quad_handler(u8* cmd);
void bunit_cmd_run_stream(u8* data, s32 flags);
u8* bunit_cmd_skip_group(u8* command);

/* comparison */
extern u32 g_bunit_comparison_display_thread_params;
extern s32 g_bunit_comparison_display_redraw_request;
extern s32 g_bunit_comparison_display_flags;
extern s32 g_bunit_comparison_display_offset_y;

/* editor */
/* Copy of the selected unit's 0x40 bytes from +0x30; only the leading
 * slot-state fields are read. */
extern battle_menu_status_panel_slot_state_t g_bunit_editor_unit_fields;
extern battle_image_location_t g_bunit_editor_numeric_geometry[];
extern battle_menu_status_panel_numeric_geometry_t g_bunit_editor_numeric_table;
extern RECT g_bunit_editor_numeric_entries_a;
extern RECT g_bunit_editor_numeric_entries_b;
extern RECT g_bunit_editor_numeric_entries_c;
extern RECT g_bunit_editor_numeric_entries_d;
extern RECT g_bunit_editor_numeric_entries_e;
extern RECT g_bunit_editor_numeric_entries_f;
extern u8 g_bunit_editor_numeric_texture[];
extern u8 g_bunit_editor_numeric_descriptor_a[];
extern u8 g_bunit_editor_numeric_descriptor_b[];
extern battle_menu_status_panel_numeric_buffer_t g_bunit_editor_numeric_state_a[2];
extern battle_menu_status_panel_numeric_buffer_t g_bunit_editor_numeric_state_b[2];
extern u8 g_bunit_editor_numeric_text_a[];
extern u8 g_bunit_editor_numeric_text_b[];

/* help */
extern u32 g_bunit_help_text_table[];
extern u8 g_bunit_help_text_state;
extern u16 g_bunit_help_cursor_anim_state[];
extern u8 g_bunit_help_cursor_initialized;

/* g_bunit_help_nodes_unit_browse[23].help_base and [24].help_base as their own symbols: as element
 * accesses GCC derives the second store and the later table address from one base register. */
extern s16 g_bunit_help_node_23_help_base;
extern s16 g_bunit_help_node_24_help_base;
extern u8 g_bunit_help_node_index;
extern u8 g_bunit_help_node_index_reload;
extern u8 g_bunit_help_node_index_reload_2;
extern help_navigation_record_t* g_bunit_help_nodes;
extern help_navigation_record_t g_bunit_help_nodes_banner_bottom[];
extern help_navigation_record_t g_bunit_help_nodes_banner_top[];

/* Same words as g_bunit_help_nodes/g_bunit_help_node_index; the target reloads them at each use
 * instead of reusing the cached copies, which separate symbols reproduce. */
extern help_navigation_record_t* g_bunit_help_nodes_reload;
extern help_navigation_record_t g_bunit_help_nodes_unit_browse[];
extern s32 g_bunit_help_screen_id;

/* job */
extern u32 g_bunit_job_current_jp;
extern s32 g_bunit_job_current_level;
extern s32 g_bunit_job_current_mastered;
extern u32 g_bunit_job_current_total_jp;
extern s16 g_bunit_job_ids[];
extern u8 g_bunit_job_list_initialized;
extern s8 g_bunit_job_menu_active;
extern s8 g_bunit_job_menu_phase;
extern u32 g_bunit_job_next_level_jp_requirement;
extern s16 g_bunit_job_selected_generic_index;
extern s16 g_bunit_job_selected_id;
extern s32 g_bunit_job_list_menu_script;
s32 bunit_job_build_unit_job_list(s32 unit_id, s16* job_ids);
void bunit_job_calculate_current_jp(s32 index);
s32 bunit_job_calculate_current_level(s32 index);
s32 bunit_job_calculate_current_mastered(s32 index);
void bunit_job_calculate_current_total_jp(s32 index);
u32 bunit_job_calculate_next_level_jp_requirement(s32 index);
s32 bunit_job_get_base(s16 unit_id);
u32 bunit_job_get_current_jp(void);
s32 bunit_job_get_current_level(void);
u32 bunit_job_get_current_total_jp(void);
s32 bunit_job_get_generic_index(s32 job_id);
u32 bunit_job_get_next_level_jp_requirement(void);
s32 bunit_job_is_current_mastered(void);
s32 bunit_job_is_special_monster(s32 job_id);
s32 bunit_job_find_first_for_skillset(s32 skillset_id);
u8 bunit_job_get_skillset(s32 job_id);

/* status */
extern u8 g_bunit_status_banner_enabled;
extern u32 g_bunit_status_display_thread_params;
extern s32 g_bunit_status_display_offset_y;
extern u32 g_bunit_status_display_flags;
extern s32 g_bunit_status_display_redraw_request;
extern s32 g_bunit_status_display_highlight;
extern u8 g_bunit_status_banner_at_bottom;
extern u8 g_bunit_status_banner_at_bottom_latch;
extern s8 g_bunit_status_banner_slide_step;

/* text */
/* Pen of bunit_text_render_glyph_to_4bpp_image: x in pixels (two per byte),
 * y in rows, and the image's row stride in halfwords. */
typedef struct bunit_text_image_position {
    u16 x;          /* 0x00 */
    s16 y;          /* 0x02 */
    s16 row_stride; /* 0x04 */
} bunit_text_image_position_t;
typedef char bunit_text_image_position_size_must_be_6[(sizeof(bunit_text_image_position_t) == 6) ? 1 : -1];

/* Ten-byte draw-number command (menu script opcodes 0x19-0x1B) executed by
 * bunit_menu_script_draw_formatted_number, which advances by `length`. Same layout as WORLD's
 * world_draw_number_command_t. */
typedef struct bunit_draw_number_command {
    u8 opcode;            /* 0x00 */
    u8 length;            /* 0x01 */
    u8 style;             /* 0x02 */
    u8 getter;            /* 0x03: g_bunit_menu_row_callbacks index */
    u8 width;             /* 0x04 */
    u8 spacing;           /* 0x05 */
    u8 alternate_palette; /* 0x06 */
    u8 x;                 /* 0x07 */
    u8 y;                 /* 0x08 */
    u8 value;             /* 0x09 */
} bunit_draw_number_command_t;
typedef char bunit_draw_number_command_size_must_be_0x0a[(sizeof(bunit_draw_number_command_t) == 0x0a) ? 1 : -1];

extern const char g_bunit_text_decimal_format[];
extern battle_menu_status_panel_glyph_t g_bunit_text_decimal_glyph;
extern s32 g_bunit_text_selection_id;
extern u8 g_bunit_numeric_editor_thread_params;
extern s32 g_bunit_numeric_editor_redraw_request;
extern s32 g_bunit_numeric_editor_highlight;
extern u16 g_bunit_text_digit_texture_page;
extern u16 g_bunit_text_clut_2_mode0;
extern u16 g_bunit_text_clut_2_mode1;
extern u16 g_bunit_text_clut_1_mode1;
extern u16 g_bunit_text_clut_1_mode0;
extern u16 g_bunit_text_clut_3_mode0;
extern u16 g_bunit_text_clut_3_mode1;
extern u16 g_bunit_text_clut_0_mode1;
extern u16 g_bunit_text_clut_0_mode0;
extern u16 g_bunit_text_metric_3;
extern u16 g_bunit_text_metric_2;
extern u16 g_bunit_text_metric_1;
extern u16 g_bunit_text_metric_0;
extern u16 g_bunit_text_metric_4;
extern u16 g_bunit_text_metric_5;
extern u8 g_bunit_text_layout_mode;

/* Text-section buffers: bunit_menu_run_reorder_list passes g_bunit_text_reorder_list_buffer
 * straight to bunit_text_concatenate_ids as its u8* output string and
 * g_bunit_text_menu_section as the text-section base; bunit_gfx_init_vram_and_start_fade
 * installs both addresses as u8* text-section pointers. */
extern u8 g_bunit_text_menu_section[];
extern u8 g_bunit_text_reorder_list_buffer[];
void bunit_run_numeric_editor_thread(void);

void bunit_text_render_decimal_entry_list(s32 pixels, battle_menu_status_panel_gauge_entry_t* entries,
    battle_menu_status_panel_text_position_t* out, s32 count);

void bunit_text_render_decimal_value(
    s32 value, s32 flags, void* pixels, battle_menu_status_panel_text_position_t* position);

s32 bunit_text_render_glyph_to_4bpp_image(
    s32 glyph_id, u8* image, const bunit_text_image_position_t* position, s32 style);

void bunit_text_render_signed_decimal_entries(s32 pixels, battle_menu_status_panel_gauge_entry_t* entries,
    battle_menu_status_panel_text_position_t* out, s32 count);

void bunit_text_set_palette_and_metrics(s32 mode);
void bunit_text_concatenate_ids(s32 text_table, u8* out, s16* list, s32 separate);
void bunit_text_render_id_rows_to_vram(s32 text_table, u16* text_ids, RECT* destination, s32 flags);

void bunit_text_render_ids_into_image(u8* image, battle_menu_text_image_bounds_t* rect, s32 unused, s32 max_chars,
    u8* font, s16* ids, s32 count, s16 glyph, s32 unused_flags);

const u8* bunit_text_skip_encoded_segments(const u8* data, s16 count);
void bunit_text_start_selection_thread(u8* menu_state);

/* thread */
extern battle_menu_status_panel_indicator_prims_t g_bunit_thread_indicator_packets[2][2];
extern u8 g_bunit_thread_status_snapshot[16];
s32 bunit_thread_is_running(s32 thread_id);
void bunit_thread_request_stop(s32 thread_id);
void bunit_thread_toggle_7(s32 enable);
void bunit_thread_toggle_8(s32 enable);
void bunit_thread_toggle_9_and_12(s32 enable);

/* cursor */
/* Rhombus-cursor trail, oldest point first; x == -1 marks an empty slot
 * (bunit_gfx_init_rhombus_cursor_tpages clears all eight). */
typedef struct bunit_cursor_trail_point {
    s16 x;
    s16 y;
} bunit_cursor_trail_point_t;

/* Screen rectangle, texture window and CLUT/texture page consumed by
 * bunit_gfx_enqueue_oriented_textured_quad. Every member is a halfword in the
 * writers; the reader narrows the texture members to their low byte. */
typedef struct bunit_oriented_quad {
    u16 x;     /* 0x00 */
    u16 y;     /* 0x02 */
    u16 w;     /* 0x04 */
    u16 h;     /* 0x06 */
    u16 u;     /* 0x08 */
    u16 v;     /* 0x0a */
    u16 uw;    /* 0x0c */
    u16 vh;    /* 0x0e */
    u16 clut;  /* 0x10 */
    u16 tpage; /* 0x12 */
} bunit_oriented_quad_t;
typedef char bunit_oriented_quad_size_must_be_0x14[(sizeof(bunit_oriented_quad_t) == 0x14) ? 1 : -1];

/* Oriented-quad descriptors for the two cursor halves. */
extern bunit_oriented_quad_t g_bunit_cursor_shadow_sprite;
extern bunit_oriented_quad_t g_bunit_cursor_sprite;
extern bunit_cursor_trail_point_t g_bunit_cursor_trail_points[8];
extern u8 g_bunit_cursor_trail_brightness[];

/* gfx */
typedef enum bunit_vertical_scroll_direction {
    BUNIT_VERTICAL_SCROLL_UP = -1,
    BUNIT_VERTICAL_SCROLL_NONE = 0,
    BUNIT_VERTICAL_SCROLL_DOWN = 1,
    BUNIT_VERTICAL_SCROLL_WAIT = 2,
} bunit_menu_vertical_scroll_direction_e;

/* One of BUNIT.OUT's two 0xec-byte frame buffers: 25 primitive-pool pointers
 * in libgpu primitive order, then the draw and display environments.
 * bunit_gfx_init_double_packet_buffers binds the pools of both buffers and
 * bunit_gfx_init_packet_pools initializes them; g_bunit_gfx_context points at the
 * active buffer. Pools whose packet type has no clean-room libgpu declaration
 * are typed by their packet size. */
typedef struct bunit_gfx_context {
    u32* otag;                            /* 0x00 */
    POLY_F3* flat_triangles;              /* 0x04 */
    POLY_FT3* textured_triangles;         /* 0x08 */
    POLY_F4* flat_quads;                  /* 0x0c */
    POLY_FT4* textured_quads;             /* 0x10 */
    POLY_G3* gouraud_triangles;           /* 0x14 */
    POLY_GT3* gouraud_textured_triangles; /* 0x18 */
    POLY_G4* gouraud_quads;               /* 0x1c */
    POLY_GT4* gouraud_textured_quads;     /* 0x20 */
    LINE_F2* flat_lines;                  /* 0x24 */
    u8 (*flat_lines_3)[0x18];             /* 0x28: LINE_F3 */
    u8 (*flat_lines_4)[0x1c];             /* 0x2c: LINE_F4 */
    LINE_G2* gouraud_lines;               /* 0x30 */
    u8 (*gouraud_lines_3)[0x20];          /* 0x34: LINE_G3 */
    u8 (*gouraud_lines_4)[0x28];          /* 0x38: LINE_G4 */
    TILE* tiles;                          /* 0x3c */
    u8 (*tiles_1)[0xc];                   /* 0x40: TILE_1 */
    u8 (*tiles_8)[0xc];                   /* 0x44: TILE_8 */
    u8 (*tiles_16)[0xc];                  /* 0x48: TILE_16 */
    SPRT* sprites;                        /* 0x4c */
    u8 (*sprites_8)[0x10];                /* 0x50: SPRT_8 */
    u8 (*sprites_16)[0x10];               /* 0x54: SPRT_16 */
    DR_MOVE* draw_moves;                  /* 0x58 */
    DR_AREA* draw_areas;                  /* 0x5c */
    DR_MODE* draw_modes;                  /* 0x60 */
    DRAWENV draw_env;                     /* 0x64 */
    DISPENV disp_env;                     /* 0xc0 */
    u8 unknown_d4[0x18];                  /* 0xd4 */
} bunit_gfx_context_t;
typedef char bunit_gfx_context_size_must_be_0xec[(sizeof(bunit_gfx_context_t) == 0xec) ? 1 : -1];
typedef char bunit_gfx_context_textured_quads_must_be_0x10
    [((unsigned long)&((bunit_gfx_context_t*)0)->textured_quads == 0x10) ? 1 : -1];
typedef char bunit_gfx_context_tiles_must_be_0x3c[((unsigned long)&((bunit_gfx_context_t*)0)->tiles == 0x3c) ? 1 : -1];
typedef char
    bunit_gfx_context_draw_modes_must_be_0x60[((unsigned long)&((bunit_gfx_context_t*)0)->draw_modes == 0x60) ? 1 : -1];
typedef char
    bunit_gfx_context_disp_env_must_be_0xc0[((unsigned long)&((bunit_gfx_context_t*)0)->disp_env == 0xc0) ? 1 : -1];

/* Screen rectangle, texture origin and CLUT/texture page consumed by
 * bunit_gfx_enqueue_gouraud_textured_quad; the texture extent is the screen
 * extent. */
typedef struct bunit_gouraud_rect {
    u16 x;     /* 0x00 */
    u16 y;     /* 0x02 */
    u16 w;     /* 0x04 */
    u16 h;     /* 0x06 */
    u16 u;     /* 0x08 */
    u16 v;     /* 0x0a */
    u16 clut;  /* 0x0c */
    u16 tpage; /* 0x0e */
} bunit_gouraud_rect_t;
typedef char bunit_gouraud_rect_size_must_be_0x10[(sizeof(bunit_gouraud_rect_t) == 0x10) ? 1 : -1];

/* Four explicit screen corners, their texture coordinates and the CLUT and
 * texture page, consumed by bunit_gfx_enqueue_textured_quad_corners. */
typedef struct bunit_sprite_desc {
    u16 x0;    /* 0x00 */
    u16 y0;    /* 0x02 */
    u16 x1;    /* 0x04 */
    u16 y1;    /* 0x06 */
    u16 x2;    /* 0x08 */
    u16 y2;    /* 0x0a */
    u16 x3;    /* 0x0c */
    u16 y3;    /* 0x0e */
    u16 u0;    /* 0x10 */
    u16 v0;    /* 0x12 */
    u16 u1;    /* 0x14 */
    u16 v1;    /* 0x16 */
    u16 u2;    /* 0x18 */
    u16 v2;    /* 0x1a */
    u16 u3;    /* 0x1c */
    u16 v3;    /* 0x1e */
    u16 clut;  /* 0x20 */
    u16 tpage; /* 0x22 */
} bunit_sprite_desc_t;
typedef char bunit_sprite_desc_size_must_be_0x24[(sizeof(bunit_sprite_desc_t) == 0x24) ? 1 : -1];

/* Paired GPU draw-area commands and their source rectangles. The builder at
 * 0x801bf204 writes both arrays with these exact contiguous offsets. */
typedef struct bunit_gfx_scaled_draw_area_pair {
    DR_AREA areas[2];
    RECT rects[2];
} bunit_gfx_scaled_draw_area_pair_t;
typedef char
    bunit_gfx_scaled_draw_area_pair_size_must_be_0x28[(sizeof(bunit_gfx_scaled_draw_area_pair_t) == 0x28) ? 1 : -1];

extern bunit_sprite_desc_t g_bunit_bouncing_cursor_quad;

/* Scratch descriptor for the blank cells of
 * bunit_cmd_draw_right_aligned_number_handler; only x/y are written, the rest
 * comes from its static initializer. */
extern bunit_oriented_quad_t g_bunit_gfx_blank_cell_rect;
extern bunit_gfx_context_t* g_bunit_gfx_context;
extern u8 g_bunit_gfx_decreasing_fade_active;
extern RECT g_bunit_gfx_draw_area;
extern const u16 g_bunit_gfx_draw_area_scale_percent[12];
extern u16 g_bunit_gfx_draw_move_count;
extern u8 g_bunit_gfx_fade_color[3];
extern u8 g_bunit_gfx_fade_color_b;
extern u8 g_bunit_gfx_fade_color_g;
extern s16 g_bunit_gfx_fade_intensity;
extern RECT g_bunit_gfx_fade_rect[];
extern u8 g_bunit_gfx_increasing_fade_active;
extern u16 g_bunit_gfx_otag_count;
extern s16 g_bunit_gfx_otag_index;
extern u16 g_bunit_gfx_poly_ft4_count;
extern u16 g_bunit_gfx_poly_gt4_count;
extern s16 g_bunit_gfx_portrait_origin[];
extern u8 g_bunit_gfx_portrait_transition_offsets[];
extern s16 g_bunit_gfx_semitrans_enabled;
extern u16 g_bunit_gfx_texture_page;
extern RECT g_bunit_gfx_draw_area_template;
extern s16 g_bunit_gfx_falloff_focus_x;
extern s16 g_bunit_gfx_falloff_focus_y;
extern s16 g_bunit_gfx_window_zoom_percent[];
extern s16 g_bunit_gfx_draw_offset_y;
extern u16 g_bunit_gfx_poly_g4_count;
extern u16 g_bunit_gfx_drawenv_y;
extern u16 g_bunit_gfx_draw_area_count;
extern u16 g_bunit_gfx_draw_mode_count;
extern u16 g_bunit_gfx_tile_count;

/* Base of the two frame buffers; g_bunit_gfx_context alternates between them. */
extern bunit_gfx_context_t* g_bunit_gfx_context_base;
extern u16 g_bunit_gfx_background_clut;
extern u16 g_bunit_gfx_background_tile_layout[];
extern u16 g_bunit_gfx_clut_id;
extern s16 g_bunit_gfx_line_f2_count;
extern s16 g_bunit_gfx_line_f3_count;
extern s16 g_bunit_gfx_line_f4_count;
extern s16 g_bunit_gfx_line_g2_count;
extern s16 g_bunit_gfx_line_g3_count;
extern s16 g_bunit_gfx_line_g4_count;
extern s8 g_bunit_gfx_otag_index_locked;
extern s16 g_bunit_gfx_poly_f3_count;
extern s16 g_bunit_gfx_poly_f4_count;
extern s16 g_bunit_gfx_poly_ft3_count;
extern s16 g_bunit_gfx_poly_g3_count;
extern s16 g_bunit_gfx_poly_gt3_count;
extern s16 g_bunit_gfx_sprite_16_count;
extern s16 g_bunit_gfx_sprite_8_count;
extern u8 g_bunit_gfx_sprite_color[3];
extern s16 g_bunit_gfx_sprite_count;
extern s16 g_bunit_gfx_tile_16_count;
extern s16 g_bunit_gfx_tile_1_count;
extern s16 g_bunit_gfx_tile_8_count;
extern s8 g_bunit_gfx_transition_frame;
extern u8 g_bunit_sprite_cell_sizes[][2];
void bunit_gfx_init_packet_pools(bunit_gfx_context_t* buffer);

void bunit_gfx_build_scaled_draw_area_packets(
    bunit_gfx_scaled_draw_area_pair_t* packet, const RECT* source, s32 scale_index, s32 lower_half, const s16* offset);

void bunit_gfx_build_portrait_poly_ft4(s32 portrait_id, POLY_FT4* poly);
s32 bunit_gfx_calculate_distance_falloff(s32 x, s32 y, s32 base_value, s32 minimum);
void bunit_gfx_clip_portrait_poly_from_left(POLY_FT4* poly, s32 amount);
void bunit_gfx_clip_portrait_poly_from_right(POLY_FT4* poly, s32 amount);

void bunit_gfx_enqueue_textured_quad(
    const RECT* rect, s32 u, s32 v, const u8* color, s32 semitrans, u16 texture_page, u16 clut, s32 otag_index);

void bunit_gfx_enqueue_gouraud_textured_quad(
    const bunit_gouraud_rect_t* rect, const u8* color, s32 semitrans, s32 otag_index);

void bunit_gfx_enqueue_oriented_textured_quad(
    const bunit_oriented_quad_t* descriptor, const u8* color, s32 orientation, s32 semitrans, s32 otag_index);

void bunit_gfx_enqueue_textured_quad_corners(bunit_sprite_desc_t* src, u8* color, s32 semi, s32 idx);

void bunit_gfx_enqueue_textured_quad_list(const battle_menu_sprite_cell_t* quads, const u8* color, s32 semitrans,
    u16 texture_page, u16 clut, s32 otag_index, s32 count);

void bunit_gfx_enqueue_translucent_tile(RECT* rect, u8* color, u8 semi, s32 idx);
s32 bunit_gfx_get_fade_state(void);
void bunit_gfx_init_menu_tile_and_line_primitives(battle_menu_status_panel_buffer_t* menu);
void bunit_gfx_init_rhombus_cursor_tpages(void);
void bunit_gfx_set_clut_rect_from_id(RECT* rect, s32 clut_id);
void bunit_gfx_set_transition_frame(s32 frame);
void bunit_gfx_start_decreasing_fade(void);
void bunit_gfx_start_increasing_fade(void);
s32 bunit_gfx_update_increasing_fade(void);

void bunit_gfx_apply_menu_palette_for_mode(
    world_menu_palette_primitives_t* primitives, const battle_menu_status_panel_frame_config_t* thread);

void bunit_gfx_build_portrait_transition_primitives(const RECT* texture_rect, s32* transition,
    const s32* first_portrait, const s32* second_portrait, u8* image, POLY_FT4* poly, s32 direction);

void bunit_gfx_draw_falloff_shaded_rows(s32 y);
void bunit_gfx_draw_unit_sprite_and_name_plate(s16 unit, s32 x, s32 y);
void bunit_gfx_enqueue_draw_area(RECT* rect, s32 idx);
void bunit_gfx_enqueue_draw_mode(s32 dfe, s32 dtd, s32 tpage, RECT* tw, s32 idx);
void bunit_gfx_enqueue_draw_move(const RECT* rect, s32 u, s32 v, s32 otag_index);
void bunit_gfx_enqueue_gouraud_quad(u16* pts, u8* color, s32 semi, s32 idx);
u8* bunit_gfx_get_draw_color(void);
void bunit_gfx_init_double_packet_buffers();
void bunit_gfx_init_scaled_draw_area_packets(u8* primitive);
void bunit_gfx_init_vram_and_start_fade(void);
void bunit_gfx_load_image_and_wait(RECT* rect, u32* data);
void bunit_gfx_set_draw_color(const u8* color);
void bunit_gfx_set_otag_index(s16 value);
void bunit_gfx_store_image_and_wait(RECT* rect, u32* data);
void bunit_gfx_submit_frame_and_swap_buffers(s32 first_otag, s32 draw_otag);
s32 bunit_gfx_update_decreasing_fade(void);
void bunit_gfx_update_rhombus_cursor_trail(bunit_cursor_trail_point_t* target);

/* input */
extern u8 g_bunit_input_activation_timer;
extern u32 g_bunit_input_current;
extern u32 g_bunit_input_event;
extern u32 g_bunit_input_initial_repeat_counter;
extern u16 g_bunit_input_latched_button;
extern u8 g_bunit_input_lock_timer;
extern u8 g_bunit_input_page_scroll_disabled;
extern u16 g_bunit_input_page_scroll_hold_frames;
extern u16 g_bunit_input_previous;
extern u32 g_bunit_input_repeat_counter;
extern volatile u32 g_bunit_input_repeat_counters[16];
extern u32 g_bunit_input_secondary_repeat_counter;
extern u32 g_bunit_previous_input;
extern u32* g_bunit_input_controller;
extern u32 g_bunit_input_primary_repeat;
extern u32 g_bunit_input_secondary_repeat;
void bunit_input_clear_state(void);
s32 bunit_input_get_lock_timer(void);
bunit_menu_vertical_scroll_direction_e bunit_input_read_page_scroll_direction(void);
void bunit_input_update_controller(void);
s32 bunit_input_read_pad1_unless_locked(void);
void bunit_input_update_event_state(void* state, u32 input, s16 frame_arg);
void bunit_input_update_lock_timer(void);

/* menu */
/*
 * Row-value getters for the BUNIT menu command stream: a 16-entry table at
 * 0x801ecb70 indexed by a command byte.
 *
 * Different menus install different signatures here --
 * bunit_run_ability_list_menu stores predicates, bunit_menu_init_unit_list
 * stores two-argument stat getters, and the row-sprite handler expects a
 * pointer return -- so callers cast to the signature they actually invoke.
 */
typedef s32 (*bunit_menu_row_callback_t)(s32 row);

/*
 * One remembered list position, 6 bytes per entry, 14 entries at 0x801eb22c.
 * bunit_menu_set_selection_record stores the cursor index, the scroll base and
 * the masked id of the entry under the cursor; bunit_menu_get_selection_record
 * reads them back and re-finds entry_id in the live list when the remembered
 * index no longer points at the same entry.
 */
typedef struct bunit_menu_selection_record {
    s16 selected_index;    /* 0x00 */
    s16 scroll_base_index; /* 0x02 */
    s16 entry_id;          /* 0x04: list entry id, low 10 bits */
} bunit_menu_selection_record_t;

enum { BUNIT_MENU_SELECTION_RECORD_COUNT = 14 };

extern u8 g_bunit_menu_cursor_forward_offset_bytes[];
extern u8 g_bunit_menu_cursor_reverse_offset_bytes[];
extern s32 g_bunit_menu_cursor_row;
extern s8 g_bunit_menu_event_speed;
extern s32 g_bunit_menu_indicator_brightness[2];
extern s32 g_bunit_menu_indicator_fade_work[2];
extern s32 g_bunit_menu_indicator_packet_index[2];
extern s32 g_bunit_menu_indicator_state[2];
extern s16 g_bunit_menu_list_visible_rows;
extern u16 g_bunit_menu_list_vram_width;
extern s16 g_bunit_menu_primary_result;
extern s16 g_bunit_menu_reorder_list_entries[];
extern s16 g_bunit_menu_reorder_list_index;
extern u8 g_bunit_menu_reorder_list_initialized;
extern s16 g_bunit_menu_reorder_result;
extern bunit_menu_row_callback_t g_bunit_menu_row_callbacks[];
extern s16 g_bunit_menu_scroll_base_index;
extern s16 g_bunit_menu_scroll_list_active;
extern s16 g_bunit_menu_scroll_pixel_offset;
extern bunit_menu_selection_record_t g_bunit_menu_selection_records[];
extern s16 g_bunit_menu_selection_values[];
extern u8 g_bunit_menu_thread_running;
extern s32 g_bunit_menu_current_menu;
extern s16 g_bunit_menu_reorder_list_redraw_request;
extern world_menu_entry_t g_bunit_menu_reorder_list_desc[];
extern u16 g_bunit_menu_icon_texture_page;
extern u16 g_bunit_menu_cursor_mode0_foreground_clut;
extern u16 g_bunit_menu_cursor_mode0_background_clut;
extern u16 g_bunit_menu_cursor_mode1_foreground_clut;
extern u16 g_bunit_menu_cursor_mode1_background_clut;

/* Idle and active vertical bounce curves of (frame threshold, pixel offset)
 * byte pairs, shared by the animated and bouncing cursor renderers. */
extern s8 g_bunit_menu_cursor_forward_offset_keyframes[];
extern s8 g_bunit_menu_cursor_reverse_offset_keyframes[];
extern s16 g_bunit_menu_list_row_group_count;
extern s8 g_bunit_menu_list_redraw_pending; /* read with lb (bunit_cmd_run_scrollable_list_handler) */
extern u8 g_bunit_menu_list_glyph_width;
extern u8 g_bunit_menu_list_text_columns;
extern s8 g_bunit_menu_list_scroll_direction; /* read with lb */
extern u16 g_bunit_menu_button_clut;

/* Signed 4.12 x/y scale pair per sprite entry; a negative scale flips the quad. */
extern s16 g_bunit_menu_button_piece_scales[][2];
extern u8 g_bunit_menu_button_piece_starts[];

/* Per-slot blink phase, and the half-open span of sprite entries the slot draws. */
extern s8 g_bunit_menu_button_press_timers[];
extern u32 g_bunit_menu_input_active_mask;
extern u16 g_bunit_menu_input_mask;
extern u32 g_bunit_menu_input_repeat_mask;
extern u16 g_bunit_menu_list_cursor_anim_state[];
extern u16* g_bunit_menu_list_entries;
extern u16 g_bunit_menu_list_entry_count;
extern u16 g_bunit_menu_list_row_height;
extern u16 g_bunit_menu_list_scroll_entry_ids[];
extern u16 g_bunit_menu_list_scroll_progress;
extern s16 g_bunit_menu_list_selected_index;
extern u16* g_bunit_menu_list_text_table;
extern s16 g_bunit_menu_window_right_x;
bunit_draw_number_command_t* bunit_menu_script_draw_formatted_number(bunit_draw_number_command_t* command);
void bunit_menu_clear_selection_record(s32 index);
void bunit_menu_clear_selection_records(void);
void bunit_menu_draw_scrollable_list(u8* script);
void bunit_menu_draw_thread_status_indicators(void);
s32 bunit_menu_get_event_speed(void);
void bunit_menu_get_selection_record(s32 index, s16* out_index, s16* out_b, s16* list);
s32 bunit_menu_is_unit_list_row_visible(s32 page_offset);
s32 bunit_menu_is_unit_list_scroll_needed(void);
void bunit_menu_reset_results(void);
s32 bunit_menu_run_descriptor_thread(s32 id, world_menu_entry_t* desc);
s32 bunit_menu_run_job_list(void);
void bunit_menu_run_reorder_list(void);
void bunit_menu_scroll_list_by_page(bunit_menu_vertical_scroll_direction_e direction, u8* script);
void bunit_menu_set_event_speed(s32 speed);
s32 bunit_menu_update_wrapped_grid_selection(s32 step, s32 count, s32 max, s32 current, s32 input);
void bunit_menu_dispatch_with_override(s32 menu_id, s32 state, s32 override);
void bunit_menu_draw_pressable_button(s16 slot, s16 x, s32 y, s16 pressed, s16 busy, u16 sound_id);
void bunit_menu_init_scrollable_list(s32 entries, s32 selected_index, s16 scroll_base_index, s32 text_table);
void bunit_menu_init_scrollable_list_core(s16* entries, s32 selected_index, s32 text_table);
void bunit_menu_init_unit_list(void);
s32 bunit_menu_is_active_or_transitioning(void);
s32 bunit_menu_run_unit_list_frame(void);
void bunit_menu_set_selection_record(s32 index, s32 field0, s32 field2, u16* lookup_table);
s32 bunit_menu_set_selection_value(u8 index, s16 value);
void bunit_menu_update_and_draw_animated_cursor(u16* pos, u16* state, s32 mode);
void bunit_menu_update_and_draw_bouncing_cursor(u16* pos, u16* state, s32 mode);
void bunit_menu_update_event_thread_state(void);

void bunit_menu_update_horizontal_selection_and_mark_change(
    u16 entry_count, u8 selection_index, s32 input_mask, u8 changed_state);

void bunit_menu_update_job(void);
void bunit_menu_update_node_cursor(s16 mode);
void bunit_menu_update_unit_browser(void);

s32 bunit_menu_update_unit_grid(s16 allow_banner_toggle, s16 allow_browse, s16 show_stat_panel, s16 allow_stat_cycle,
    u16 stat_mode, u16 buttons, s16 locked);

void bunit_menu_update_unit_list_scroll(s8* step, s8* offset);
s16 bunit_menu_update_wrapped_horizontal_selection(u16 entry_count, u8 selection_index, u16 input_mask);

/* unit */
typedef enum bunit_unit_stat {
    BUNIT_UNIT_STAT_HP = 0,
    BUNIT_UNIT_STAT_MP = 1,
    BUNIT_UNIT_STAT_CT = 2,
} bunit_unit_stat_e;

/*
 * BUNIT.OUT unit-data record. This is the formation/menu representation,
 * distinct from party_data_t, entd_unit_t, and battle_stats_t. Unnamed fields
 * remain padding until their uses establish a meaning.
 */
typedef struct bunit_unit_data {
    s16 level;      /* 0x00; getters at 0x801c4054..0x801c4163 read level, experience, brave and faith */
    s16 team_kind;  /* 0x02 */
    s16 list_index; /* 0x04 */
    s16 unit_count; /* 0x06; shown units plus eggs in each billboard record */
    s16 experience; /* 0x08 */
    s16 entd_slot;  /* 0x0a */
    s16 current_hp;
    u8 _pad0e[2];
    s16 max_hp;
    s16 current_mp;
    u8 _pad14[2];
    s16 max_mp;
    s16 current_ct;
    u8 _pad1a[2];
    s16 max_ct;          /* 0x1c */
    s16 formation_index; /* 0x1e */
    u8 _pad20[2];
    s16 entd_slot_22;        /* 0x22 */
    s16 monster_base_job_id; /* 0x24; used when character_identity is 0x82 */
    s16 brave;               /* 0x26 */
    s16 faith;               /* 0x28 */
    s16 zodiac;              /* 0x2a */
    s16 roster_id;           /* 0x2C; searched by bunit_unit_find_index_by_roster_id */
    u8 _pad2e[2];
    s16 move;                     /* 0x30 */
    s16 speed;                    /* 0x32 */
    s16 jump;                     /* 0x34 */
    s16 right_weapon_power;       /* 0x36 */
    s16 left_weapon_power;        /* 0x38 */
    s16 right_weapon_evade;       /* 0x3a */
    s16 left_weapon_evade;        /* 0x3c */
    s16 uses_monster_skillset;    /* 0x3e; nonzero restricts the job list to its base entry */
    s16 two_handed;               /* 0x40 */
    s16 physical_attack;          /* 0x42 */
    s16 physical_class_evade;     /* 0x44 */
    s16 physical_shield_evade;    /* 0x46 */
    s16 physical_accessory_evade; /* 0x48 */
    u8 _pad4a[2];
    s16 magical_attack;          /* 0x4c */
    s16 magical_class_evade;     /* 0x4e */
    s16 magical_shield_evade;    /* 0x50 */
    s16 magical_accessory_evade; /* 0x52 */
    s16 equipment[5];            /* 0x54 */
    s16 abilities[5];            /* 0x5e; primary skillset, secondary, reaction, support, movement (filled by
                                  * bunit_panel_copy_unit_data_to_billboard; bunit_create_ability_list reads [0]) */
    u8 _pad68[8];
    u8 gender_flags;          /* 0x70; Bard/Dancer restrictions */
    u8 misc_unit_id;          /* 0x71 */
    u8 initial_team_flags;    /* 0x72 */
    u8 formation_order_key;   /* 0x73; bit 0x40 marks an encoded roster ID */
    u8 character_identity;    /* 0x74; special character, generic male/female/monster */
    u8 unlocked_jobs[3];      /* 0x75; bit reader consumes Squire through Mime */
    u8 learned_abilities[57]; /* 0x78 */
    u8 job_levels[10];        /* 0xb1; two packed job-level nibbles per byte */
    u8 _padbb;
    u16 job_points[20];       /* 0xbc; current JP by generic job */
    u16 total_job_points[20]; /* 0xe4; cumulative JP by generic job */
} bunit_unit_data_t;
typedef char bunit_unit_data_size_must_be_0x10c[(sizeof(bunit_unit_data_t) == 0x10c) ? 1 : -1];
typedef char bunit_unit_data_character_identity_must_be_0x74
    [((unsigned long)&((bunit_unit_data_t*)0)->character_identity == 0x74) ? 1 : -1];
typedef char
    bunit_unit_data_gender_flags_must_be_0x70[((unsigned long)&((bunit_unit_data_t*)0)->gender_flags == 0x70) ? 1 : -1];
typedef char bunit_unit_data_unlocked_jobs_must_be_0x75[((unsigned long)&((bunit_unit_data_t*)0)->unlocked_jobs == 0x75)
        ? 1
        : -1];
typedef char bunit_unit_data_uses_monster_skillset_must_be_0x3e
    [((unsigned long)&((bunit_unit_data_t*)0)->uses_monster_skillset == 0x3e) ? 1 : -1];
typedef char
    bunit_unit_data_experience_must_be_0x08[((unsigned long)&((bunit_unit_data_t*)0)->experience == 0x08) ? 1 : -1];
typedef char
    bunit_unit_data_current_hp_must_be_0x0c[((unsigned long)&((bunit_unit_data_t*)0)->current_hp == 0x0C) ? 1 : -1];
typedef char bunit_unit_data_max_hp_must_be_0x10[((unsigned long)&((bunit_unit_data_t*)0)->max_hp == 0x10) ? 1 : -1];
typedef char
    bunit_unit_data_current_mp_must_be_0x12[((unsigned long)&((bunit_unit_data_t*)0)->current_mp == 0x12) ? 1 : -1];
typedef char bunit_unit_data_max_mp_must_be_0x16[((unsigned long)&((bunit_unit_data_t*)0)->max_mp == 0x16) ? 1 : -1];
typedef char
    bunit_unit_data_current_ct_must_be_0x18[((unsigned long)&((bunit_unit_data_t*)0)->current_ct == 0x18) ? 1 : -1];
typedef char bunit_unit_data_brave_must_be_0x26[((unsigned long)&((bunit_unit_data_t*)0)->brave == 0x26) ? 1 : -1];
typedef char bunit_unit_data_faith_must_be_0x28[((unsigned long)&((bunit_unit_data_t*)0)->faith == 0x28) ? 1 : -1];
typedef char
    bunit_unit_data_roster_id_must_be_0x2c[((unsigned long)&((bunit_unit_data_t*)0)->roster_id == 0x2C) ? 1 : -1];
typedef char bunit_unit_data_formation_order_key_must_be_0x73
    [((unsigned long)&((bunit_unit_data_t*)0)->formation_order_key == 0x73) ? 1 : -1];

extern s16 g_bunit_unit_count;
extern s16 g_bunit_unit_current_stat_value;
extern bunit_unit_data_t* g_bunit_unit_data[];
extern u8 g_bunit_unit_list_count;
extern u8 g_bunit_unit_list_index_map[];
extern s8 g_bunit_unit_list_page_offset;
extern s8 g_bunit_unit_list_scroll_step;
extern s16 g_bunit_unit_max_stat_value;
extern s8 g_bunit_unit_reorderable_indices[];
extern s16 g_bunit_unit_selected_index;
extern u8 g_bunit_unit_stat_mode;
extern u16 g_bunit_unit_comparison_index;
extern u8 g_bunit_unit_browse_enabled;
extern u8 g_bunit_unit_grid_selected_cell;
extern u8 g_bunit_unit_list_initialized;
extern u8 g_bunit_unit_list_triangle_menu_open;
extern s8 g_bunit_unit_reorderable_count;
extern u8 g_bunit_unit_records[];
extern s8 g_bunit_unit_dot_glow_direction;
extern u8 g_bunit_unit_dot_glow_timer;
extern bunit_oriented_quad_t g_bunit_unit_dot_sprite;
extern bunit_draw_number_command_t g_bunit_unit_grid_brave_number;
extern bunit_draw_number_command_t g_bunit_unit_grid_brave_number_wide;
extern u16 g_bunit_unit_grid_ct_gauge_clut;
extern u8 g_bunit_unit_grid_ct_gauge_colors[];
extern bunit_cursor_trail_point_t g_bunit_unit_grid_cursor_target;
extern u8 g_bunit_unit_grid_dimmed_color[];
extern bunit_draw_number_command_t g_bunit_unit_grid_experience_number;
extern bunit_draw_number_command_t g_bunit_unit_grid_faith_number;
extern bunit_draw_number_command_t g_bunit_unit_grid_faith_number_wide;
extern s8 g_bunit_unit_grid_gauge_offsets[];
extern u16 g_bunit_unit_grid_hp_gauge_clut;
extern u8 g_bunit_unit_grid_hp_gauge_colors[];
extern battle_menu_sprite_cell_t g_bunit_unit_grid_label_tiles[];
extern battle_menu_sprite_cell_t g_bunit_unit_grid_label_tiles_bottom[];
extern bunit_draw_number_command_t g_bunit_unit_grid_level_number;
extern bunit_draw_number_command_t g_bunit_unit_grid_max_stat_number;
extern u16 g_bunit_unit_grid_mp_gauge_clut;
extern u8 g_bunit_unit_grid_mp_gauge_colors[];
extern u8 g_bunit_unit_grid_normal_color[];
extern u8 g_bunit_unit_grid_stat_bar_script_bottom[];
extern u8 g_bunit_unit_grid_stat_bar_script_top[];
extern bunit_draw_number_command_t g_bunit_unit_grid_stat_number;
extern world_menu_entry_t g_bunit_unit_list_triangle_menu;
extern bunit_oriented_quad_t g_bunit_unit_shadow_sprite;

/* Halfword field index, per sort key, into the 0x10C-byte formation record
 * built by bunit_panel_build_unit_billboard_list. The name is provisional. */
extern s8 g_bunit_unit_sort_field_indices[];
void bunit_unit_sort_index_list_by_order_keys(s32 a);
void bunit_unit_build_list_index_map(void);
s32 bunit_unit_build_reorderable_index_list(void);
void bunit_unit_copy_orders_to_reorder_list(s32 unused, s16* entries);
void bunit_unit_copy_selected_data(void);
s32 bunit_unit_find_index_by_roster_id(s16 roster_id);
s16 bunit_unit_get_current_stat(s32 unit_index, s32 fallback);
s32 bunit_unit_get_level(s32 unit_id);
s32 bunit_unit_get_experience(s32 unit_id);
s32 bunit_unit_get_brave(s32 unit_id);
s32 bunit_unit_get_faith(s32 unit_id);
s16 bunit_unit_get_max_stat(s32 unit_index, s32 fallback);
void bunit_render_unit_status_panel_thread(void);
void bunit_unit_copy_reorder_list_to_orders(s32 unused, const u8* entries);
s32 bunit_unit_is_two_handing_weapon(s16* slots, s32 allow_secondary);

/* panel */
extern battle_menu_status_panel_editor_state_t g_bunit_panel_selected_billboard;
extern battle_menu_status_panel_editor_state_t g_bunit_panel_comparison_billboard;
extern s16 g_bunit_panel_selected_unit_data[];
extern s16 g_bunit_panel_comparison_unit_data[];
extern battle_menu_status_panel_editor_packet_t g_bunit_panel_comparison_editor_packets[2];
extern u8 g_bunit_panel_comparison_large_number_image[];
extern u8 g_bunit_panel_comparison_name_image[];
extern u8 g_bunit_panel_comparison_number_image[];
extern battle_menu_status_panel_numeric_entry_t g_bunit_panel_comparison_numeric_entries[];
extern battle_menu_status_panel_packet_t g_bunit_panel_comparison_packets[2];
extern u8 g_bunit_panel_comparison_portrait_image[];
extern RECT g_bunit_panel_comparison_portrait_rect;
extern u8 g_bunit_panel_editor_label_cells[];
extern u16 g_bunit_panel_editor_label_cluts[][2];
extern u8 g_bunit_panel_editor_mode_cell[];
extern u8 g_bunit_panel_editor_mode_cells[];
extern u8 g_bunit_panel_editor_value_cells[];
extern u16 g_bunit_panel_editor_value_cluts[][2];
extern u8 g_bunit_panel_frame_rect[];
extern CVECTOR g_bunit_panel_gauge_bar_colors[];
extern battle_image_location_t g_bunit_panel_item_icon_texture[];
extern u8 g_bunit_panel_portrait_cell[];
extern battle_menu_status_panel_editor_packet_t g_bunit_panel_selected_editor_packets[2];
extern u8 g_bunit_panel_selected_large_number_image[];
extern u8 g_bunit_panel_selected_name_image[];
extern u8 g_bunit_panel_selected_number_image[];
extern battle_menu_status_panel_numeric_entry_t g_bunit_panel_selected_numeric_entries[];
extern battle_menu_status_panel_packet_t g_bunit_panel_selected_packets[2];
extern u8 g_bunit_panel_selected_portrait_image[];
extern RECT g_bunit_panel_selected_portrait_rect;
extern s16 g_bunit_panel_slide_down_y[];
extern s16 g_bunit_panel_slide_up_y[];
extern u8 g_bunit_panel_sprite_cells[];

void bunit_panel_set_primitive_colors(
    battle_menu_status_panel_buffer_t* primitives, const battle_menu_status_panel_frame_config_t* state);

void bunit_panel_run_character_status_thread(void);
s32 bunit_panel_build_unit_billboard_list(s32 mode, s32 unit_data, s32 sort_mode);
void bunit_panel_copy_unit_data_to_billboard(struct battle_stats* unit, bunit_unit_data_t* record, s32 unused_slot);

/* other */
enum {
    BUNIT_FORMATION_ORDER_KEY_ENCODED_ROSTER_ID = 0x40,
};

extern u8 g_bunit_selected_roster_id;
extern u8 g_bunit_saved_unit_browse_enabled;
extern u8 g_bunit_saved_status_banner_enabled;
extern s16 g_bunit_frame_arg;
extern s16 g_bunit_shown_unit_indices[];
extern u8 g_bunit_sound_queued_effect_id;
s32 bunit_entrypoint(void);

#endif
