#ifndef FFT_BATTLE_MOVE_H
#define FFT_BATTLE_MOVE_H

#include "fft/battle.h"
#include "fft/map.h"

/* The 0x80-byte movement-path block at Misc Unit Data 0x9c..0x11b. */
typedef struct battle_walk_path {
    u8 bytes[0x80];
} battle_walk_path_t;

enum {
    BATTLE_MOVEMENT_SAVED_SOURCE_HEIGHT = 1,
    BATTLE_MOVEMENT_SAVED_DESTINATION_HEIGHT = 2,
};

/* Per-tile frontier flags produced by the movement spread pass. These are
 * distinct from map_tile_t::ceiling_depth_and_marks and are consumed by the
 * reachable-tile and path-propagation helpers. */
enum {
    FRONTIER_FLAG_LOW_CORNER = 0x02,
    FRONTIER_FLAG_UNIT_ON_TILE = 0x04,
    FRONTIER_FLAG_REACHABLE = 0x10,
    FRONTIER_FLAG_VALID_DESTINATION = 0x20,
};

/* Composite event work produced after movement. This is separate from the
 * tile-query flags in battle_move_find_result_data_t and the learned movement
 * ability bitsets. Bit 0x0200 has readers but no proven producer. */
typedef enum battle_move_post_event_flags {
    BATTLE_MOVE_POST_EVENT_CRYSTAL_OR_TREASURE = 0x0001,
    BATTLE_MOVE_POST_EVENT_MOVEMENT_BENEFIT = 0x0002,
    BATTLE_MOVE_POST_EVENT_ITEM_FOUND = 0x0004,
    BATTLE_MOVE_POST_EVENT_TRAP_TRIGGERED = 0x0008,
    BATTLE_MOVE_POST_EVENT_CHARGING_CANCEL = 0x0010,
    BATTLE_MOVE_POST_EVENT_MOVE_HP_UP = 0x0020,
    BATTLE_MOVE_POST_EVENT_MOVE_MP_UP = 0x0040,
    BATTLE_MOVE_POST_EVENT_MOVE_GET_EXP = 0x0080,
    BATTLE_MOVE_POST_EVENT_MOVE_GET_JP = 0x0100,
    BATTLE_MOVE_POST_EVENT_MOUNT_STATUS_CHANGE = 0x0400,
} battle_move_post_event_flags_e;

/* Scratch +0x22 has signed halfword arithmetic and explicit low-byte readers
 * at 0x80177880–0x80177944. The byte view preserves that truncation. */
typedef union battle_move_budget {
    s16 value;
    struct {
        u8 low;
        u8 high;
    } bytes;
} battle_move_budget_t;

/* Packed half-height offsets at 0x8018f86e. A missing presence flag preserves
 * that side's previous value when applying a saved candidate at 0x80177b64. */
typedef union battle_move_height_offsets {
    u8 byte;
    struct {
        u8 destination_present : 1;
        u8 destination_height_offset : 3;
        u8 source_present : 1;
        u8 source_height_offset : 3;
    } bits;
} battle_move_height_offsets_t;

typedef char battle_movement_height_offsets_size_must_be_1[(sizeof(battle_move_height_offsets_t) == 1) ? 1 : -1];

/* Shared movement work state addressed through 0x8018f4e4.
 *
 * Propagation at 0x80178ca4 and jump spreading at 0x80177c08 use the same
 * base. Coordinate halfwords have both signed and unsigned readers; cast
 * explicitly for signed comparisons. This is a verified prefix, not a claim
 * about the allocation's full size.
 */
