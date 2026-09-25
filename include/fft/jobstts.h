#ifndef FFT_JOBSTTS_H
#define FFT_JOBSTTS_H

#include "fft/battle.h"
#include "fft/battle_text.h"
#include "fft/geometry.h"
#include "psx/gpu.h"
#include "psx/types.h"

extern world_menu_window_command_t g_jobstts_cmd_window_interior_command;

typedef enum jobstts_page_scroll_direction {
    JOBSTTS_PAGE_SCROLL_UP = -1,
    JOBSTTS_PAGE_SCROLL_NONE = 0,
    JOBSTTS_PAGE_SCROLL_DOWN = 1,
} jobstts_menu_page_scroll_direction_e;

typedef enum jobstts_ability_category {
    JOBSTTS_ABILITY_CATEGORY_ACTION = 0,
    JOBSTTS_ABILITY_CATEGORY_REACTION = 1,
    JOBSTTS_ABILITY_CATEGORY_SUPPORT = 2,
    JOBSTTS_ABILITY_CATEGORY_MOVEMENT = 3,
    JOBSTTS_ABILITY_CATEGORY_ALL = 15,
} jobstts_ability_category_e;

enum {
    JOBSTTS_LIST_COMMAND_VISIBLE_ROWS_INDEX = 6,
};

/* Render-command stream callbacks: every handler consumes and returns a
 * command pointer; every condition is indexed by a row or literal operand. */
typedef u8* (*jobstts_command_handler_t)(u8*);
typedef s32 (*jobstts_condition_t)(s32);

typedef struct jobstts_ability_selection_state {
    s16 selected_index;
    s16 scroll_index;
    u16 selected_ability_id;
} jobstts_ability_selection_state_t;

typedef char assert_jobstts_ability_selection_state_size[sizeof(jobstts_ability_selection_state_t) == 6 ? 1 : -1];

typedef struct jobstts_text_image_position {
    u16 x;
    s16 y;
    s16 row_stride;
} jobstts_text_image_position_t;

/* Job and ability data copied into JOBSTTS.OUT for the selected unit. */
typedef struct jobstts_unit_job_data {
    s16 job_id;               /* 0x00 */
    s16 entd_slot;            /* 0x02 */
    u16 primary_skillset;     /* 0x04 */
    u8 unit_flags;            /* 0x06 */
    u8 character_identity;    /* 0x07 */
    u8 unlocked_jobs[3];      /* 0x08 */
    u8 learned_abilities[57]; /* 0x0B */
    u8 job_levels[10];        /* 0x44 */
    u16 job_points[20];       /* 0x4E */
    u16 total_job_points[20]; /* 0x76 */
} jobstts_unit_job_data_t;

typedef char jobstts_unit_job_data_size_must_be_0x9e[(sizeof(jobstts_unit_job_data_t) == 0x9E) ? 1 : -1];

/* Per-buffer primitive-pool pointers used by JOBSTTS.OUT's renderer. */
typedef struct jobstts_gfx_context {
    u32* otag;                /* 0x00 */
    POLY_F3* poly_f3;         /* 0x04 */
    u8* poly_ft3;             /* 0x08; 0x20 stride */
    POLY_F4* poly_f4;         /* 0x0C */
    POLY_FT4* textured_quads; /* 0x10 */
    u8* poly_g3;              /* 0x14; 0x1C stride */
    u8* poly_gt3;             /* 0x18; 0x28 stride */
    POLY_G4* poly_g4;         /* 0x1C */
    POLY_GT4* poly_gt4;       /* 0x20 */
    LINE_F2* line_f2;         /* 0x24 */
    u8* line_f3;              /* 0x28; 0x18 stride */
    u8* line_f4;              /* 0x2C; 0x1C stride */
    LINE_G2* line_g2;         /* 0x30 */
    u8* line_g3;              /* 0x34; 0x20 stride */
    u8* line_g4;              /* 0x38; 0x28 stride */
    TILE* tiles;              /* 0x3C */
    u8* tiles_16;             /* 0x40; 0xC stride */
    u8* tiles_8;              /* 0x44; 0xC stride */
    u8* tiles_1;              /* 0x48; 0xC stride */
    SPRT* sprites;            /* 0x4C */
    u8* sprites_16;           /* 0x50; 0x10 stride */
    u8* sprites_8;            /* 0x54; 0x10 stride */
    DR_MOVE* draw_moves;      /* 0x58 */
    DR_AREA* draw_areas;      /* 0x5C */
    u8 pad_60[0x94];          /* 0x60 */
} jobstts_gfx_context_t;

