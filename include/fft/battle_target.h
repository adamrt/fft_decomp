#ifndef BATTLE_TARGET_H
#define BATTLE_TARGET_H

/* Battle ability range, target panels, cursor selection and tile highlights. */

#include "fft/battle_gfx.h"

/* range and selection types */
/* Five-byte range/targeting work entry at 0x80192dd8, 256 entries per level.
 * Movement-cost propagation uses remaining_range as the residual budget;
 * the AI reads it at 0x801995f4 and 0x801996ac. */
typedef struct target_panel {
    u8 remaining_range;
    /* Targeting frontier/targeted mark; pathing stores the step number here. */
    u8 mark;
    /* Index 0..15 of the compact movement record on the tile (0x80174e84),
     * not a battle id. */
    u8 unit_record_index;
    /* Remaining range after landing on a rideable unit (0x80175fa4); cleared on
     * the acting tile by 0x80178dac; pathing reuses it on 512+ panels as the step number. */
    u8 ride_remaining_range;
    /* Largest single height step along the path, the equal-budget tie-breaker
     * (0x80175958, 0x80177794); 0xff initial value in mode 1. AI propagation
     * sets the target to 0 and old actor tile to 0xff. */
    u8 max_height_delta;
} target_panel_t;
typedef char target_panel_size_must_be_5[(sizeof(target_panel_t) == 5) ? 1 : -1];

/* Directions of target_calculate_linear_attack_tiles. */
typedef enum target_line_direction {
    TARGET_LINE_EAST = 0,  /* +x */
    TARGET_LINE_WEST = 2,  /* -x */
    TARGET_LINE_SOUTH = 4, /* -y */
    TARGET_LINE_NORTH = 6, /* +y */
} target_line_direction_e;

/* Result of target_set_panels_for_action, kept in
 * battle_unit_t.ability_preview_phase: how the action's target is chosen. */
typedef enum target_panels {
    TARGET_PANELS_INVALID = -1,
    TARGET_PANELS_UNIT_OR_TILE = 0, /* the ability can follow a unit */
    TARGET_PANELS_TILE_ONLY = 1,    /* ABILITY_SECONDARY_FLAG_2_NO_FOLLOW_TARGET */
    TARGET_PANELS_NO_TARGETING = 2, /* nothing to choose: the action is centerd on the acting unit */
    TARGET_PANELS_NONE = 3,         /* the panel routine marked no tile */
} target_panels_e;

/* highlight types */
/* target_set_tile_background_color modes and the g_target_tile_mark_modes states:
 * one layer per map tile mark (move destination, ability range, targeted). A SHOW or HIDE
 * request recolors the current render buffer; map_draw_mesh_and_weather then does the
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

/* range panels */
extern target_panel_t g_target_panel_data[MAP_TILE_COUNT];

/* Second name of g_target_panel_data, unsized: 512 map panels followed
 * by 16 auxiliary panels, all initialized at 0x80174e84. */
extern target_panel_t g_target_panels[];

void battle_target_apply_vertical_fixed(s32 x, s32 y);
void battle_target_apply_vertical_tolerance(u8 ref_height, u8 tolerance, s32 single_layer);
s32 battle_target_calculate_ability_range_with_map_parameters(const unit_stats_t* unit, u8 range);
void battle_target_calculate_aoe_vertical_tolerance(s32 x, s32 y, s32 lo, s32 hi);
void battle_target_calculate_arc_range(const unit_stats_t* unit, u8 range, u8 flags);
void battle_target_calculate_linear_attack_tiles(s32 dir, s32 x, s32 y);
void battle_target_calculate_strike_lunge_range(const unit_stats_t* unit, u8 flags);
void battle_target_calculate_weapon_range(const unit_stats_t* unit);
void battle_target_clear_range_on_untargetable_tiles(void);
void battle_target_clear_panel_data(void);
void battle_target_clear_panel_spread_flags(void);
void battle_target_clear_panels_on_untargetable_tiles(void);
s32 battle_target_clear_selection_state_of_all_panels(void);
void battle_target_disable_green_panel_flags(void);
s32 battle_target_disable_green_panel_on_all_but_target_tile(const action_command_t* source);
void battle_target_remove_close_range(s32 x, s32 y, s32 range);
void battle_target_set_state_for_all_unit_panels(const unit_stats_t* unit, s32 flags);
s32 battle_target_spread_panel_to_neighbors(s32 y, s32 x);
void battle_target_spread_panels(u8 passes, s32 unused);

