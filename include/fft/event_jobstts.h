#ifndef EVENT_JOBSTTS_H
#define EVENT_JOBSTTS_H

/* EVENT/JOBSTTS.OUT: menu screen overlay that runs with BATTLE. */

#include "fft/battle.h"

/* screen */

extern s8 g_jobstts_active_page; /* menu_job_list_step_e; each page's function returns the next one */

extern u8 g_jobstts_initialized;
extern u8 g_jobstts_previous_event_mode;

u32* jobstts_entrypoint(s32 unit, void* unused_otag);

/* unit */
/* Job and ability data copied into JOBSTTS.OUT for the selected unit. */
typedef struct jobstts_unit_job_data {
    s16 job_id;                /* 0x00 */
    s16 entd_slot;             /* 0x02 */
    u16 primary_skillset;      /* 0x04 */
    u8 unit_flags;             /* 0x06 */
    u8 character_identity;     /* 0x07 */
    unit_career_data_t career; /* 0x08; unlocks, learned abilities, levels and JP */
} jobstts_unit_job_data_t;
typedef char jobstts_unit_job_data_size_must_be_158[(sizeof(jobstts_unit_job_data_t) == 158) ? 1 : -1];

extern jobstts_unit_job_data_t* g_jobstts_unit_data[];
extern jobstts_unit_job_data_t g_jobstts_unit_job_data;

void jobstts_unit_copy_job_data(unit_stats_t* unit, jobstts_unit_job_data_t* destination);
void jobstts_unit_init_job_data(s32 unit_id);

/* ability */

extern u8 g_jobstts_ability_list_initialized;
extern s16 g_jobstts_ability_list_job_name_ids[];
extern menu_instruction_t g_jobstts_ability_list_render_commands;

s32 jobstts_ability_build_list(
    s16 unit_index, s16 job_id, menu_ability_category_e ability_category, s16* list, s32 unlearned_only);
s32 jobstts_ability_update_list_menu(void);

/* job */
extern u32 g_jobstts_job_current_jp;
extern s32 g_jobstts_job_current_mastered;
extern u32 g_jobstts_job_current_total_jp;
extern menu_instruction_t g_jobstts_job_list_render_commands;
extern u16 g_jobstts_job_selected_id;

s32 jobstts_job_build_unit_job_list(u8 unit_id, s16* job_ids, s32 unused);
s32 jobstts_job_calculate_current_jp(s32 index);
s32 jobstts_job_calculate_current_level(s32 index);
s32 jobstts_job_calculate_current_mastered(s32 index);
s32 jobstts_job_calculate_current_total_jp(s32 index);
s32 jobstts_job_get_base(s16 unit_id);
s32 jobstts_job_get_current_jp(s32 unused_row);
s32 jobstts_job_get_current_total_jp(s32 unused_row);
s32 jobstts_job_is_current_mastered(s32 unused_row);

/* menus */
typedef enum jobstts_page_scroll_direction {
    JOBSTTS_PAGE_SCROLL_UP = -1,
    JOBSTTS_PAGE_SCROLL_NONE = 0,
    JOBSTTS_PAGE_SCROLL_DOWN = 1,
} jobstts_menu_page_scroll_direction_e;

extern s32 g_jobstts_job_list_menu_initialized;

extern u16 g_jobstts_menu_list_cursor_anim_state;
extern s16* g_jobstts_menu_list_entries;
extern s16 g_jobstts_menu_list_entry_count;
extern u8 g_jobstts_menu_list_glyph_width;
extern s8 g_jobstts_menu_list_redraw_pending;
extern s16 g_jobstts_menu_list_row_group_count;
extern s16 g_jobstts_menu_list_row_height;
extern s32 g_jobstts_menu_list_row_index;
extern s16 g_jobstts_menu_list_row_rendering;
extern s8 g_jobstts_menu_list_scroll_direction; /* read with lb (jobstts_cmd_draw_scrollable_list_body_handler) */
extern s16 g_jobstts_menu_list_scroll_entry_ids;
extern s16 g_jobstts_menu_list_scroll_offset_y;
extern u16 g_jobstts_menu_list_scroll_progress;
extern s16 g_jobstts_menu_list_selected_index;
extern u8 g_jobstts_menu_list_text_columns;
extern u8* g_jobstts_menu_list_text_table;
extern s16 g_jobstts_menu_list_visible_rows;
extern u16 g_jobstts_menu_list_vram_width;
extern u8 g_jobstts_menu_palette_image[];
extern s16 g_jobstts_menu_scroll_base_index;
extern menu_selection_record_t g_jobstts_menu_selection_records[5];
extern u16 g_jobstts_menu_selection_values[];
/* Window right edge; WORLD's twin stores it to g_world_menu_window_right_x. */
extern s16 g_jobstts_menu_window_right_x;