typedef char jobstts_gfx_context_size_must_be_0xf4[(sizeof(jobstts_gfx_context_t) == 0xF4) ? 1 : -1];

extern s16 g_jobstts_ability_category;
extern s16 g_jobstts_ability_entries[];
extern u8* g_jobstts_ability_selected_specific;
extern u8* g_jobstts_ability_selected_specific_arg_alias;
extern s32 g_jobstts_ability_type;
extern u8 g_jobstts_bits_primary_reset;
extern const u8* g_jobstts_bits_primary_source;
extern s32 g_jobstts_bits_reader_1_index;
extern s32 g_jobstts_bits_reader_2_index;
extern u8 g_jobstts_bits_secondary_reset;
extern const u8* g_jobstts_bits_secondary_source;
extern jobstts_condition_t g_jobstts_cmd_conditions[];
extern jobstts_command_handler_t g_jobstts_cmd_handlers[];
extern u16 g_jobstts_gfx_background_clut_id;
extern u16 g_jobstts_gfx_background_texture_page;
extern u16 g_jobstts_gfx_clut_id;
extern jobstts_gfx_context_t* g_jobstts_gfx_context;
extern jobstts_gfx_context_t* g_jobstts_gfx_context_base;
extern RECT g_jobstts_gfx_draw_area;
extern u16 g_jobstts_gfx_draw_area_count;
extern u16 g_jobstts_gfx_draw_move_count;
extern u16 g_jobstts_gfx_drawenv_y;
extern s16 g_jobstts_gfx_otag_index;
extern s8 g_jobstts_gfx_otag_index_locked;
extern u16 g_jobstts_gfx_otag_length;
extern u16 g_jobstts_gfx_poly_ft4_count;
extern s16 g_jobstts_gfx_semitransparency;
extern u16 g_jobstts_gfx_texture_page;
extern s8 g_jobstts_gfx_transition_frame;
extern u16 g_jobstts_input_newly_pressed;
extern u16 g_jobstts_input_page_scroll_hold_frames;
extern s32 g_jobstts_input_primary_repeat;
extern s32 g_jobstts_input_secondary_repeat;
extern u32 g_jobstts_job_current_jp;
extern s32 g_jobstts_job_current_level;
extern s32 g_jobstts_job_current_mastered;
extern u32 g_jobstts_job_current_total_jp;
extern s16 g_jobstts_job_ids[];
extern s32 g_jobstts_job_list_menu_initialized;
extern u8 g_jobstts_job_list_render_commands[];
extern u32 g_jobstts_job_next_level_jp_requirement;
extern u16 g_jobstts_job_selected_id;
extern s8 g_jobstts_menu_event_speed;
extern u16* g_jobstts_menu_list_entries;
extern s16 g_jobstts_menu_list_entry_count;
extern u16 g_jobstts_menu_list_row_height;
extern s32 g_jobstts_menu_list_row_index;
extern s16 g_jobstts_menu_list_row_rendering;
extern s16 g_jobstts_menu_list_scroll_offset_y;
extern s16 g_jobstts_menu_list_selected_index;
extern u16* g_jobstts_menu_list_text_table;
extern s16 g_jobstts_menu_list_visible_rows;
extern u16 g_jobstts_menu_list_vram_width;
extern s16 g_jobstts_menu_scroll_base_index;
extern u16 g_jobstts_menu_selection_values[];
extern u8 g_jobstts_sound_queued_effect_id;
extern u8 g_jobstts_text_color[3];
extern jobstts_unit_job_data_t* g_jobstts_unit_data[];
extern jobstts_unit_job_data_t g_jobstts_unit_job_data;

