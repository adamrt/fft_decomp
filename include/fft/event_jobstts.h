#ifndef EVENT_JOBSTTS_H
#define EVENT_JOBSTTS_H

/* EVENT/JOBSTTS.OUT: menu screen overlay that runs with BATTLE. */

#include "fft/battle.h"
#include "fft/bits.h"
#include "fft/card.h"
#include "fft/file.h"
#include "fft/gfx.h"
#include "fft/item.h"
#include "fft/job.h"
#include "fft/sound.h"
#include "fft/text.h"
#include "fft/thread.h"

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

/* job */
extern menu_instruction_t g_jobstts_job_list_render_commands;

s32 jobstts_job_build_unit_job_list(u8 unit_id, s16* job_ids, s32 unused);
s32 jobstts_job_get_base(s16 unit_id);

/* menus */
/* Unsigned here where the other overlays keep signed values. */
extern u16 g_jobstts_menu_selection_values[];
typedef enum jobstts_page_scroll_direction {
    JOBSTTS_PAGE_SCROLL_UP = -1,
    JOBSTTS_PAGE_SCROLL_NONE = 0,
    JOBSTTS_PAGE_SCROLL_DOWN = 1,
} jobstts_menu_page_scroll_direction_e;

extern s32 g_jobstts_job_list_menu_initialized;

extern u16 g_jobstts_menu_list_cursor_anim_state;
extern s16 g_jobstts_menu_list_scroll_entry_ids;
extern u8 g_jobstts_menu_palette_image[];
/* Window right edge; WORLD's twin stores it to g_world_menu_window_right_x. */
extern s16 g_jobstts_menu_window_right_x;

void jobstts_menu_load_images_and_clear_selection(void);
s32 jobstts_menu_update_wrapped_horizontal_selection(u16 count, u8 index, s32 buttons);

/* help */

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
extern jobstts_gfx_context_t* g_jobstts_gfx_context_base;
extern jobstts_gfx_context_t g_jobstts_gfx_contexts[];
extern RECT g_jobstts_gfx_draw_area;
extern u16 g_jobstts_gfx_draw_area_capacity;
extern u16 g_jobstts_gfx_draw_move_capacity;
extern s16 g_jobstts_gfx_line_f2_count;

extern volatile u16 g_jobstts_gfx_line_g2_capacity; /* LINE_G2 capacity; load order matters in 0x801e49f0 */
extern volatile u16 g_jobstts_gfx_line_g3_capacity; /* LINE_G3 capacity; load order matters in 0x801e49f0 */
extern s8 g_jobstts_gfx_otag_index_locked;
extern s16 g_jobstts_gfx_poly_f4_count;
extern s16 g_jobstts_gfx_poly_g4_count;
extern s16 g_jobstts_gfx_tile_count;
extern s16 g_jobstts_gfx_transition_height_percent[];

void jobstts_gfx_enqueue_oriented_textured_quad(
    RECT* rect, texture_source_t* uv, rgb24_t* color, s32 orientation, s32 semitrans, s32 ot_index);
void jobstts_gfx_enqueue_textured_quad_rect(
    RECT* rect, menu_icon_source_t* tex, rgb24_t* color, s32 semitrans, s32 ot_index);
void jobstts_gfx_init_contexts(jobstts_gfx_context_t* base, u32* otag, POLY_F3* poly_f3, POLY_FT3* poly_ft3,
    POLY_F4* poly_f4, POLY_FT4* textured_quads, POLY_G3* poly_g3, POLY_GT3* poly_gt3, POLY_G4* poly_g4,
    POLY_GT4* poly_gt4, LINE_F2* line_f2, LINE_F3* line_f3, LINE_F4* line_f4, LINE_G2* line_g2, LINE_G3* line_g3,
    LINE_G4* line_g4, TILE* tiles, TILE_1* tiles_1, TILE_8* tiles_8, TILE_16* tiles_16, SPRT* sprites,
    SPRT_8* sprites_8, SPRT_16* sprites_16, DR_MOVE* draw_moves, DR_AREA* draw_areas);
void jobstts_gfx_init_primitive_buffers(jobstts_gfx_context_t* context);
void jobstts_gfx_swap_context_and_clear_otag(s32 first_otag, s32 unused);

/* input */

jobstts_menu_page_scroll_direction_e jobstts_input_read_page_scroll_direction(void);

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

menu_instruction_t* menu_cmd_draw_border_tiles_with_transition_clip(menu_instruction_t* instruction);
menu_instruction_t* jobstts_cmd_draw_window_frame_handler(menu_instruction_t* instruction);
menu_instruction_t* jobstts_cmd_set_otag_index_if_unlocked_handler(menu_instruction_t* instruction);

/* text */
/* The "%d" literal that JOBSTTS.OUT keeps at overlay file offset 0, i.e. the
 * three data bytes ahead of the first function. */
extern const char g_jobstts_text_decimal_format[];

/* bit streams */

/* threads */

/* sound */

#endif