void jobstts_menu_clear_selection_record(s32 index);
void jobstts_menu_clear_selection_records(void);
void jobstts_menu_clear_selection_values(void);
void jobstts_menu_draw_scrollable_list(const menu_instruction_t* script);
void jobstts_menu_get_selection_record(s32 index, s16* out_selected_index, s16* out_scroll_index, const s16* abilities);
void jobstts_menu_init_and_draw_scrollable_list(
    s16* entries, s32 selected_index, s32 scroll_base_index, u8* text_table, const menu_instruction_t* script);
void jobstts_menu_init_and_draw_scrollable_list_simple(
    s16* entries, s32 selected_index, u8* text_table, const menu_instruction_t* script);
void jobstts_menu_init_scrollable_list(s16* entries, s32 selected_index, s32 scroll_base_index, u8* text_table);
void jobstts_menu_init_scrollable_list_core(s16* entries, s32 selected_index, u8* text_table);
void jobstts_menu_load_images_and_clear_selection(void);
void jobstts_menu_scroll_list_by_page(jobstts_menu_page_scroll_direction_e direction, const menu_instruction_t* list);
void jobstts_menu_set_selection_record(s32 index, s32 selected_index, s32 scroll_index, s16* abilities);
s32 jobstts_menu_update_job_list(void);
s32 jobstts_menu_update_wrapped_horizontal_selection(u16 count, u8 index, s32 buttons);

/* help */
extern u32 g_jobstts_help_text_table[];

/* graphics */
/* Per-buffer primitive-pool pointers used by JOBSTTS.OUT's renderer. */
typedef struct jobstts_gfx_context {
    u32* otag;                /* 0x00 */
    POLY_F3* poly_f3;         /* 0x04 */
    POLY_FT3* poly_ft3;       /* 0x08 */
    POLY_F4* poly_f4;         /* 0x0c */
    POLY_FT4* textured_quads; /* 0x10 */
    POLY_G3* poly_g3;         /* 0x14 */
    POLY_GT3* poly_gt3;       /* 0x18 */
    POLY_G4* poly_g4;         /* 0x1c */
    POLY_GT4* poly_gt4;       /* 0x20 */
    LINE_F2* line_f2;         /* 0x24 */
    LINE_F3* line_f3;         /* 0x28 */
    LINE_F4* line_f4;         /* 0x2c */
    LINE_G2* line_g2;         /* 0x30 */
    LINE_G3* line_g3;         /* 0x34 */
    LINE_G4* line_g4;         /* 0x38 */
    TILE* tiles;              /* 0x3c */
    TILE_1* tiles_1;          /* 0x40 */
    TILE_8* tiles_8;          /* 0x44 */
    TILE_16* tiles_16;        /* 0x48 */
    SPRT* sprites;            /* 0x4c */
    SPRT_8* sprites_8;        /* 0x50 */
    SPRT_16* sprites_16;      /* 0x54 */
    DR_MOVE* draw_moves;      /* 0x58 */
    DR_AREA* draw_areas;      /* 0x5c */
    u8 _unused_60[0x94];      /* 0x60 */
} jobstts_gfx_context_t;
typedef char jobstts_gfx_context_size_must_be_244[(sizeof(jobstts_gfx_context_t) == 244) ? 1 : -1];

extern u16 g_jobstts_gfx_background_texture_page;
extern jobstts_gfx_context_t* g_jobstts_gfx_context;
extern jobstts_gfx_context_t* g_jobstts_gfx_context_base;
extern jobstts_gfx_context_t g_jobstts_gfx_contexts[];
extern RECT g_jobstts_gfx_draw_area;
extern DR_AREA g_jobstts_gfx_draw_area_buffer[];
extern u16 g_jobstts_gfx_draw_area_capacity;
extern u16 g_jobstts_gfx_draw_area_count;
extern DR_MOVE g_jobstts_gfx_draw_move_buffer[];
extern u16 g_jobstts_gfx_draw_move_capacity;
extern u16 g_jobstts_gfx_draw_move_count;
extern u16 g_jobstts_gfx_drawenv_y;
extern u16 g_jobstts_gfx_line_f2_capacity;
extern s16 g_jobstts_gfx_line_f2_count;