/* ability selection */
void battle_target_build_directional_attack_panels(const action_command_t* source, u8 mode);
s32 battle_target_calculate_for_menu_types(const action_command_t* source);
s32 battle_target_calculate_map_for_action(const action_command_t* action, u8* flags_3);
s32 battle_target_run_calculator(const action_command_t* source);
s32 battle_target_set_ability_panels(const action_command_t* source);
s32 battle_target_set_all_panels_targeted_if_targetable(void);
s32 battle_target_set_green_panels_for_action(const action_command_t* action);
s32 battle_target_set_jump_ability_panels(const action_command_t* source);
s32 battle_target_set_panels_for_action(const action_command_t* action);
s32 battle_target_set_weapon_attack_panels(const action_command_t* source);

/* eligibility and reflection */
extern s32 g_battle_sort_targets_nearest_first;

s32 battle_target_apply_reflect(unit_stats_t* unit);
void battle_target_apply_unit_team_eligibility(s32 raw_unit_id, u8 allow_allies, u8 allow_enemies, u8 aoe_is_0xff);
void battle_target_check_moldball_virus_depth(s16 ability_id);
s32 battle_target_find_unit_at_tile(s32 x, s32 y, s32 level);
s32 battle_target_is_unit_untargetable_and_store_tile(s32 unit_id, s32* tile_index);
s32 battle_target_validate_height_overlap(
    s32 attacker_id, s32 target_x, s32 target_y, s32 target_elevation, s32 target_id);
s32 battle_target_validate_lunging_target(s32 unit_id, s32 x, s32 y, s32 elevation, s32 target_id);
s32 battle_target_validate_weapon_target(const action_command_t* source);

/* target lists */
extern u8 g_target_ability_targets_list[BATTLE_UNIT_RUNTIME_SLOT_COUNT];

s32 battle_target_copy_ability_targets(u8* out);
s32 battle_target_list_units_on_panels(u8* list, const unit_stats_t* origin);
void battle_target_select_random_tile_for_random_fire_abilities(void);
void battle_target_set_coordinates_for_ability(const unit_stats_t* acting, battle_strike_work_t* out);
void battle_target_sort_list(s32 target_battle_id);

/* cursor */
void battle_target_move_cursor_to_action_target(const battle_unit_t* unit);
s32 battle_target_move_cursor_by_input(void);
s32 battle_target_move_cursor_to_battle_id(u32 battle_id);
void battle_target_move_cursor_to_unit(const battle_unit_t* unit);
void battle_target_project_cursor_tile_to_screen(VECTOR* projected, SVECTOR* position, SVECTOR* raised);
void battle_target_select_tile(void);
void battle_target_store_cursor_casting_unit_name_and_data(void);
void battle_target_store_cursor_unit_as_preview_target(void);
void battle_target_store_cursor_unit_name_and_data(void);
void battle_target_update_free_cursor_selection(const battle_unit_t* unit, const battle_unit_t* selected_unit);

/* tile highlights */
extern u16 g_target_color_tile_x;
extern s16 g_target_color_tile_y;
extern s16 g_target_tile_mark_modes[3]; /* battle_tile_mark_mode_e, one per tile mark */
extern battle_render_buffer_t* g_target_tile_color_buffer;

s32 battle_target_calculate_cursor_tile_polygon(const SVECTOR* coords, u8 layer, SVECTOR* quad);
void battle_target_calculate_tile_coords_and_glow_from_at_list(void);
void battle_target_calculate_tile_coords_with_cursor_glow(void);
s32 battle_target_set_boxes_red(void);
void battle_target_set_tile_background_color(s32 mode, s32 unused);
void battle_target_show_move_range_in_free_cursor(void);

#endif
