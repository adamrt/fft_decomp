#ifndef UNIT_GRID_H
#define UNIT_GRID_H

/* The unit grid of roster portraits that Formation, the shops' fitting room, the Soldier Office and BUNIT share. */

#include "psx/types.h"

/* grid */
/* WORLD's and BUNIT's own bodies: one frame of the grid (cursor, browsing, banner) and its row backgrounds. */
#ifdef FFT_EVENT_BUNIT
s32 unit_grid_update(s16 allow_banner_toggle, s16 allow_browse, s16 show_stat_panel, s16 allow_stat_cycle,
    s16 stat_mode, s16 buttons, s16 locked);
void unit_grid_draw_background(s32 y);
#else
s32 unit_grid_update(s16 background_y, s16 toggle_banner, s16 allow_browse, s16 input, s16 locked, s32 (*get_mode)(s32),
    s32 (*get_shade)(s32));
void unit_grid_draw_background(s32 y, s32 unused_1);
#endif
s32 unit_grid_check_scroll(void);

/* WORLD's Formation screen: move the grid cursor and update the selected unit. */
void unit_grid_update_selection(void);

void unit_grid_init(s32 unused_selected_index);

#endif