typedef struct battle_move_spread_state {
    map_tile_t* current_tile;                 /* 0x00 */
    map_tile_t* destination_tile;             /* 0x04 */
    battle_target_panel_t* current_panel;     /* 0x08 */
    battle_target_panel_t* destination_panel; /* 0x0c */
    battle_target_panel_t* source_panel;      /* 0x10; selected at 0x8017567c */
    /* 0x14; base-grid or 512 + record panel, set at 0x80177614 */
    battle_target_panel_t* effective_destination_panel;
    s16 reachable_count;                            /* 0x18 */
    s16 tile_index;                                 /* 0x1a */
    s16 destination_index;                          /* 0x1c */
    s16 source_panel_index;                         /* 0x1e; terrain index or 512 + compact record index */
    s16 destination_panel_index;                    /* 0x20; terrain index or 512 + compact record index */
    battle_move_budget_t candidate_remaining_range; /* 0x22 */
    s16 jump_remaining_range;                       /* 0x24; source remaining range minus pass at 0x80176594 */
    u16 tile_x;                                     /* 0x26 */
    u16 tile_y;                                     /* 0x28 */
    s16 tile_level;                                 /* 0x2a */
    u16 level_offset;                               /* 0x2c */
    u16 row_offset;                                 /* 0x2e */
    s16 work_x;                                     /* 0x30 */
    s16 work_y;                                     /* 0x32 */
    s16 work_level;                                 /* 0x34; signed at 0x8017742c, low byte copied at 0x80177fdc */
    /* Signed side differences before water adjustment. Source is exit minus
     * opposite at 0x80175864, reversed at 0x80177358. Destination is opposite
     * minus entry at 0x80175b7c, reversed at 0x801776b0. */
    s16 source_side_height_delta;      /* 0x36 */
    s16 destination_side_height_delta; /* 0x38 */
    s16 x_step;                        /* 0x3a */
    s16 y_step;                        /* 0x3c */
    u8 _pad3e[2];
    /* 0x40-0x43: whether the source/destination is a unit's compact record
     * (a path above a unit) and that record's index, from panel byte 0x02. */
    u8 source_unit_record_flag;       /* 0x40 */
    u8 source_unit_record_index;      /* 0x41 */
    u8 destination_unit_record_flag;  /* 0x42 */
    u8 destination_unit_record_index; /* 0x43; also the per-step on-record flag in path encoding */
    u8 source_side_shift;
    u8 destination_side_shift;            /* 0x45; slope shift of the destination entry side */
    u8 source_ceiling_height;             /* 0x46; half-height units */
    u8 destination_ceiling_height;        /* 0x47; half-height units */
    u8 source_exit_height;                /* 0x48; half-height units */
    u8 source_opposite_height;            /* 0x49; extended samples at 0x80177180 leave it unchanged */
    u8 destination_entry_height;          /* 0x4a; half-height units */
    u8 destination_opposite_height;       /* 0x4b */
    u8 source_base_height_times_two;      /* 0x4c */
    u8 destination_base_height_times_two; /* 0x4d */
    u8 _pad4e[2];
    /* 0x50; height scratch: a side height compared with the exit/entry height,
     * a panel max_height_delta, or a unit record index in the jump check. */
    u8 work_height;
    u8 jump_half_height;        /* 0x51; intermediate-panel half height at 0x8017637c */
    u8 jump_slope;              /* 0x52; intermediate-panel packed slope at 0x80176388 */
    u8 jump_unit_size;          /* 0x53; acting-unit body size copied at 0x80176260 */
    u8 outer_count;             /* 0x54 */
    u8 inner_count;             /* 0x55 */
    u8 source_half_height;      /* 0x56; tile byte 3, low five bits */
    u8 destination_half_height; /* 0x57 */
    u8 source_slope;            /* 0x58; packed two-bit side multipliers */
    u8 destination_slope;       /* 0x59 */
    u8 budget_matches;          /* 0x5a; candidate budget equals predecessor budget minus step cost */
    u8 candidate_saved;         /* 0x5b; set by the save-selected-candidate step at 0x8017808c */
    u8 _pad5c[6];
    u8 frontier_max_remaining_range; /* 0x62; maximum residual queued for the next pass */
    u8 _pad63;
    /* 0x64 is |exit - entry| (0 when flying), stored into panel max_height_delta;
     * 0x65-0x67 are the best candidate's tie-breakers (0x80177e64). */
    u8 height_delta;          /* 0x64 */
    u8 best_height_delta;     /* 0x65 */
    u8 best_fly_height_delta; /* 0x66 */
    u8 best_extra_span;       /* 0x67 */
    u8 saved_x;               /* 0x68 */
    u8 saved_y;               /* 0x69 */
    u8 saved_level;           /* 0x6a */
    u8 saved_unit_record_flag;
    u8 saved_unit_record_index;
    u8 saved_remaining_range;
    u8 saved_destination_height_offset;
    u8 saved_source_height_offset;
    u8 saved_height_offset_flags;
    u8 placement_failed; /* 0x71 */
    u8 selected_source_side_shift;
    u8 previous_source_side_shift;
    u8 clearance_height; /* 0x74; ceilings below this plus unit size block the step (0x801777cc) */
} battle_move_spread_state_t;

