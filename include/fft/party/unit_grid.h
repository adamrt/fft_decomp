#ifndef UNIT_GRID_H
#define UNIT_GRID_H

/* The unit grid of roster portraits that Formation, the shops' fitting room, the Soldier Office and BUNIT share. */

#include "psx/types.h"

/* grid */
s32 unit_grid_check_scroll(void);

/* WORLD's Formation screen: move the grid cursor and update the selected unit. */
void unit_grid_update_selection(void);

#endif
