#ifndef UNIT_VIEW_H
#define UNIT_VIEW_H

/* The unit view, the event library's panels over the battle map: the supervisor thread that keeps a view mode's panels
 * running, the unit stats panels, the unit summary and the selected-tile display. Provisional: the game has no name for
 * it. */

#include "psx/libgpu.h"
#include "psx/types.h"

/* view modes */
/* First argument of unit_view_start and value of g_unit_view_mode. Modes 1 through 5 include
 * selected-tile information; the retained numeric/summary configuration does not. */
typedef enum unit_view_mode {
    UNIT_VIEW_MODE_NONE = 0,           /* no panel */
    UNIT_VIEW_MODE_TILE_INFO = 1,      /* the tile info panel alone */
    UNIT_VIEW_MODE_HOVERED_UNIT = 2,   /* hovered-unit stats */
    UNIT_VIEW_MODE_UNIT_SUMMARY = 3,   /* unit summary panel and hovered-unit stats */
    UNIT_VIEW_MODE_RIGHT_UNIT = 4,     /* right-side unit stats */
    UNIT_VIEW_MODE_ATTACK_PREVIEW = 5, /* attack caster and attack target stats */
    /* Provisional: retained numeric, summary and hovered-unit configuration; no normal producer is known. */
    UNIT_VIEW_MODE_NUMERIC_SUMMARY = 6
} unit_view_mode_e;

extern s32 g_unit_view_mode;

/* Requests a view mode for the units unit_a and unit_b from the supervisor thread. */
void unit_view_start(s32 mode, s32 unit_a, s32 unit_b);

/* Yields until the four panel threads (hovered and right unit stats, unit summary, tile info) have stopped. */
void unit_view_panels_wait(void);

/* unit stats panels */
extern RECT g_unit_view_hovered_stats_rect;
extern RECT g_unit_view_right_stats_rect;
extern RECT g_unit_view_attack_caster_stats_rect;
extern RECT g_unit_view_attack_target_stats_rect;

/* Each reloads its unit and redraws its panel; a negative list index draws dashes. */
void unit_view_refresh_hovered_stats(void);
void unit_view_refresh_right_stats(void);
void unit_view_preview_caster_stats(void);
void unit_view_preview_target_stats(void);

/* unit summary */
void unit_view_summary_thread(void);

/* selected-tile display */
void unit_view_tile_info_thread(void);

/* BATTLE's effect playback suppresses the tile's depth number for the length of an ability effect. */
void unit_view_show_tile_depth(void);
void unit_view_hide_tile_depth(void);

#endif