typedef char
    battle_move_spread_tile_x_must_be_26[((unsigned long)&((battle_move_spread_state_t*)0)->tile_x == 0x26) ? 1 : -1];

typedef char battle_move_spread_frontier_must_be_62
    [((unsigned long)&((battle_move_spread_state_t*)0)->frontier_max_remaining_range == 0x62) ? 1 : -1];

typedef char battle_move_spread_placement_must_be_71
    [((unsigned long)&((battle_move_spread_state_t*)0)->placement_failed == 0x71) ? 1 : -1];

/* Sixteen seven-byte records reached through 0x8018f4e8. AI propagation
 * invalidates byte 3 at 0x80178d50–0x80178d60. The ordinary movement producer
 * at 0x80174430 stores heights in half-height units: body height includes a
 * rider adjustment, standing height includes terrain/water/Float adjustments,
 * and top height is their sum (0x80174a28–0x80174b44). */
typedef struct battle_move_record {
    u8 coords[3];       /* x, y, level | 0x80 stepping-stone flag */
    u8 unit_id_flags;   /* 0x03; 0x1f battle id, 0x20 not mountable, 0x40 not in active team, 0xff none */
    u8 body_height;     /* 0x04 */
    u8 standing_height; /* 0x05 */
    u8 top_height;      /* 0x06 */
} battle_move_record_t;

typedef char battle_movement_record_size_must_be_7[(sizeof(battle_move_record_t) == 7) ? 1 : -1];

extern battle_move_pathfind_scratch_t* g_battle_move_config_ptr;

/* The two map tiles of the step in progress, refreshed by battle_move_get_current_and_destination_tiles from
 * the unit's movement value. Callers that only forward them to the step
 * starters take them as opaque words; the geometry readers use the tile
 * fields (height at 0x02, depth_half_height at 0x03, slope_type, flags_06). */
extern map_tile_t* g_battle_move_current_tile;
extern map_tile_t* g_battle_move_destination_tile;
/* Pending battle_move_post_event_flags_e bits, dispatched in priority order. */
extern s32 g_battle_move_find_result;
extern u8* g_battle_move_frontier_flags_ptr;

/* Seven proven half-height offsets at 0x8018f4d8; the following byte is not
 * established as another element. Extended callers select indices 0..6. */
extern u8 g_battle_move_height_offsets[7];
extern battle_move_record_t* g_battle_move_records_ptr;
extern battle_move_spread_state_t* g_battle_move_scratch_pad_ptr;

/* Unnamed data, in address order. */
extern u8 g_battle_move_path_height_offsets;

/* func */
void battle_move_get_current_and_destination_tiles(
    battle_unit_misc_data_t* unit, map_tile_t** current_tile, map_tile_t** destination_tile);
void battle_move_check_occupied_tile_standing_height(s32 record_index, s32 extra_span);

