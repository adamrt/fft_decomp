#ifndef UNIT_VIEW_H
#define UNIT_VIEW_H

/* The unit view, the event library's panels over the battle map: the supervisor thread that keeps a view mode's panels
 * running, the unit stats panels, the unit summary and the selected-tile display. Provisional: the game has no name for
 * it. */

#include "fft/gfx.h"
#include "fft/menu.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* view modes */
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

extern s32 g_unit_view_mode;
extern u16 g_unit_view_cached_battle_id;
extern u16 g_unit_view_cached_comparison_id;

/* Requests a view mode for the units unit_a and unit_b from the supervisor thread. */
void unit_view_start(s32 mode, s32 unit_a, s32 unit_b);

/* Yields until the four panel threads (hovered and right unit stats, unit summary, tile info) have stopped. */
void unit_view_wait_for_panels(void);

/* unit stats panels */

/* The sprite page the stats panels draw their labels with (menu_sprite_page_t). */
extern RECT g_menu_sprite_page_texture_windows[5];
extern image_load_parameters_t g_menu_sprite_page_image_params[30];

/* Entries 5..7 of g_menu_sprite_page_image_params; a separate alias preserves the original
 * template base. */
extern image_load_parameters_t g_menu_sprite_page_image_param_bases[];

/* The final two entries of g_unit_view_attack_caster_stats_setup. */
extern menu_number_entry_t g_menu_projected_value_panel[2];

/* Live caster-panel image parameters followed by their defaults. */
extern image_load_parameters_t g_menu_attack_caster_quad_image_params[2][5];
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

/* unit summary */

extern RECT g_unit_view_summary_panel_rect;
extern menu_unit_number_images_t g_unit_view_summary_digit_images;
extern menu_palette_slot_t g_unit_view_summary_palettes[];
extern image_load_parameters_t g_unit_view_summary_sprite_image_params[];
extern image_load_parameters_t g_unit_view_summary_portrait_image_params;

void unit_view_summary_thread(void);

/* selected-tile display */
/* The panel describing the tile under the cursor. */

extern u8 g_unit_view_quad_group_starts[]; /* [group + 1] ends each group */
extern point16_t g_unit_view_quad_origin;
extern RECT g_unit_view_tile_info_image_location;
extern image_load_parameters_t g_unit_view_tile_info_image_params[2];
extern u16 g_unit_view_depth_display_suppressed; /* nonzero while effects hide the tile's depth number */

void unit_view_tile_info_thread(void);

/* BATTLE's effect playback suppresses the tile's depth number for the length of an ability effect. */
void unit_view_show_tile_depth(void);
void unit_view_hide_tile_depth(void);

#endif
