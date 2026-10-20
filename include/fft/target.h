#ifndef TARGET_H
#define TARGET_H

/* Targeting: the range panels, an action's range and effect area, how it follows its target, target eligibility, and
 * choosing the target tile. */

#include "fft/battle_map.h"
#include "fft/unit.h"
#include "psx/types.h"

/* range panels */
/* Five-byte range and targeting work entry, 256 per level. Movement-cost propagation uses
 * remaining_range as the residual budget; target_distance_grid_fill reads it too. */
typedef struct target_panel {
    u8 remaining_range;
    /* Targeting frontier/targeted mark; pathing stores the step number here. */
    u8 mark;
    /* Move spreading stores the compact movement-record index 0..15 (0 is valid).
     * Relocation search reuses this byte as 0 free, 1 occupied or reserved. */
    u8 unit_record_index;
    /* Remaining range after landing on a rideable unit (spread_occupied_tile_standing_height_check);
     * cleared on the acting tile by ai_move_propagate_from_target; pathing reuses it on 512+
     * panels as the step number. */
    u8 ride_remaining_range;
    /* Largest single height step along the path, the equal-budget tie-breaker
     * (spread_destination_propagate, path_candidate_update); 0xff initial value in
     * mode 1. AI propagation sets the target to 0 and old actor tile to 0xff. */
    u8 max_height_delta;
} target_panel_t;
typedef char target_panel_size_must_be_5[(sizeof(target_panel_t) == 5) ? 1 : -1];

/* Map panels followed by 16 auxiliary movement panels, initialized together. */
extern target_panel_t g_target_panels[BATTLE_MAP_TILE_COUNT + 16];

void target_clear_panels(void);
void target_clear_panel_spread_flags(void);
void target_area_clear_flags(void);

/* range and area */
/* How an action follows its target: action_command_t.targeting_type, in the AI and menu commands
 * and in the command a unit stores. */
typedef enum target_type {
    TARGET_TYPE_TILE = 5, /* aims at target_position */
    TARGET_TYPE_UNIT = 6  /* follows the unit target_id */
} target_type_e;

/* Result of target_range_set_for_action, kept in
 * unit_runtime_t.ability_preview_phase: how the action's target is chosen. */
typedef enum target_panels {
    TARGET_PANELS_INVALID = -1,
    TARGET_PANELS_UNIT_OR_TILE = 0, /* the ability can follow a unit */
    TARGET_PANELS_TILE_ONLY = 1,    /* ABILITY_SECONDARY_FLAG_2_NO_FOLLOW_TARGET */
    TARGET_PANELS_NO_TARGETING = 2, /* nothing to choose: the action is centered on the acting unit */
    TARGET_PANELS_NONE = 3          /* the panel routine marked no tile */
} target_panels_e;

/* Directions of area_linear_attack_tiles_calculate. */
typedef enum target_line_direction {
    TARGET_LINE_EAST = 0,  /* +x */
    TARGET_LINE_WEST = 2,  /* -x */
    TARGET_LINE_SOUTH = 4, /* -y */
    TARGET_LINE_NORTH = 6  /* +y */
} target_line_direction_e;

target_panels_e target_range_set_for_action(const action_command_t* action);
s32 target_area_set_for_action(const action_command_t* source);
s32 target_resolve_rotated_offset_layer(
    unit_stats_t* unit, s32 dx, s32 dy, s32 flags, s32* out_x, s32* out_y, s32* out_layer);

/* eligibility */
extern s32 g_target_sort_nearest_first;

s32 target_find_unit_at_tile(s32 x, s32 y, s32 level);
s32 target_is_unit_untargetable_and_store_tile(s32 unit_id, s32* tile_index);
s32 target_validate_for_weapon(const action_command_t* source);

/* tile selection */
target_panels_e target_tile_selection_start(void);
void target_tile_select(void);
void target_tile_mark_colors_set(s32 mode, s32 unused);

#endif
