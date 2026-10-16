#ifndef UNIT_VIEW_H
#define UNIT_VIEW_H

/* The unit info shown over the battle map: stats panel, summary, tile info and the action preview. Provisional: the
 * game has no name for it. */

#include "fft/gfx.h"
#include "fft/menu.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

extern image_load_parameters_t g_unit_view_summary_portrait_image_params;

/* First argument of unit_view_start and value of g_unit_view_mode: the row of
 * g_unit_view_thread_sets the supervisor keeps running. Each mode from 1 up runs the selected-tile
 * display plus the panels named. */
typedef enum unit_view_mode {
    UNIT_VIEW_MODE_NONE = 0,           /* no panel */
    UNIT_VIEW_MODE_TILE_INFO = 1,      /* the tile info panel alone */
    UNIT_VIEW_MODE_HOVERED_UNIT = 2,   /* hovered-unit stats */
    UNIT_VIEW_MODE_UNIT_SUMMARY = 3,   /* unit summary panel and hovered-unit stats */
    UNIT_VIEW_MODE_RIGHT_UNIT = 4,     /* right-side unit stats */
    UNIT_VIEW_MODE_ATTACK_PREVIEW = 5, /* attack caster and attack target stats */
} unit_view_mode_e;

/* One row of g_unit_view_thread_sets, indexed by the view mode. Each of the four slots names a thread
 * entry (or -1), the scheduler slot it runs in, and the task id it must carry. Provisional. */
typedef struct menu_unit_view_thread_set {
    void (*entries[4])(void); /* 0x00 */
    s32 thread_ids[4];        /* 0x10 */
    s32 task_ids[4];          /* 0x20 */
} unit_view_thread_set_t;

typedef char unit_view_thread_set_size_must_be_48[(sizeof(unit_view_thread_set_t) == 48) ? 1 : -1];
extern unit_view_thread_set_t g_unit_view_thread_sets[];
extern s32 g_unit_view_mode;
extern u16 g_unit_view_cached_battle_id;
extern u16 g_unit_view_cached_comparison_id;

/* Requests a view mode for the units unit_a and unit_b from the supervisor thread. */
void unit_view_start(s32 mode, s32 unit_a, s32 unit_b);

/* Double-buffered 0x2ec-byte page of a unit stats panel (g_unit_view_stats_panel_pages; each mode's first page
 * comes from g_unit_view_stats_first_page_by_mode). Provisional. */
typedef struct menu_unit_status_page {
    POLY_G4 bars[3];                   /* 0x000: HP/MP/CT gauges */
    DR_MODE draw_mode_1;               /* 0x06c */
    DR_MODE draw_mode_0;               /* 0x078 */
    menu_palette_primitives_t palette; /* 0x084 */
    POLY_FT4 quads[5];                 /* 0x170 */
    SPRT sprites[7];                   /* 0x238 */
    POLY_FT4 portrait;                 /* 0x2c4 */
} unit_view_stats_page_t;

extern RECT g_unit_view_hovered_stats_rect;
extern menu_number_entry_t g_unit_view_hovered_stats_setup[10];
extern RECT g_unit_view_right_stats_rect;
extern menu_number_entry_t g_unit_view_right_stats_setup[10];
extern RECT g_unit_view_attack_caster_stats_rect;
extern menu_number_entry_t g_unit_view_attack_caster_stats_setup[8];
extern RECT g_unit_view_attack_target_stats_rect;
extern menu_number_entry_t g_unit_view_attack_target_stats_setup[8];
extern struct action_data* g_unit_view_preview_target_action;
extern u16 g_unit_view_hovered_stats_frame_count;
extern unit_view_stats_page_t g_unit_view_stats_panel_pages[];
extern u8 g_unit_view_stats_first_page_by_mode[];
extern u16 g_unit_view_stats_bounce_step;
extern u16 g_unit_view_stats_bounce_offsets[];
extern CVECTOR g_unit_view_stats_gauge_colors[];
extern image_load_parameters_t g_unit_view_stats_icon_image_params[];
extern u16 g_unit_view_stats_quad_clut_pairs[][2];
extern image_load_parameters_t* g_unit_view_stats_quad_image_params_by_mode[];
extern u16 g_unit_view_stats_sprite_clut_pairs[][2];
extern image_load_parameters_t* g_unit_view_stats_sprite_image_params_by_mode[];

/* Each reloads its unit and redraws its panel; a negative list index draws dashes. */
void unit_view_refresh_hovered_stats(void);

void unit_view_refresh_right_stats(void);
void unit_view_preview_caster_stats(void);
void unit_view_preview_target_stats(void);

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
} unit_view_summary_frame_t;

extern RECT g_unit_view_summary_panel_rect;
extern menu_unit_number_images_t g_unit_view_summary_digit_images;
extern menu_palette_slot_t g_unit_view_summary_palettes[];
extern image_load_parameters_t g_unit_view_summary_sprite_image_params[];
void unit_view_summary_thread(void);
extern menu_quad_colors_t g_unit_view_quad_colors;
extern u8 g_unit_view_quad_group_starts[]; /* [group + 1] ends each group */
extern point16_t g_unit_view_quad_origin;
extern menu_quad_piece_t g_unit_view_quad_pieces[];
extern menu_quad_page_t g_unit_view_height_display_pages[2];

/* Nonzero while effects hide the tile's depth number in the selected-tile display. */
extern u16 g_unit_view_depth_display_suppressed;

extern RECT g_unit_view_tile_info_image_location;
extern image_load_parameters_t g_unit_view_tile_info_image_params[2];

/* BATTLE's effect playback suppresses the tile's depth number for the length of an ability effect. */
void unit_view_show_tile_depth(void);

void unit_view_hide_tile_depth(void);
void unit_view_tile_info_thread(void);
void unit_view_wait_for_panels(void);

#endif