/* Unnamed data, in address order. */
extern u8 g_jobstts_menu_palette_image[];
extern s8 g_jobstts_active_page;
extern u8 g_jobstts_initialized;
extern u8 g_jobstts_ability_list_render_commands[];
extern u8 g_jobstts_ability_list_initialized;
extern s16 g_jobstts_ability_list_job_name_ids[];
extern u32 g_jobstts_help_text_table[];
extern u16 g_jobstts_input_latched_button;
extern u8 g_jobstts_text_thread_running_state;
extern u8 g_jobstts_input_lock_timer;
extern u16 g_jobstts_menu_cursor_texture_page;
extern u16 g_jobstts_text_clut_2_mode0;
extern u16 g_jobstts_text_clut_2_mode1;
extern u16 g_jobstts_text_clut_1_mode1;
extern u16 g_jobstts_text_clut_1_mode0;
extern u16 g_jobstts_text_clut_3_mode0;
extern u16 g_jobstts_text_clut_3_mode1;
extern u16 g_jobstts_menu_cursor_mode0_foreground_clut;
extern u16 g_jobstts_menu_cursor_mode0_background_clut;
extern u16 g_jobstts_menu_cursor_mode1_foreground_clut;
extern u16 g_jobstts_menu_cursor_mode1_background_clut;
extern u16 g_jobstts_text_clut_0_mode1;
extern u16 g_jobstts_text_clut_0_mode0;
extern s16 g_jobstts_gfx_transition_height_percent[];
extern s8 g_jobstts_menu_cursor_forward_offset_keyframes[];
extern s8 g_jobstts_menu_cursor_reverse_offset_keyframes[];
extern u8 g_jobstts_previous_event_mode;
extern u8 g_jobstts_gfx_poly_ft4_buffer[];
extern u8 g_jobstts_gfx_draw_move_buffer[];
extern u8 g_jobstts_gfx_draw_area_buffer[];
extern u8 g_jobstts_gfx_otag_buffer[];
extern u8 g_jobstts_gfx_contexts[];
extern volatile u32 g_jobstts_input_repeat_counters[16];
extern u16 g_jobstts_input_previous;
extern s32 g_jobstts_cmd_stream_input;
extern s16 g_jobstts_menu_list_row_group_count;
extern s8 g_jobstts_menu_list_redraw_pending;
extern u8 g_jobstts_menu_list_glyph_width;
extern u8 g_jobstts_menu_list_text_columns;
extern u8 g_jobstts_text_layout_mode;
extern s8 g_jobstts_menu_list_scroll_direction; /* read with lb (jobstts_cmd_draw_scrollable_list_body_handler) */
extern s16 g_jobstts_gfx_poly_f3_capacity;
extern s16 g_jobstts_gfx_poly_g3_capacity;
extern s16 g_jobstts_gfx_poly_f4_capacity;
extern s16 g_jobstts_gfx_poly_g4_capacity;
extern s16 g_jobstts_gfx_poly_f3_count;
extern s16 g_jobstts_gfx_poly_g3_count;
extern s16 g_jobstts_gfx_poly_f4_count;
extern s16 g_jobstts_gfx_poly_g4_count;
extern u16 g_jobstts_text_metric_3;
extern s16 g_jobstts_gfx_sprite_capacity;
extern s16 g_jobstts_gfx_line_f2_capacity;
extern volatile u16 g_jobstts_gfx_line_g2_capacity; /* LINE_G2 capacity; load order matters in 0x801e49f0 */
extern s16 g_jobstts_gfx_line_f3_capacity;
extern volatile u16 g_jobstts_gfx_line_g3_capacity; /* LINE_G3 capacity; load order matters in 0x801e49f0 */
extern s16 g_jobstts_gfx_line_f4_capacity;
extern s16 g_jobstts_gfx_line_g4_capacity;
extern s16 g_jobstts_gfx_poly_ft3_capacity;
extern s16 g_jobstts_gfx_line_f2_count;
extern s16 g_jobstts_gfx_poly_gt3_capacity;
extern u16 g_jobstts_gfx_poly_ft4_capacity;
extern s16 g_jobstts_gfx_line_g2_count;
extern s16 g_jobstts_gfx_line_f3_count;
extern s16 g_jobstts_gfx_poly_gt4_capacity;
extern s16 g_jobstts_gfx_line_g3_count;
extern s16 g_jobstts_gfx_line_f4_count;
extern s16 g_jobstts_gfx_line_g4_count;
extern s16 g_jobstts_gfx_poly_ft3_count;
extern s16 g_jobstts_gfx_poly_gt3_count;
extern s16 g_jobstts_gfx_poly_gt4_count;
extern s16 g_jobstts_gfx_sprite_16_capacity;
extern u16 g_jobstts_text_metric_1;
extern s16 g_jobstts_gfx_sprite_8_capacity;
extern u16 g_jobstts_gfx_draw_area_capacity;
extern u16 g_jobstts_gfx_tile_capacity;
extern u16 g_jobstts_gfx_draw_move_capacity;
extern s16 g_jobstts_gfx_tile_count;
extern s16 g_jobstts_gfx_sprite_count;
extern s16 g_jobstts_gfx_tile_16_capacity;
extern u16 g_jobstts_text_metric_0;
extern s16 g_jobstts_gfx_tile_8_capacity;
extern s16 g_jobstts_gfx_tile_1_count;
extern u16 g_jobstts_text_metric_4;
extern s16 g_jobstts_gfx_tile_8_count;
extern s16 g_jobstts_gfx_sprite_8_count;
extern s16 g_jobstts_gfx_tile_1_capacity;
extern u16 g_jobstts_text_metric_5;
extern s16 g_jobstts_gfx_tile_16_count;
extern s16 g_jobstts_gfx_sprite_16_count;

