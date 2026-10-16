#ifndef TARGET_H
#define TARGET_H

/* Targeting: the range panels, an action's range and effect area, how it follows its target, target eligibility, and
 * choosing the target tile. */

#include "fft/battle_map.h"
#include "fft/unit.h"
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

/* range and area */
/* How an action follows its target: action_command_t.targeting_type, in the AI and menu commands
 * and in the command a unit stores. */
typedef enum target_type {
    TARGET_TYPE_TILE = 5, /* aims at target_position */
    TARGET_TYPE_UNIT = 6, /* follows the unit target_id */
} target_type_e;

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

s32 target_range_set_for_action(const action_command_t* action);
s32 target_area_set_for_action(const action_command_t* source);
s32 target_resolve_rotated_offset_layer(unit_stats_t*, s32, s32, s32, s32*, s32*, s32*);

/* eligibility */
extern s32 g_target_sort_nearest_first;

s32 target_find_unit_at_tile(s32 x, s32 y, s32 level);
s32 target_is_unit_untargetable_and_store_tile(s32 unit_id, s32* tile_index);
s32 target_validate_for_weapon(const action_command_t* source);

/* tile selection */
s32 target_begin_tile_selection(void);
void target_select_tile(void);
void target_set_tile_mark_colors(s32 mode, s32 unused);

#endif