/* move */
void battle_move_advance_display_unit_step(battle_unit_misc_data_t* unit);
void battle_move_apply_selected_candidate(battle_move_spread_state_t* state);
void battle_move_calculate_spread(void);
u8 battle_move_calculate_tile_ceiling(s32 x, s32 y, s32 exit_slope_shift, s32 exit_height);
s32 battle_move_calculate_tile_layer_step_offset(s32 x, s32 y, u32 layer);
s32 battle_move_check_spreading_tile_coordinates(void);
void battle_move_clear_reachable_flags_under_dead_or_jumping_units(void);
void battle_move_displace_overlapping_unit(battle_unit_misc_data_t* unit, s32 direction);
void battle_move_displace_unit_along_step_direction(battle_unit_misc_data_t* unit, s32 x, s32 y, s32 delta);
void battle_move_finalize_path_after_animation(battle_unit_misc_data_t* unit);
battle_move_effective_flags_e battle_move_get_effective_flags(const battle_stats_t* unit);
s32 battle_move_get_support_flags(battle_stats_t* unit);
s32 battle_move_has_reached_current_tile_exit_edge(s32 direction, battle_unit_misc_data_t* unit);
s32 battle_move_has_reached_destination_tile_center(s32 direction, battle_unit_misc_data_t* unit);
s32 battle_move_has_reached_destination_tile_entry_edge(s32 direction, battle_unit_misc_data_t* unit);
s32 battle_move_init_destination_geometry(s32 candidate);
void battle_move_init_knockback(battle_unit_misc_data_t* unit);
void battle_move_init_post_movement_display(void);
s32 battle_move_init_source_geometry(s32 direction);
s32 battle_move_init_source_panel(s32 direction);
void battle_move_init_spread_scratch(void);
s32 battle_move_calculate_jump_arc_velocity(
    const battle_screen_coords_t* from, const battle_screen_coords_t* to, VECTOR* out);
battle_walk_path_t* battle_move_calculate_walkto_pathing(
    s32 a, s32 b, s32 x, s32 y, s32 z, s32 destination_x, s32 destination_y, s32 destination_z);
/* Scales the three velocity words at misc-unit offsets 0x28/0x2c/0x30; the
 * definition indexes them as a word array. */
void battle_move_interpolate_partial(s32* velocity_words, s32 scale);
u32 battle_move_set_target_for_mounted_unit_and_find_item(battle_stats_t* unit);
void battle_move_start_unit_step(battle_unit_misc_data_t* unit, const map_tile_t* from, s16 facing);
void battle_move_start_float_step(battle_unit_misc_data_t* unit, const map_tile_t* from, const map_tile_t* to);
void battle_move_start_unit_step_at_climb_speed(battle_unit_misc_data_t* unit, const map_tile_t* from);
extern s32 battle_move_is_unit_moving_by_misc_id(u32 misc_id);
s32 battle_move_propagate_destination(s32 candidate, s32 extra_span);
void battle_move_return_to_previous_map_square(battle_unit_misc_data_t* unit);
void battle_move_save_selected_candidate(s32 direction, s32 candidate, s32 extra_span);
void battle_move_set_reachable_tile_flags(void);
void battle_move_set_spreading_tile_x_and_y_coordinates(void);
void battle_move_snap_axis_to_current_tile_exit_edge(s32 direction, battle_unit_misc_data_t* unit);
void battle_move_snap_axis_to_destination_tile_center(s32 direction, battle_unit_misc_data_t* unit);
void battle_move_spread_to_neighbors(void);
s32 battle_move_start_unit_walk_to(s32 misc_id, s16* coordinates, s32 elevation_addend, s32 mode, s32 speed);
void battle_move_step_unit_to_map_tile_center_no_height_change(battle_unit_misc_data_t* unit);
void battle_move_step_unit_to_map_tile_center_with_height_change(battle_unit_misc_data_t* unit);
void battle_move_transfer_tiles_height_halves_and_slope_to_scratch_pad(void);
void battle_move_update_airborne_ascent_phase(battle_unit_misc_data_t* unit);
s32 battle_move_update_candidate(s32 extra_span);
void battle_move_update_knockback_after_animation(battle_unit_misc_data_t* unit);
void battle_move_update_unit_step_to_destination_tile_center(battle_unit_misc_data_t* unit);
s32 battle_move_validate_float_fly(battle_unit_misc_data_t* unit);