/* ability */
s32 jobstts_ability_build_list(
    s16 unit_index, s16 job_id, jobstts_ability_category_e ability_category, s16* list, s32 unlearned_only);
s32 jobstts_ability_get_ct_display_value(s32 index);
u32 jobstts_ability_get_jp_cost_display_value(s32 index);
u32 jobstts_ability_get_mp_cost_display_value(s32 index);
s32 jobstts_ability_is_action(void);
s32 jobstts_ability_is_action_tab(void);
s32 jobstts_ability_is_learned(s32 index);
s32 jobstts_ability_is_movement_tab(void);
s32 jobstts_ability_is_non_action_learned(s32 index);
s32 jobstts_ability_is_non_action_unlearned(s32 index);
s32 jobstts_ability_is_reaction_tab(void);
s32 jobstts_ability_is_support_tab(void);

/* bits */
void jobstts_bits_init_primary_reader(const u8* data);
u32 jobstts_bits_read_primary(s32 count);

/* cmd */
u8* jobstts_cmd_draw_background_tiles_handler(u8* stream);
u8* jobstts_cmd_draw_border_tiles_with_transition_clip(u8* command);
void jobstts_cmd_run_stream(u8* data, s32 flags);
void jobstts_cmd_run_stream_with_mode(void* first, void* second, s32 event_mode);

/* create */
u8* jobstts_create_border_tiles(u8* command);

/* gfx */
void jobstts_gfx_enqueue_draw_area(RECT* rect, s32 idx);
void jobstts_gfx_enqueue_textured_quad(
    urect16_t* rect, s32 u, s32 v, u8* color, s32 semitrans, u16 tpage, u16 clut, s32 ot_index);
s32 jobstts_gfx_get_transition_frame(void);
void jobstts_gfx_init_contexts(jobstts_gfx_context_t* base, u32* otag, POLY_F3* poly_f3, u8* poly_ft3, POLY_F4* poly_f4,
    POLY_FT4* textured_quads, u8* poly_g3, u8* poly_gt3, POLY_G4* poly_g4, POLY_GT4* poly_gt4, LINE_F2* line_f2,
    u8* line_f3, u8* line_f4, LINE_G2* line_g2, u8* line_g3, u8* line_g4, TILE* tiles, u8* tiles_16, u8* tiles_8,
    u8* tiles_1, SPRT* sprites, u8* sprites_16, u8* sprites_8, DR_MOVE* draw_moves, DR_AREA* draw_areas);
void jobstts_gfx_init_primitive_buffers(jobstts_gfx_context_t* context);
void jobstts_gfx_load_image_and_wait(RECT* rect, u32* data);
void jobstts_gfx_set_transition_frame(s32 frame);
void jobstts_gfx_store_image_and_wait(RECT* rect, u32* data);
void jobstts_gfx_swap_context_and_clear_otag(s32 first_otag);

/* input */
jobstts_menu_page_scroll_direction_e jobstts_input_read_page_scroll_direction(void);

