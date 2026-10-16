#ifndef EVENT_JOBSTTS_H
#define EVENT_JOBSTTS_H

/* EVENT/JOBSTTS.OUT: the BATTLE overlay of the job and ability screens of the unit status menu. */

#include "fft/gfx.h"
#include "fft/job.h"
#include "fft/menu.h"
#include "fft/menu_script.h"
#include "fft/unit.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* screen */
extern s8 g_status_screen_active_page; /* menu_job_list_step_e; each page's function returns the next one */
extern u8 g_status_screen_initialized;
extern u8 g_status_screen_previous_event_mode;

u32* status_screen_entrypoint(s32 unit, void* unused_otag);

/* unit */
/* Job and ability data copied into JOBSTTS.OUT for the selected unit. */
typedef struct status_screen_unit_jobs {
    s16 job_id;                /* 0x00 */
    s16 entd_slot;             /* 0x02 */
    u16 primary_skillset;      /* 0x04 */
    u8 unit_flags;             /* 0x06 */
    u8 character_identity;     /* 0x07 */
    unit_career_data_t career; /* 0x08; unlocks, learned abilities, levels and JP */
} status_screen_unit_jobs_t;
typedef char jobstts_unit_job_data_size_must_be_158[(sizeof(status_screen_unit_jobs_t) == 158) ? 1 : -1];

extern status_screen_unit_jobs_t* g_status_screen_unit_data[];

void status_screen_init_unit_jobs(s32 unit_id);

/* jobs */
extern s32 g_job_list_list_menu_initialized;
extern menu_instruction_t g_job_list_list_render_commands;

s32 job_list_run(void);
s32 job_list_build(u8 unit_id, s16* job_ids, s32 unused);
s32 job_list_calculate_level(s32 index);
s32 job_list_calculate_jp(s32 index);
s32 job_list_calculate_total_jp(s32 index);
s32 job_list_calculate_mastered(s32 index);

/* abilities */
extern u8 g_ability_list_list_initialized;
extern s16 g_ability_list_list_job_name_ids[];
extern menu_instruction_t g_ability_list_list_render_commands;

s32 ability_list_update(void);
s32 ability_list_build(
    s16 unit_index, s16 job_id, menu_ability_category_e ability_category, s16* list, s32 unlearned_only);

/* menus */
/* Unsigned here where the other overlays keep signed values. */
extern u16 g_status_screen_menu_list_cursor_anim_state;
extern s16 g_status_screen_menu_list_scroll_entry_ids;
extern u8 g_status_screen_menu_palette_image[];
/* Window right edge; WORLD's twin stores it to g_screen_window_right_x. */
extern s16 g_status_screen_menu_window_right_x;

s32 status_screen_update_wrapped_horizontal_selection(u16 count, u8 index, s32 buttons);

/* graphics */
/* Per-buffer primitive-pool pointers used by JOBSTTS.OUT's renderer. */
typedef struct status_screen_gfx_context {
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
} status_screen_gfx_context_t;
typedef char jobstts_gfx_context_size_must_be_244[(sizeof(status_screen_gfx_context_t) == 244) ? 1 : -1];

extern status_screen_gfx_context_t* g_status_screen_gfx_context_base;
extern status_screen_gfx_context_t g_status_screen_gfx_contexts[];
extern u16 g_status_screen_gfx_draw_area_capacity;
extern u16 g_status_screen_gfx_draw_move_capacity;
/* LINE_G2 and LINE_G3 capacities; volatile keeps their load order in status_screen_gfx_init_contexts. */
extern volatile u16 g_status_screen_gfx_line_g2_capacity;
extern volatile u16 g_status_screen_gfx_line_g3_capacity;
extern s16 g_status_screen_gfx_line_f2_count;
extern s16 g_status_screen_gfx_poly_f4_count;
extern s16 g_status_screen_gfx_poly_g4_count;
extern s16 g_status_screen_gfx_tile_count;
extern u16 g_status_screen_gfx_background_texture_page;

void status_screen_gfx_init_contexts(status_screen_gfx_context_t* base, u32* otag, POLY_F3* poly_f3, POLY_FT3* poly_ft3,
    POLY_F4* poly_f4, POLY_FT4* textured_quads, POLY_G3* poly_g3, POLY_GT3* poly_gt3, POLY_G4* poly_g4,
    POLY_GT4* poly_gt4, LINE_F2* line_f2, LINE_F3* line_f3, LINE_F4* line_f4, LINE_G2* line_g2, LINE_G3* line_g3,
    LINE_G4* line_g4, TILE* tiles, TILE_1* tiles_1, TILE_8* tiles_8, TILE_16* tiles_16, SPRT* sprites,
    SPRT_8* sprites_8, SPRT_16* sprites_16, DR_MOVE* draw_moves, DR_AREA* draw_areas);
void status_screen_gfx_swap_context(s32 first_otag, s32 unused);
void status_screen_gfx_enqueue_oriented_textured_quad(
    RECT* rect, texture_source_t* uv, rgb24_t* color, s32 orientation, s32 semitrans, s32 ot_index);
void menu_gfx_enqueue_textured_quad_rect(
    RECT* rect, menu_icon_source_t* tex, rgb24_t* color, s32 semitrans, s32 ot_index);

/* input */
menu_vertical_scroll_direction_e menu_input_read_page_scroll_direction(void);
void menu_input_update_with_message_state(void);

/* menu opcodes */
/* Menu display script opcode handlers: each consumes and returns a command pointer. */
typedef menu_instruction_t* (*menu_opcode_handler_t)(menu_instruction_t* instruction);

/* The opcode that ends a JOBSTTS menu display stream. */
enum {
    STATUS_SCREEN_SCRIPT_END = 0x16,
};

enum {
    STATUS_SCREEN_LIST_COMMAND_VISIBLE_ROWS_INDEX = 6,
};

void menu_run_script_with_palette_mode(menu_instruction_t* script, s32 flags, s32 event_mode);
menu_instruction_t* menu_opcode_00_draw_window_frame(menu_instruction_t* instruction);
menu_instruction_t* menu_opcode_0d_draw_formatted_number(menu_instruction_t* instruction);
menu_instruction_t* menu_opcode_0f_set_draw_priority_if_unlocked(menu_instruction_t* instruction);
menu_instruction_t* menu_opcode_10_draw_scrollable_list(menu_instruction_t* instruction);
menu_instruction_t* menu_opcode_12_draw_opening_window_frame(menu_instruction_t* instruction);

/* text */
/* The "%d" literal that JOBSTTS.OUT keeps at overlay file offset 0, i.e. the
 * three data bytes ahead of the first function. */
extern const char g_status_screen_text_decimal_format[];

#endif
