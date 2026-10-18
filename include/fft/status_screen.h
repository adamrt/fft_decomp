#ifndef STATUS_SCREEN_H
#define STATUS_SCREEN_H

/* The Status screen: its equipment and ability panel, numbers panel and status list (event library, in BATTLE and
 * WORLD), JOBSTTS's job pages, and DEBUGCHR's read-only view. */

#include "fft/gfx.h"
#include "fft/job.h"
#include "fft/menu.h"
#include "fft/menu_list.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* status panel */
void status_screen_thread(void);

/* numbers panel */
void status_screen_init_numbers_frame(RECT* bounds, menu_numeric_frame_t* menu);
void status_screen_submit_numbers_frame(menu_numeric_frame_t* display);
void status_screen_numbers_thread(void);

/* status list */
/* The status panel's list of the unit's current statuses, in two columns. */
extern menu_scroll_text_layout_t g_status_screen_status_list_layout;

void status_screen_build_status_list(void);

/* overlay screens */
void status_screen_jobs_thread(void);               /* runs JOBSTTS while its state is nonzero */
void status_screen_open_mounted_unit(void);         /* DEBUGCHR's view of a selected rider or mount */
void debugchr_mounted_unit_entrypoint(s32 unit_id); /* DEBUGCHR's read-only rider/mount Status loop */

/* job pages (JOBSTTS) */
/* Job and ability data copied into JOBSTTS.OUT for the selected unit. */
typedef struct status_screen_unit_jobs {
    s16 job_id;                /* 0x00 */
    s16 active_battle_id;      /* 0x02 */
    u16 primary_skillset;      /* 0x04 */
    u8 unit_flags;             /* 0x06 */
    u8 character_identity;     /* 0x07 */
    unit_career_data_t career; /* 0x08; unlocks, learned abilities, levels and JP */
} status_screen_unit_jobs_t;
typedef char status_screen_unit_jobs_size_must_be_158[(sizeof(status_screen_unit_jobs_t) == 158) ? 1 : -1];

extern status_screen_unit_jobs_t* g_status_screen_unit_data[];

/* Advance JOBSTTS one frame; return its ordering table while open, or zero on close. */
u32* jobstts_entrypoint(s32 unit, void* unused_otag);
void status_screen_init_unit_jobs(s32 unit_id);

/* menu display (JOBSTTS) */
/* The opcode that ends a JOBSTTS menu display stream. */
enum { STATUS_SCREEN_SCRIPT_END = 0x16 };

enum { STATUS_SCREEN_LIST_COMMAND_VISIBLE_ROWS_INDEX = 6 };

/* Window right edge; WORLD's twin stores it to g_screen_window_right_x. */
extern s16 g_status_screen_menu_window_right_x;

s32 status_screen_update_wrapped_horizontal_selection(u16 count, u8 index, s32 buttons);

/* primitive pools (JOBSTTS) */
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
typedef char status_screen_gfx_context_size_must_be_244[(sizeof(status_screen_gfx_context_t) == 244) ? 1 : -1];

extern u16 g_status_screen_gfx_draw_area_capacity;
extern u16 g_status_screen_gfx_draw_move_capacity;
extern u16 g_status_screen_gfx_background_texture_page;

/* LINE_G2 and LINE_G3 capacities; volatile keeps their load order in status_screen_gfx_init_contexts. */
extern volatile u16 g_status_screen_gfx_line_g2_capacity;
extern volatile u16 g_status_screen_gfx_line_g3_capacity;

void status_screen_gfx_init_contexts(status_screen_gfx_context_t* base, u32* otag, POLY_F3* poly_f3, POLY_FT3* poly_ft3,
    POLY_F4* poly_f4, POLY_FT4* textured_quads, POLY_G3* poly_g3, POLY_GT3* poly_gt3, POLY_G4* poly_g4,
    POLY_GT4* poly_gt4, LINE_F2* line_f2, LINE_F3* line_f3, LINE_F4* line_f4, LINE_G2* line_g2, LINE_G3* line_g3,
    LINE_G4* line_g4, TILE* tiles, TILE_1* tiles_1, TILE_8* tiles_8, TILE_16* tiles_16, SPRT* sprites,
    SPRT_8* sprites_8, SPRT_16* sprites_16, DR_MOVE* draw_moves, DR_AREA* draw_areas);
void status_screen_gfx_swap_context(s32 first_otag, s32 unused);
void status_screen_gfx_enqueue_oriented_textured_quad(
    RECT* rect, texture_source_t* uv, rgb24_t* color, s32 orientation, s32 semitrans, s32 ot_index);

#endif
