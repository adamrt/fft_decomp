#ifndef BATTLE_TARGET_H
#define BATTLE_TARGET_H

/* Battle targeting: the range panels of an action, target eligibility, the targeting cursor and the
 * tile highlights. */

#include "fft/battle_gfx.h"
#include "fft/battle_unit.h"
#include "fft/map.h"
#include "fft/unit.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* range panels */
/* Five-byte range and targeting work entry, 256 per level. Movement-cost propagation uses
 * remaining_range as the residual budget; battle_ai_fill_target_distance_grid reads it too. */
typedef struct target_panel {
    u8 remaining_range;
    /* Targeting frontier/targeted mark; pathing stores the step number here. */
    u8 mark;
    /* Index 0..15 of the compact movement record on the tile (set by
     * battle_move_set_tile_flags_for_pathfinding), not a battle id. */
    u8 unit_record_index;
    /* Remaining range after landing on a rideable unit (battle_move_check_occupied_tile_standing_height);
     * cleared on the acting tile by battle_ai_propagate_target_movement; pathing reuses it on 512+
     * panels as the step number. */
    u8 ride_remaining_range;
    /* Largest single height step along the path, the equal-budget tie-breaker
     * (battle_move_propagate_destination, battle_move_update_candidate); 0xff initial value in
     * mode 1. AI propagation sets the target to 0 and old actor tile to 0xff. */
    u8 max_height_delta;
} target_panel_t;
typedef char target_panel_size_must_be_5[(sizeof(target_panel_t) == 5) ? 1 : -1];

extern target_panel_t g_battle_target_panel_data[MAP_TILE_COUNT];

/* Second name of g_battle_target_panel_data. Pathfinding also uses the 16 auxiliary panels past
 * the 512 map panels (MOVE_PANEL_COUNT in all), all initialized by
 * battle_move_set_tile_flags_for_pathfinding; both declarations cover only the map panels. */
extern target_panel_t g_battle_target_panels[MAP_TILE_COUNT];

void battle_target_clear_panel_data(void);
void battle_target_clear_panel_spread_flags(void);
void battle_target_disable_green_panel_flags(void);

/* ability selection */
/* Result of battle_target_set_panels_for_action, kept in
 * battle_unit_t.ability_preview_phase: how the action's target is chosen. */
typedef enum target_panels {
    TARGET_PANELS_INVALID = -1,
    TARGET_PANELS_UNIT_OR_TILE = 0, /* the ability can follow a unit */
    TARGET_PANELS_TILE_ONLY = 1,    /* ABILITY_SECONDARY_FLAG_2_NO_FOLLOW_TARGET */
    TARGET_PANELS_NO_TARGETING = 2, /* nothing to choose: the action is centered on the acting unit */
    TARGET_PANELS_NONE = 3,         /* the panel routine marked no tile */
} target_panels_e;

/* Directions of battle_target_calculate_linear_attack_tiles. */
typedef enum target_line_direction {
    TARGET_LINE_EAST = 0,  /* +x */
    TARGET_LINE_WEST = 2,  /* -x */
    TARGET_LINE_SOUTH = 4, /* -y */
    TARGET_LINE_NORTH = 6, /* +y */
} target_line_direction_e;

s32 battle_target_calculate_for_menu_types(const action_command_t* source);
s32 battle_target_set_panels_for_action(const action_command_t* action);

/* eligibility and reflection */
extern s32 g_battle_sort_targets_nearest_first;

s32 battle_target_copy_ability_targets(u8* out);
s32 battle_target_find_unit_at_tile(s32 x, s32 y, s32 level);
s32 battle_target_is_unit_untargetable_and_store_tile(s32 unit_id, s32* tile_index);
void battle_target_remove_ability_target(s32 target_battle_id);
s32 battle_target_validate_weapon_target(const action_command_t* source);

/* cursor */
void battle_target_hide_cursor(void);
void battle_target_move_cursor_to_action_target(const battle_unit_t* unit);
s32 battle_target_move_cursor_by_input(void);
s32 battle_target_move_cursor_to_battle_id(u32 battle_id);
void battle_target_move_cursor_to_unit(const battle_unit_t* unit);
void battle_target_project_cursor_tile_to_screen(VECTOR* projected, SVECTOR* position, SVECTOR* raised);
void battle_target_select_tile(void);
void battle_target_show_cursor(void);
void battle_target_show_attack_preview_panel(void);
void battle_target_show_right_unit_panel(void);
void battle_target_show_hovered_unit_panel(void);
void battle_target_move_cursor_to_selection(const battle_unit_t* unit, const battle_unit_t* selected_unit);

/* tile highlights */
/* battle_target_set_tile_mark_colors modes and the g_battle_target_tile_mark_modes states:
 * one layer per map tile mark (move destination, ability range, targeted). A SHOW or HIDE
 * request recolors the current render buffer; battle_map_draw_mesh_and_weather then does the
 * other buffer and leaves the layer SHOWN or NONE. */
typedef enum battle_tile_mark_mode {
    BATTLE_TILE_MARK_NONE = 0, /* as a mode: restore every polygon and request the three HIDEs */
    BATTLE_TILE_MARK_SHOW_MOVE = 1,
    BATTLE_TILE_MARK_SHOW_RANGE = 2,
    BATTLE_TILE_MARK_HIDE_RANGE = 3,
    BATTLE_TILE_MARK_HIDE_MOVE = 5,
    BATTLE_TILE_MARK_SHOW_TARGETS = 7,
    BATTLE_TILE_MARK_HIDE_TARGETS = 8,
    BATTLE_TILE_MARK_MOVE_SHOWN = 10,
    BATTLE_TILE_MARK_RANGE_SHOWN = 11,
    BATTLE_TILE_MARK_TARGETS_SHOWN = 12,
} battle_tile_mark_mode_e;

extern u16 g_battle_target_color_tile_x;
extern s16 g_battle_target_color_tile_y;
extern s16 g_battle_target_tile_mark_modes[3]; /* battle_tile_mark_mode_e, one per tile mark */
extern battle_render_buffer_t* g_battle_target_tile_color_buffer;

s32 battle_target_begin_tile_selection(void);
s32 battle_target_calculate_cursor_tile_polygon(const SVECTOR* coords, u8 layer, SVECTOR* quad);
void battle_target_draw_cursor_with_tile_glow(void);
void battle_target_set_tile_mark_colors(s32 mode, s32 unused);
void battle_target_show_move_range_in_free_cursor(void);
void battle_target_update_cursor(void);

#endif
