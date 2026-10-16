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
 * remaining_range as the residual budget; ai_move_fill_target_distance_grid reads it too. */
typedef struct target_panel {
    u8 remaining_range;
    /* Targeting frontier/targeted mark; pathing stores the step number here. */
    u8 mark;
    /* Index 0..15 of the compact movement record on the tile (set by
     * move_spread_set_frontier_flags), not a battle id. */
    u8 unit_record_index;
    /* Remaining range after landing on a rideable unit (move_spread_check_occupied_tile_standing_height);
     * cleared on the acting tile by ai_move_propagate_from_target; pathing reuses it on 512+
     * panels as the step number. */
    u8 ride_remaining_range;
    /* Largest single height step along the path, the equal-budget tie-breaker
     * (move_spread_propagate_destination, move_path_update_candidate); 0xff initial value in
     * mode 1. AI propagation sets the target to 0 and old actor tile to 0xff. */
    u8 max_height_delta;
} target_panel_t;
typedef char target_panel_size_must_be_5[(sizeof(target_panel_t) == 5) ? 1 : -1];

extern target_panel_t g_target_panel_data[BATTLE_MAP_TILE_COUNT];

/* Second name of g_target_panel_data. Pathfinding also uses the 16 auxiliary panels past
 * the 512 map panels (MOVE_PANEL_COUNT in all), all initialized by
 * move_spread_set_frontier_flags; both declarations cover only the map panels. */
extern target_panel_t g_target_panels[BATTLE_MAP_TILE_COUNT];

void target_clear_panels(void);
void target_clear_panel_spread_flags(void);
void target_area_clear_flags(void);

/* ability selection */
/* Result of target_range_set_for_action, kept in
 * unit_t.ability_preview_phase: how the action's target is chosen. */
typedef enum target_panels {
    TARGET_PANELS_INVALID = -1,
    TARGET_PANELS_UNIT_OR_TILE = 0, /* the ability can follow a unit */
    TARGET_PANELS_TILE_ONLY = 1,    /* ABILITY_SECONDARY_FLAG_2_NO_FOLLOW_TARGET */
    TARGET_PANELS_NO_TARGETING = 2, /* nothing to choose: the action is centered on the acting unit */
    TARGET_PANELS_NONE = 3,         /* the panel routine marked no tile */
} target_panels_e;

/* Directions of target_area_calculate_linear_attack_tiles. */
typedef enum target_line_direction {
    TARGET_LINE_EAST = 0,  /* +x */
    TARGET_LINE_WEST = 2,  /* -x */
    TARGET_LINE_SOUTH = 4, /* -y */
    TARGET_LINE_NORTH = 6, /* +y */
} target_line_direction_e;

s32 target_area_set_for_action(const action_command_t* source);
s32 target_range_set_for_action(const action_command_t* action);

/* eligibility and reflection */
extern s32 g_target_sort_nearest_first;

s32 action_copy_ability_targets(u8* out);
s32 target_find_unit_at_tile(s32 x, s32 y, s32 level);
s32 target_is_unit_untargetable_and_store_tile(s32 unit_id, s32* tile_index);
void action_remove_ability_target(s32 target_battle_id);
s32 target_validate_for_weapon(const action_command_t* source);

/* cursor */
void cursor_hide(void);
void cursor_move_to_action_target(const unit_t* unit);
s32 cursor_move_by_input(void);
s32 cursor_move_to_battle_id(u32 battle_id);
void cursor_move_to_unit(const unit_t* unit);
void cursor_project_tile_to_screen(VECTOR* projected, SVECTOR* position, SVECTOR* raised);
void target_select_tile(void);
void cursor_show(void);
void cursor_show_attack_preview_panel(void);
void cursor_show_right_unit_panel(void);
void cursor_show_hovered_unit_panel(void);
void cursor_move_to_selection(const unit_t* unit, const unit_t* selected_unit);

/* tile highlights */
/* target_set_tile_mark_colors modes and the g_battle_map_tile_mark_modes states:
 * one layer per map tile mark (move destination, ability range, targeted). A SHOW or HIDE
 * request recolors the current render buffer; battle_map_draw_mesh_and_weather then does the
 * other buffer and leaves the layer SHOWN or NONE. */
typedef enum battle_map_tile_mark_mode {
    BATTLE_MAP_TILE_MARK_NONE = 0, /* as a mode: restore every polygon and request the three HIDEs */
    BATTLE_MAP_TILE_MARK_SHOW_MOVE = 1,
    BATTLE_MAP_TILE_MARK_SHOW_RANGE = 2,
    BATTLE_MAP_TILE_MARK_HIDE_RANGE = 3,
    BATTLE_MAP_TILE_MARK_HIDE_MOVE = 5,
    BATTLE_MAP_TILE_MARK_SHOW_TARGETS = 7,
    BATTLE_MAP_TILE_MARK_HIDE_TARGETS = 8,
    BATTLE_MAP_TILE_MARK_MOVE_SHOWN = 10,
    BATTLE_MAP_TILE_MARK_RANGE_SHOWN = 11,
    BATTLE_MAP_TILE_MARK_TARGETS_SHOWN = 12,
} battle_map_tile_mark_mode_e;

extern u16 g_battle_map_tile_mark_x;
extern s16 g_battle_map_tile_mark_y;
extern s16 g_battle_map_tile_mark_modes[3]; /* battle_map_tile_mark_mode_e, one per tile mark */
extern render_buffer_t* g_battle_map_tile_mark_color_buffer;

s32 target_begin_tile_selection(void);
s32 cursor_calculate_tile_polygon(const SVECTOR* coords, u8 layer, SVECTOR* quad);
void cursor_draw_with_tile_glow(void);
void target_set_tile_mark_colors(s32 mode, s32 unused);
void state_enter_display_move_area(void);
void cursor_update(void);

#endif
