#ifndef MENU_UNIT_VIEW_H
#define MENU_UNIT_VIEW_H

/* The unit view: the panels over the battle map for the units and tile under the cursor (the selected-tile
 * display, the unit stats panels and the unit summary) and the supervisor thread that keeps a mode's panels
 * running. */

#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/menu_panel.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

struct action_data;

/* view modes */
/* First argument of menu_start_unit_view_thread and value of g_menu_unit_view_mode: the row of
 * g_menu_unit_view_thread_sets the supervisor keeps running. Each mode from 1 up runs the selected-tile
 * display plus the panels named. */
typedef enum unit_view_mode {
    UNIT_VIEW_MODE_NONE = 0,           /* no panel */
    UNIT_VIEW_MODE_TILE_INFO = 1,      /* the tile info panel alone */
    UNIT_VIEW_MODE_HOVERED_UNIT = 2,   /* hovered-unit stats */
    UNIT_VIEW_MODE_UNIT_SUMMARY = 3,   /* unit summary panel and hovered-unit stats */
    UNIT_VIEW_MODE_RIGHT_UNIT = 4,     /* right-side unit stats */
    UNIT_VIEW_MODE_ATTACK_PREVIEW = 5, /* attack caster and attack target stats */
} unit_view_mode_e;

/* One row of g_menu_unit_view_thread_sets, indexed by the view mode. Each of the four slots names a thread
 * entry (or -1), the scheduler slot it runs in, and the task id it must carry. Provisional. */
typedef struct menu_unit_view_thread_set {
    void (*entries[4])(void); /* 0x00 */
    s32 thread_ids[4];        /* 0x10 */
    s32 task_ids[4];          /* 0x20 */
} menu_unit_view_thread_set_t;
typedef char menu_unit_view_thread_set_size_must_be_48[(sizeof(menu_unit_view_thread_set_t) == 48) ? 1 : -1];

extern menu_unit_view_thread_set_t g_menu_unit_view_thread_sets[];
extern s32 g_menu_unit_view_mode;
extern u16 g_menu_unit_view_cached_battle_id;
extern u16 g_menu_unit_view_cached_comparison_id;

/* Requests a view mode for the units unit_a and unit_b from the supervisor thread. */
void menu_start_unit_view_thread(s32 mode, s32 unit_a, s32 unit_b);

/* unit stats panels */
/* The four unit stats panels are one thread body, menu_run_unit_stats_panel, run in a mode: the panel slides
 * in, redraws each frame and slides out on the stop request. */

/* The mode argument of menu_run_unit_stats_panel: which of the four
 * unit stats panels the thread draws; each value has one caller. */
typedef enum unit_stats_panel {
    UNIT_STATS_PANEL_HOVERED_UNIT = 0,  /* menu_refresh_hovered_unit_stats_display */
    UNIT_STATS_PANEL_ATTACK_CASTER = 1, /* menu_preview_attack_caster_stats */
    UNIT_STATS_PANEL_ATTACK_TARGET = 2, /* menu_preview_attack_target_stats; shows the reaction ability name */
    UNIT_STATS_PANEL_RIGHT_UNIT = 3,    /* menu_refresh_right_unit_stats_display */
} unit_stats_panel_e;

/* A stats panel's two draw offsets (menu_run_unit_stats_panel). */
typedef struct menu_unit_status_offsets {
    menu_status_panel_draw_offset_t a; /* 0x00 */
    menu_status_panel_draw_offset_t b; /* 0x10 */
} menu_unit_status_offsets_t;

/* Double-buffered 0x2ec-byte page of a unit stats panel (g_menu_unit_status_panel_pages; each mode's first page
 * comes from g_menu_unit_status_first_page_by_mode). Provisional. */
typedef struct menu_unit_status_page {
    POLY_G4 bars[3];                   /* 0x000: HP/MP/CT gauges */
    DR_MODE draw_mode_1;               /* 0x06c */
    DR_MODE draw_mode_0;               /* 0x078 */
    menu_palette_primitives_t palette; /* 0x084 */
    POLY_FT4 quads[5];                 /* 0x170 */
    SPRT sprites[7];                   /* 0x238 */
    POLY_FT4 portrait;                 /* 0x2c4 */
} menu_unit_status_page_t;

