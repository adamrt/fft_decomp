#ifndef CURSOR_H
#define CURSOR_H

/* The map cursor: its tile, screen position and movement, the tile glow and selection cursor drawn over its tile, and
 * the unit panels it brings up. */

#include "fft/gfx.h"
#include "fft/unit.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* position and movement */
/* g_cursor_input_mode: how the directional buttons map to map directions. The first two are
 * the Cursor Movement option's settings; FREE_CURSOR sends the four buttons along the screen's axes. */
typedef enum cursor_input_mode {
    CURSOR_INPUT_OPTION_A = 0,
    CURSOR_INPUT_OPTION_B = 1,
    CURSOR_INPUT_FREE_CURSOR = 2
} cursor_input_mode_e;

extern s32 g_cursor_x;
extern s32 g_cursor_y;
extern s32 g_cursor_z;
extern u16 g_cursor_screen_x;
extern s16 g_cursor_screen_y;
extern s32 g_cursor_visible;
extern s32 g_cursor_input_mode;
extern s32 g_cursor_input_mode_saved;

void cursor_update(void);
void cursor_show(void);
void cursor_hide(void);
s32 cursor_move_by_input(void);
void cursor_move_to_unit(const unit_t* unit);
s32 cursor_move_to_battle_id(u32 battle_id);
void cursor_move_to_action_target(const unit_t* unit);
void cursor_move_to_selection(const unit_t* unit, const unit_t* selected_unit);
void cursor_project_tile_to_screen(VECTOR* projected, SVECTOR* position, SVECTOR* raised);

/* drawing */
extern palette_t g_cursor_tile_glow_palettes[2];

void cursor_draw_with_tile_glow(void);
s32 cursor_calculate_tile_polygon(const SVECTOR* coords, u8 layer, SVECTOR* quad);
void cursor_build_tile_glow(s32 palette, s32 shape, SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, SVECTOR* v3, u32* ot);
void cursor_cycle_tile_glow_palettes(void);
void cursor_draw_map_selection(SVECTOR* base, u32* ot);

/* unit panels */
void cursor_show_hovered_unit_panel(void);
void cursor_show_right_unit_panel(void);
void cursor_show_attack_preview_panel(void);

#endif