extern u16 g_jobstts_gfx_line_f3_capacity;
extern s16 g_jobstts_gfx_line_f3_count;

extern u16 g_jobstts_gfx_line_f4_capacity;
extern s16 g_jobstts_gfx_line_f4_count;
extern volatile u16 g_jobstts_gfx_line_g2_capacity; /* LINE_G2 capacity; load order matters in 0x801e49f0 */
extern s16 g_jobstts_gfx_line_g2_count;
extern volatile u16 g_jobstts_gfx_line_g3_capacity; /* LINE_G3 capacity; load order matters in 0x801e49f0 */
extern s16 g_jobstts_gfx_line_g3_count;
extern u16 g_jobstts_gfx_line_g4_capacity;
extern s16 g_jobstts_gfx_line_g4_count;
extern u32 g_jobstts_gfx_otag_buffer[];
extern s8 g_jobstts_gfx_otag_index_locked;
extern u16 g_jobstts_gfx_otag_length;
extern u16 g_jobstts_gfx_poly_f3_capacity;
extern s16 g_jobstts_gfx_poly_f3_count;
extern u16 g_jobstts_gfx_poly_f4_capacity;
extern s16 g_jobstts_gfx_poly_f4_count;
extern u16 g_jobstts_gfx_poly_ft3_capacity;
extern s16 g_jobstts_gfx_poly_ft3_count;
extern POLY_FT4 g_jobstts_gfx_poly_ft4_buffer[];
extern u16 g_jobstts_gfx_poly_ft4_capacity;
extern u16 g_jobstts_gfx_poly_ft4_count;
extern u16 g_jobstts_gfx_poly_g3_capacity;
extern s16 g_jobstts_gfx_poly_g3_count;
extern u16 g_jobstts_gfx_poly_g4_capacity;
extern s16 g_jobstts_gfx_poly_g4_count;
extern u16 g_jobstts_gfx_poly_gt3_capacity;
extern s16 g_jobstts_gfx_poly_gt3_count;
extern u16 g_jobstts_gfx_poly_gt4_capacity;
extern s16 g_jobstts_gfx_poly_gt4_count;
extern s16 g_jobstts_gfx_semitransparency;
extern u16 g_jobstts_gfx_sprite_16_capacity;
extern s16 g_jobstts_gfx_sprite_16_count;
extern u16 g_jobstts_gfx_sprite_8_capacity;
extern s16 g_jobstts_gfx_sprite_8_count;
extern u16 g_jobstts_gfx_sprite_capacity;
extern s16 g_jobstts_gfx_sprite_count;
extern u16 g_jobstts_gfx_tile_16_capacity;
extern s16 g_jobstts_gfx_tile_16_count;
extern u16 g_jobstts_gfx_tile_1_capacity;
extern s16 g_jobstts_gfx_tile_1_count;
extern u16 g_jobstts_gfx_tile_8_capacity;
extern s16 g_jobstts_gfx_tile_8_count;
extern u16 g_jobstts_gfx_tile_capacity;
extern s16 g_jobstts_gfx_tile_count;
extern s8 g_jobstts_gfx_transition_frame;
extern s16 g_jobstts_gfx_transition_height_percent[];

void jobstts_gfx_enqueue_draw_area(RECT* rect, s32 idx);
void jobstts_gfx_enqueue_draw_move(const RECT* rect, s32 u, s32 v, s32 otag_index);
void jobstts_gfx_enqueue_oriented_textured_quad(
    RECT* rect, texture_source_t* uv, rgb24_t* color, s32 orientation, s32 semitrans, s32 ot_index);
void jobstts_gfx_enqueue_textured_quad(
    const RECT* rect, s32 u, s32 v, const rgb24_t* color, s32 semitrans, u16 tpage, u16 clut, s32 ot_index);
void jobstts_gfx_enqueue_textured_quad_corners(
    textured_quad_source_t* quad, rgb24_t* color, s32 semitrans, s32 ot_index);
void jobstts_gfx_enqueue_textured_quad_list(
    menu_sprite_cell_t* sprites, rgb24_t* color, s32 semitrans, u16 tpage, u16 clut, s32 ot_index, s32 count);
void jobstts_gfx_enqueue_textured_quad_rect(
    RECT* rect, menu_icon_source_t* tex, rgb24_t* color, s32 semitrans, s32 ot_index);