extern RECT g_menu_hovered_unit_stats_rect;
extern menu_number_entry_t g_menu_hovered_unit_stats_setup[10];
extern RECT g_menu_right_unit_stats_rect;
extern menu_number_entry_t g_menu_right_unit_stats_setup[10];
extern RECT g_menu_attack_caster_stats_rect;
extern menu_number_entry_t g_menu_attack_caster_stats_setup[8];
extern RECT g_menu_attack_target_stats_rect;
extern menu_number_entry_t g_menu_attack_target_stats_setup[8];
extern struct action_data* g_menu_preview_target_action;
extern u16 g_menu_hovered_stats_frame_count;
extern menu_unit_status_page_t g_menu_unit_status_panel_pages[];
extern u8 g_menu_unit_status_first_page_by_mode[];
extern u16 g_menu_unit_status_bounce_step;
extern u16 g_menu_unit_status_bounce_offsets[];
extern CVECTOR g_menu_unit_status_gauge_colors[];
extern image_load_parameters_t g_menu_unit_status_icon_image_params[];
extern u16 g_menu_unit_status_quad_clut_pairs[][2];
extern image_load_parameters_t* g_menu_unit_status_quad_image_params_by_mode[];
extern u16 g_menu_unit_status_sprite_clut_pairs[][2];
extern image_load_parameters_t* g_menu_unit_status_sprite_image_params_by_mode[];
/* The sprite page the stats panels draw their labels with (menu_sprite_page_t). */
extern RECT g_menu_sprite_page_texture_windows[5];
extern image_load_parameters_t g_menu_sprite_page_image_params[30];
/* Entries 5..7 of g_menu_sprite_page_image_params; a separate alias preserves the original
 * template base. */
extern image_load_parameters_t g_menu_sprite_page_image_param_bases[];

/* Each reloads its unit and redraws its panel; a negative list index draws dashes. */
void menu_refresh_hovered_unit_stats_display(void);
void menu_refresh_right_unit_stats_display(void);
void menu_preview_attack_caster_stats(void);
void menu_preview_attack_target_stats(void);

/* unit summary */
/* Double-buffered 0xf0-byte frame of the unit summary panel. Provisional. */
typedef struct menu_unit_summary_frame {
    SPRT sprites[7];                    /* 0x00: [0] name/job, [1]-[3] numbers, [6] zodiac */
    DR_MODE draw_mode_0;                /* 0x8c */
    DR_MODE draw_mode_1;                /* 0x98 */
    menu_palette_primitives_t* palette; /* 0xa4 */
    DR_OFFSET draw_offset_a;            /* 0xa8 */
    DR_OFFSET draw_offset_b;            /* 0xb4 */
    point16_t offset_a;                 /* 0xc0 */
    point16_t offset_b;                 /* 0xc4 */
    POLY_FT4 portrait;                  /* 0xc8 */
} menu_unit_summary_frame_t;

extern RECT g_menu_unit_summary_panel_rect;
extern menu_unit_number_images_t g_menu_unit_summary_digit_images;
extern menu_palette_slot_t g_menu_unit_summary_palettes[];
extern image_load_parameters_t g_menu_unit_summary_sprite_image_params[];

void menu_run_unit_summary_panel_thread(void);

/* selected-tile display */
/* The panel describing the tile under the cursor. */

/* 12-byte textured-quad piece (g_menu_quad_pieces) that menu_build_quad_pieces turns into a POLY_GT4:
 * texture origin, size and screen position relative to g_menu_quad_origin. Provisional. */
typedef struct menu_quad_piece {
    u8 u; /* 0x00 */
    u8 _unused_01;
    s8 v;           /* 0x02: texture v + 0x78 */
    u8 _padding_03; /* aligns w */
    s16 w;          /* 0x04 */
    s16 h;          /* 0x06 */
    s16 x;          /* 0x08 */
    s16 y;          /* 0x0a */
} menu_quad_piece_t;

/* The four vertex colors menu_build_quad_pieces applies (g_menu_quad_colors); the leading 32-bit value is
 * untyped. Provisional. */
typedef struct menu_quad_colors {
    u32 _unused_00;
    CVECTOR vertex[4]; /* 0x04; one color per POLY_GT4 corner */
} menu_quad_colors_t;

/* One of the two 0x144-byte packet pages of the display (g_menu_height_display_pages): five Gouraud textured
 * quads, two sprites and the two draw-mode packets that select their texture windows. Provisional. */
typedef struct menu_quad_page {
    POLY_GT4 quads[5]; /* 0x000 */
    SPRT sprites[2];   /* 0x104 */
    DR_MODE modes[2];  /* 0x12c */
} menu_quad_page_t;
typedef char menu_quad_page_size_must_be_324[sizeof(menu_quad_page_t) == 324 ? 1 : -1];

extern menu_quad_colors_t g_menu_quad_colors;
extern u8 g_menu_quad_group_starts[]; /* [group + 1] ends each group */
extern point16_t g_menu_quad_origin;
extern menu_quad_piece_t g_menu_quad_pieces[];
extern menu_quad_page_t g_menu_height_display_pages[2];
/* Nonzero while effects hide the tile height in the selected-tile display. */
extern u16 g_menu_height_display_suppressed;
extern RECT g_menu_tile_info_image_location;
extern image_load_parameters_t g_menu_tile_info_image_params[2];

/* BATTLE's effect playback suppresses the tile's depth number for the length of an ability effect. */
void menu_enable_height_display(void);
void menu_suppress_height_display(void);
void menu_run_selected_tile_info_display_thread(void);

#endif
