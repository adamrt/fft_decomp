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
/* Double-buffered 0x2d0-byte frame of the Status screen panel (g_status_screen_frames). The first 0x2a4
 * bytes are laid out as unit_panel_buffer_t (draw modes, sprites, tiles and lines),
 * and the trailing draw-area pair follows menu_panel_frame_t. Provisional. */
typedef struct status_screen_frame {
    DR_MODE frame_draw_mode;            /* 0x000; MENU_TEXTURE_PAGE_FRAME */
    DR_MODE text_draw_mode;             /* 0x00c; MENU_TEXTURE_PAGE_TEXT_IMAGES */
    DR_MODE image_draw_mode;            /* 0x018; MENU_TEXTURE_PAGE_ITEM_ICONS */
    SPRT sprites[24];                   /* 0x024: 19 panel sprites, then 5 item icons */
    TILE tiles[2];                      /* 0x204 */
    LINE_F2 lines[8];                   /* 0x224 */
    menu_palette_primitives_t* palette; /* 0x2a4 */
    scaled_draw_area_pair_t draw_areas; /* 0x2a8 */
} status_screen_frame_t;
typedef char status_screen_frame_size_must_be_720[sizeof(status_screen_frame_t) == 720 ? 1 : -1];

extern status_screen_frame_t g_status_screen_frames[2];
extern RECT g_status_screen_panel_rect;  /* panel box */
extern RECT g_status_screen_cursor_rect; /* cursor source */
extern menu_palette_primitives_t g_status_screen_palette;
extern RECT g_status_screen_text_rect_a; /* first text image */
extern RECT g_status_screen_text_rect_b; /* second text image */
extern image_load_parameters_t g_status_screen_row_image_params_a[4];
extern image_load_parameters_t g_status_screen_row_image_params_b[4];
extern image_load_parameters_t g_status_screen_sprite_image_params_a[19];
extern image_load_parameters_t g_status_screen_sprite_image_params_b[19];
extern image_load_parameters_t g_status_screen_item_icon_image_params[5];

void status_screen_thread(void);

/* numbers panel */
/* The Status screen's numeric side panel draws its fifteen numbers into three images once, then the frame and
 * eighteen sprites every frame. This is its double-buffered 0x1ac-byte record
 * (g_status_screen_numbers_panel_records). Provisional. */
typedef struct status_screen_numbers_record {
    DR_MODE mode0; /* 0x000 */
    DR_MODE mode1; /* 0x00c */
    /* 0x018; menu_init_scaled_draw_area_packets */
    scaled_draw_area_pair_t draw_areas;
    SPRT sprites[18];                   /* 0x040 */
    menu_palette_primitives_t* palette; /* 0x1a8 */
} status_screen_numbers_record_t;

extern status_screen_numbers_record_t g_status_screen_numbers_panel_records[2];
extern menu_number_entry_t g_status_screen_numbers_entries[15];
extern RECT g_status_screen_numbers_frame_rect;
extern image_load_parameters_t g_status_screen_numbers_image_params[18];
extern menu_palette_slot_t g_status_screen_numbers_palettes[];
extern RECT g_status_screen_numbers_text_rect_a;
extern RECT g_status_screen_numbers_text_rect_b;
extern RECT g_status_screen_numbers_text_rect_c;
extern RECT g_status_screen_numbers_panel_texture_window;
extern RECT g_status_screen_numbers_texture_window; /* texture window for the menu page */

void status_screen_init_numbers_frame(RECT* bounds, menu_palette_primitives_t* menu);
void status_screen_submit_numbers_frame(menu_palette_primitives_t* display);
void status_screen_numbers_thread(void);

/* status list */
/* The status panel's list of the unit's current statuses, in two columns. */
extern menu_scroll_text_layout_t g_status_screen_status_list_layout;
extern u16 g_status_screen_status_list_row_actions[8];

/* Text ids of the unit's current statuses in the status panel's two columns, filled by
 * status_screen_build_status_list. */
extern s16 g_unit_panel_status_left_text_ids[20];
extern s16 g_unit_panel_status_right_text_ids[20];

void status_screen_build_status_list(void);

/* overlay screens */
void status_screen_jobs_thread(void);               /* runs JOBSTTS while its state is nonzero */
void status_screen_open_mounted_unit(void);         /* DEBUGCHR's view of a selected rider or mount */
void debugchr_mounted_unit_entrypoint(s32 unit_id); /* DEBUGCHR's read-only rider/mount Status loop */

/* job pages (JOBSTTS) */
/* Job and ability data copied into JOBSTTS.OUT for the selected unit. */
typedef struct status_screen_unit_jobs {
    s16 job_id;                /* 0x00 */
    s16 entd_slot;             /* 0x02 */
    u16 primary_skillset;      /* 0x04 */
    u8 unit_flags;             /* 0x06 */
    u8 character_identity;     /* 0x07 */
    unit_career_data_t career; /* 0x08; unlocks, learned abilities, levels and JP */
} status_screen_unit_jobs_t;
typedef char status_screen_unit_jobs_size_must_be_158[(sizeof(status_screen_unit_jobs_t) == 158) ? 1 : -1];

extern s8 g_status_screen_active_page; /* menu_job_list_step_e; each page's function returns the next one */
extern u8 g_status_screen_initialized;
extern u8 g_status_screen_previous_event_mode;
extern status_screen_unit_jobs_t* g_status_screen_unit_data[];

/* Advance JOBSTTS one frame; return its ordering table while open, or zero on close. */
u32* jobstts_entrypoint(s32 unit, void* unused_otag);
void status_screen_init_unit_jobs(s32 unit_id);

/* menu display (JOBSTTS) */
/* The opcode that ends a JOBSTTS menu display stream. */
enum {
    STATUS_SCREEN_SCRIPT_END = 0x16,
};

enum {
    STATUS_SCREEN_LIST_COMMAND_VISIBLE_ROWS_INDEX = 6,
};

extern u8 g_status_screen_menu_palette_image[];

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

extern status_screen_gfx_context_t* g_status_screen_gfx_context_base;
extern status_screen_gfx_context_t g_status_screen_gfx_contexts[];
extern u16 g_status_screen_gfx_draw_area_capacity;
extern u16 g_status_screen_gfx_draw_move_capacity;
extern s16 g_status_screen_gfx_line_f2_count;
extern s16 g_status_screen_gfx_poly_f4_count;
extern s16 g_status_screen_gfx_poly_g4_count;
extern s16 g_status_screen_gfx_tile_count;
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