s32 jobstts_gfx_get_transition_frame(void);
void jobstts_gfx_init_contexts(jobstts_gfx_context_t* base, u32* otag, POLY_F3* poly_f3, POLY_FT3* poly_ft3,
    POLY_F4* poly_f4, POLY_FT4* textured_quads, POLY_G3* poly_g3, POLY_GT3* poly_gt3, POLY_G4* poly_g4,
    POLY_GT4* poly_gt4, LINE_F2* line_f2, LINE_F3* line_f3, LINE_F4* line_f4, LINE_G2* line_g2, LINE_G3* line_g3,
    LINE_G4* line_g4, TILE* tiles, TILE_1* tiles_1, TILE_8* tiles_8, TILE_16* tiles_16, SPRT* sprites,
    SPRT_8* sprites_8, SPRT_16* sprites_16, DR_MOVE* draw_moves, DR_AREA* draw_areas);
void jobstts_gfx_init_primitive_buffers(jobstts_gfx_context_t* context);
void jobstts_gfx_set_transition_frame(s32 frame);
void jobstts_gfx_swap_context_and_clear_otag(s32 first_otag, s32 unused);

/* input */
extern u8 g_jobstts_input_lock_timer;
extern u16 g_jobstts_input_page_scroll_hold_frames;

jobstts_menu_page_scroll_direction_e jobstts_input_read_page_scroll_direction(void);
void jobstts_input_update_with_message_state(void);

/* menu commands */
/* Render-command stream callbacks: every handler consumes and returns a
 * command pointer; every condition is indexed by a row or literal operand. */
typedef menu_instruction_t* (*jobstts_command_handler_t)(menu_instruction_t* instruction);
typedef s32 (*jobstts_condition_t)(s32);
/* The opcode that ends a JOBSTTS menu display stream. */
enum {
    JOBSTTS_CMD_END = 0x16,
};
enum {
    JOBSTTS_LIST_COMMAND_VISIBLE_ROWS_INDEX = 6,
};

extern jobstts_condition_t g_jobstts_cmd_conditions[];
extern jobstts_command_handler_t g_jobstts_cmd_handlers[22];
extern s32 g_jobstts_cmd_stream_input;
extern menu_window_command_t g_jobstts_cmd_window_interior_command;

menu_instruction_t* jobstts_cmd_draw_border_tiles_with_transition_clip(menu_instruction_t* instruction);
menu_instruction_t* jobstts_cmd_draw_formatted_number_handler(menu_instruction_t* instruction);
menu_instruction_t* jobstts_cmd_draw_scrollable_list_body_handler(menu_instruction_t* instruction);
menu_instruction_t* jobstts_cmd_draw_window_frame_handler(menu_instruction_t* instruction);
menu_instruction_t* jobstts_cmd_release_otag_lock_handler(menu_instruction_t* instruction);
menu_instruction_t* jobstts_cmd_run_if_else_group_handler(menu_instruction_t* instruction);
menu_instruction_t* jobstts_cmd_run_if_group_handler(menu_instruction_t* instruction);
void jobstts_cmd_run_stream(menu_instruction_t* instruction, s32 flags);
void jobstts_cmd_run_stream_with_mode(menu_instruction_t* script, s32 flags, s32 event_mode);
menu_instruction_t* jobstts_cmd_set_otag_index_if_unlocked_handler(menu_instruction_t* instruction);
menu_instruction_t* jobstts_cmd_set_semitrans_handler(menu_instruction_t* instruction);

/* text */
/* The "%d" literal that JOBSTTS.OUT keeps at overlay file offset 0, i.e. the
 * three data bytes ahead of the first function. */
extern const char g_jobstts_text_decimal_format[];
extern s32 g_jobstts_text_selection_id;

const u8* jobstts_text_find_entry_by_index(const u8* text, s16 terminators, s32 unused_2);
void jobstts_text_render_id_rows_to_vram(u8* text_table, s16* list, RECT* rect, s32 style);

/* bit streams */
extern u8 g_jobstts_bits_primary_reset;
extern const u8* g_jobstts_bits_primary_source;
extern s32 g_jobstts_bits_reader_1_index;

void jobstts_bits_init_primary_reader(const u8* data);
u32 jobstts_bits_read_primary(s32 count);

/* threads */
extern u8 g_jobstts_text_thread_running_state;

/* sound */
extern u8 g_jobstts_sound_queued_effect_id; /* main_sfx_e */

#endif