/* job */
s32 jobstts_job_build_unit_job_list(u8 unit_id, s16* job_ids, s32 unused);
void jobstts_job_calculate_current_jp(s32 index);
s32 jobstts_job_calculate_current_level(s32 index);
s32 jobstts_job_calculate_current_mastered(s32 index);
void jobstts_job_calculate_current_total_jp(s32 index);
s32 jobstts_job_calculate_next_level_jp_requirement(s32 index);
s32 jobstts_job_find_first_for_skillset(s32 skillset_id);
s32 jobstts_job_get_base(s16 unit_id);
u32 jobstts_job_get_current_jp(void);
s32 jobstts_job_get_current_level(void);
u32 jobstts_job_get_current_total_jp(void);
s32 jobstts_job_get_generic_index(s32 job_id);
u32 jobstts_job_get_next_level_jp_requirement(void);
s32 jobstts_job_get_skillset(s32 job_id);
s32 jobstts_job_is_current_mastered(void);
s32 jobstts_job_is_special_monster(s32 job_id);

/* menu */
void jobstts_menu_clear_selection_record(s32 index);
void jobstts_menu_clear_selection_records(void);
void jobstts_menu_clear_selection_values(void);
void jobstts_menu_draw_scrollable_list(u8* script);
s32 jobstts_menu_get_event_speed(void);
void jobstts_menu_init_scrollable_list(const s16* entries, s32 selected_index, s32 value, const void* data);
void jobstts_menu_init_scrollable_list_core(s16* entries, s32 selected_index, s32 data);
void jobstts_menu_scroll_list_by_page(jobstts_menu_page_scroll_direction_e direction, const u8* commands);
s32 jobstts_menu_update_job_list(void);
void jobstts_menu_set_selection_record(s32 index, s32 selected_index, s32 scroll_index, const u16* abilities);
s32 jobstts_menu_update_wrapped_horizontal_selection(u16 count, u8 index, s32 buttons);

/* out */
void jobstts_menu_load_images_and_clear_selection(void);
void jobstts_input_update_with_message_state(void);

/* text */
void jobstts_text_render_encoded_ids_to_image(u8* image, const battle_menu_text_image_bounds_t* bounds,
    s32 glyph_spacing, s32 line_width, const void* glyph_data, const u16* text_ids, s32 max_entries, s32 fill_glyph_id,
    s32 unused_style);
void jobstts_text_render_id_rows_to_vram(s32 text_table, s16* list, RECT* rect, s32 style);
s32 jobstts_text_render_glyph_to_4bpp_image(
    s32 glyph_id, u8* image, const jobstts_text_image_position_t* position, s32 style);
void jobstts_text_set_palette_and_metrics(s32 mode);
const u8* jobstts_text_skip_encoded_segments(const u8* text, s16 terminators);
void jobstts_text_start_help_thread(s32 thread_data);

/* unit */
void jobstts_unit_copy_job_data(battle_stats_t* g_battle_unit_stats, jobstts_unit_job_data_t* destination);
void jobstts_unit_init_job_data(s32 unit_id);

/* update */
s32 jobstts_update_ability_list_menu(void);
void jobstts_update_controller_input(void);

extern u8 g_jobstts_input_activation_timer;
extern u16 g_jobstts_menu_list_cursor_anim_state;
extern u16 g_jobstts_menu_list_scroll_entry_ids;
extern u16 g_jobstts_menu_list_scroll_progress;
extern jobstts_ability_selection_state_t g_jobstts_menu_selection_records[5];

/* Window right edge; WORLD's twin stores it to g_world_menu_window_right_x. */
extern s16 g_jobstts_menu_window_right_x;

/* The "%d" literal that JOBSTTS.OUT keeps at overlay file offset 0, i.e. the
 * three data bytes ahead of the first function. */
extern const char g_jobstts_text_decimal_format[];
extern s32 g_jobstts_text_selected_job_id;

const u8* jobstts_cmd_skip_group(const u8* data);
void jobstts_gfx_enqueue_draw_move(const RECT* rect, s32 u, s32 v, s32 otag_index);
void jobstts_menu_get_selection_record(s32 index, s16* out_selected_index, s16* out_scroll_index, const s16* abilities);
void jobstts_menu_update_and_draw_animated_marker(urect16_t* anchor, u16* state, s32 mode);

#endif