void battle_move_apply_knockback(void);
s32 battle_move_calculate_teleport_chances(void);
void battle_move_check_and_spread_one_tile_around(void);
s32 battle_move_check_knockback_destination(s32 direction, s32 x, s32 y);
void battle_move_encode_path_steps(void);
s32 battle_move_get_direction(const battle_unit_misc_data_t* unit);
void battle_move_set_unit_path_flag(battle_unit_misc_data_t* unit);
void battle_move_set_unit_step_delta_center_to_edge(
    battle_unit_misc_data_t* unit, const u8* step, const map_tile_t* tile, s32 step_count);
void battle_move_set_float_step_delta_center_to_edge(
    battle_unit_misc_data_t* unit, const u8* step, const map_tile_t* tile, s32 step_count);
void battle_move_set_unit_step_delta_edge_to_center(
    battle_unit_misc_data_t* unit, const u8* step, const map_tile_t* tile, s32 step_count);
void battle_move_set_float_step_delta_edge_to_center(
    battle_unit_misc_data_t* unit, const u8* step, const map_tile_t* tile, s32 step_count);
void battle_move_set_velocity_for_contiguous_clamped_steps(
    battle_unit_misc_data_t* unit, const u8* path, const u8* step);
void battle_move_spread_horizontal_jump(void);
void battle_move_start_unit_climb_hop_step(battle_unit_misc_data_t* unit, const map_tile_t* from, const map_tile_t* to);
void battle_move_start_unit_climb_jump_step(
    battle_unit_misc_data_t* unit, const map_tile_t* from, const map_tile_t* to);
void battle_move_start_float_climb_jump_step(
    battle_unit_misc_data_t* unit, const map_tile_t* from, const map_tile_t* to);
void battle_move_start_unit_jump_step(battle_unit_misc_data_t* unit);
void battle_move_start_fly_step(battle_unit_misc_data_t* unit, s32 current_tile, s32 destination_tile);
void battle_move_transfer_target_coordinates(battle_unit_misc_data_t* src, battle_unit_misc_data_t* dst);
void battle_move_update_knockback_step(battle_unit_misc_data_t* unit);
void battle_move_update_unit_by_misc_id(void);
void battle_move_update_unit_step_to_current_tile_exit_edge(battle_unit_misc_data_t* unit);
void battle_move_update_float_step_to_current_tile_exit_edge(battle_unit_misc_data_t* unit);
void battle_move_update_float_step_to_destination_tile_center(battle_unit_misc_data_t* unit);
void battle_move_update_unit_step_to_destination_tile_entry_edge(battle_unit_misc_data_t* unit);
void battle_move_update_unit_vertical_step_to_destination_tile_center(battle_unit_misc_data_t* unit);
extern u8 g_battle_move_destination_unit_record;
extern u8 g_battle_move_effective_flags;
extern u8 g_battle_move_landing_centre_offsets[];
extern u8* g_battle_move_pathing_frontier;
extern s32 g_battle_move_pathing_resume_pass;
extern s32 g_battle_move_pathing_tile_index;
extern void (*g_battle_move_spread_preset_table[])(void);
extern u8 g_battle_move_terrain_cost;
/* Height scratch record at 0x80096238; battle_calculate_unit_height_data fills
 * it in place. */
extern battle_unit_height_data_t g_battle_move_tile_occupant_height;
extern u8 g_battle_move_weather_severity;

u8* battle_move_build_path_to_tile(s32 unit_id, s32 x, s32 y, s32 elevation);
s32 battle_move_clamp_z_to_tile_headroom(s32 value, s32 x, s32 y, u32 layer);

#endif
