#ifndef FFT_BATTLE_EFFECT_H
#define FFT_BATTLE_EFFECT_H

#include "fft/effect.h"
#include "fft/unit_slots.h"
#include "psx/types.h"

/* Twelve-byte on-hit effect vector at on_hit_effects_data (0x801b8a40), with a
 * saved copy at on_hit_effects_data_second_section (0x801b8a50). The
 * secondary-effect handlers store (0, 0x1000, 0) before stepping the effect
 * slots; effect script 0x27 copies it from the particle-system data. */
typedef struct battle_effect_on_hit_vector {
    s32 x;
    s32 y;
    s32 z;
} battle_effect_on_hit_vector_t;

/* Per-group effect records at 0x801b8566, stride 0x2e. The secondary-effect
 * handlers spawn while spawn_start_frame <= timer < spawn_end_frame and finish
 * once timer passes spawn_start_frame with no live slots;
 * battle_effect_update_active_slots walks count active slots. */
typedef struct battle_effect_group {
    u8 spawn_start_frame; /* 0x00 */
    u8 spawn_end_frame;   /* 0x01 */
    u8 count;             /* 0x02 */
    u8 unknown_03[7];
    /* 0x0a; emitter position[1] (vertical spawn offset); the level-up handler
     * seeds group 5's with -8, then lowers it by 3 per frame. */
    s16 position_y;
    u8 unknown_0c[0x1c];
    u8 spawns_per_frame; /* 0x28; slots one frame may start */
    u8 unknown_29[5];
} battle_effect_group_t;

typedef char battle_effect_group_size_must_be_0x2e[(sizeof(battle_effect_group_t) == 0x2e) ? 1 : -1];

/* Ten-byte target records at g_battle_effect_coord_data.targets (0x801bad10).
 * Unit targets store a misc id at 0x02; tile targets reuse that halfword for X
 * and carry map Z/Y at 0x04/0x06. Entry 16 is the caster. */
typedef struct battle_effect_target {
    u16 target_type; /* 0x00; 0 identifies a unit target */
    union {
        u8 misc_id;
        s16 tile_x;
        u8 bytes[2];   /* bytes[1]: 2 selects the child timeline's secondary-effect path */
    } id;              /* 0x02 */
    s16 map_z;         /* 0x04 */
    s16 tile_y;        /* 0x06 */
    u8 _unknown_08[2]; /* 0x08 */
} battle_effect_target_t;

typedef char battle_effect_target_size_must_be_0x0a[(sizeof(battle_effect_target_t) == 0x0a) ? 1 : -1];

/* Tile target of the current effect: placement mode 0x200, emitter modes 0x40/0xa0 and camera mode 0x1c0. */
/* Provisional view of an effect slot's position and velocity words. */
typedef struct battle_effect_charge_trail {
    DVECTOR points[7]; /* 0x00; screen positions, newest at age % 7 */
    s16 age;           /* 0x1c */
    u8 slot_id;        /* 0x1e */
    u8 _unknown_1f;
} battle_effect_charge_trail_t;

typedef struct battle_effect_charge_state {
    s16 x;                                   /* 0x0000 */
    s16 y;                                   /* 0x0002 */
    DR_MODE mode[2];                         /* 0x0004 */
    LINE_G2 lines[2][96];                    /* 0x001c */
    battle_effect_charge_trail_t trails[16]; /* 0x0f1c */
    u8 ids_a[2];                             /* 0x111c */
    u8 ids_b[14];                            /* 0x111e */
    union {
        struct {
            s16 trails;
            u8 a;
            u8 b;
        } n;
        s32 all;
    } count;            /* 0x112c */
    s32 angle;          /* 0x1130 */
    s32 speed;          /* 0x1134 */
    s32 spread_range;   /* 0x1138 */
    s32 spread_offset;  /* 0x113c */
    s32 duration;       /* 0x1140 */
    s32 radius;         /* 0x1144 */
    s32 spawn_interval; /* 0x1148 */
    s32 colour;         /* 0x114c */
    s32 max_trails;     /* 0x1150 */
    s32 fade_row;       /* 0x1154 */
    s32 done;           /* 0x1158 */
} battle_effect_charge_state_t;

/* Provisional summon-charge state: three rings of ten trailing points. */
typedef struct battle_effect_ring_state {
    s32 index;             /* 0x00; newest trail column, 0..9 */
    s32 radius;            /* 0x04 */
    s32 angle;             /* 0x08 */
    s32 angle_step;        /* 0x0c */
    SVECTOR center;        /* 0x10 */
    s32 brightness;        /* 0x18 */
    SVECTOR points[3][10]; /* 0x1c */
} battle_effect_ring_state_t;

/* Provisional zodiac-poof spawn state for the 24-slot trap group 3. */
typedef struct battle_effect_trap_state_24 {
    s32 done;   /* 0x00; reaches 1 when the effect is finished */
    s32 count;  /* 0x04; slots started so far */
    u8 ids[24]; /* 0x08 */
} battle_effect_trap_state_24_t;

/* Work area at 0x801bade0 shared by the secondary effect handlers; the
 * spell-charge, summon-charge and zodiac-poof handlers each lay it out as
 * their own state. */
typedef union battle_effect_secondary_state {
    battle_effect_charge_state_t charge;
    battle_effect_ring_state_t summon_ring;
    battle_effect_trap_state_24_t zodiac_poof;
} battle_effect_secondary_state_t;

extern battle_effect_secondary_state_t g_battle_effect_secondary_state;

extern battle_effect_target_t g_battle_effect_target_tile;

typedef struct battle_effect_coord_data {
    u16 hit_counter;                    /* 0x00; effect target count: bounds child spawns and scales spawn_delay */
    u8 palette_target_count;            /* 0x02; entries of g_battle_effect_palette_target_misc_ids */
    u8 math_skill_flag;                 /* 0x03; enables the effect flags' spawn_delay_override */
    battle_effect_target_t targets[17]; /* 0x04 */
} battle_effect_coord_data_t;

/* Provisional effect state view used by the two keyframe-action dispatchers. */
typedef struct battle_keyframe_effect_state {
    u8 _unknown_00[2];
    s16 callback_arg_02; /* Passed as callback arg0 by the 0x801a4000 dispatcher. */
    u8 _unknown_04[0x22 - 4];
    u8 callback_state[7]; /* 0x22 */
    u8 _unknown_29[0xd4 - 0x29];
    s32 callback_ptrs[7]; /* 0xd4 */
} battle_keyframe_effect_state_t;

/* 0x80-byte particle-channel keyframe table walked by
 * battle_effect_step_emitter_timeline. The halfword arrays at 0x00 and 0x4a
 * are indexed by keyframe, the byte array at 0x31 selects the emitter action,
 * and 0x7e is the highest valid keyframe index (the role
 * effect_misc_data_t::last_keyframe_index plays). Array bounds are inferred
 * from the gaps between the observed offsets. */
typedef struct battle_effect_keyframe_table {
    u16 frame_start[0x18]; /* 0x00 */
    u8 _unknown_30;        /* 0x30 */
    u8 action[0x19];       /* 0x31 */
    u16 selector[0x18];    /* 0x4a */
    u8 _unknown_7a[4];     /* 0x7a */
    s16 last_index;        /* 0x7e */
} battle_effect_keyframe_table_t;

typedef char battle_effect_keyframe_table_size_must_be_0x80[(sizeof(battle_effect_keyframe_table_t) == 0x80) ? 1 : -1];

/* Timed map background changes: the palette-track layout with a second
 * colour per keyframe for the lower gradient stop. */
typedef struct battle_effect_background_track {
    s16 duration[BATTLE_EFFECT_PALETTE_TRACK_KEYFRAMES];
    s8 color[BATTLE_EFFECT_PALETTE_TRACK_KEYFRAMES][3];
    s8 second_color[BATTLE_EFFECT_PALETTE_TRACK_KEYFRAMES][3];
    u8 preset[BATTLE_EFFECT_PALETTE_TRACK_KEYFRAMES];
    s16 count;
} battle_effect_background_track_t;

typedef char
    battle_effect_background_track_size_must_be_0x12c[(sizeof(battle_effect_background_track_t) == 0x12c) ? 1 : -1];

/* One phase's colour tracks. */
typedef struct effect_timeline_color_tracks {
    battle_effect_palette_track_t affected_units; /* 0x000 */
    battle_effect_palette_track_t caster;         /* 0x0c8 */
    battle_effect_palette_track_t target;         /* 0x190 */
    battle_effect_background_track_t screen;      /* 0x258 */
} effect_timeline_color_tracks_t;

/* Provisional view of the interpolated emitter values the Emitter Control
 * Routine passes in (0x801c878c). The secondary handlers pass a group record's
 * address minus two bytes (a raw byte offset: this view and
 * battle_effect_group_t are not yet reconciled). */
typedef struct battle_effect_emitter_values {
    u8 _unknown_00[6];   /* 0x00 */
    u16 anchor_flags;    /* 0x06; 0xe00 selects the spawn anchor */
    u16 motion_flags;    /* 0x08; 0x410 selects the orientation mode */
    s16 position[3];     /* 0x0a */
    s16 spread[3];       /* 0x10 */
    s16 angle[3];        /* 0x16 */
    u16 angle_spread[3]; /* 0x1c */
    s16 weight_min;      /* 0x22 */
    s16 weight_max;      /* 0x24 */
    u16 speed_min;       /* 0x26 */
    u16 speed_max;       /* 0x28 */
    u8 _unknown_2a[2];   /* 0x2a */
    s8 lifetime_min;     /* 0x2c */
    s8 lifetime_max;     /* 0x2d */
} battle_effect_emitter_values_t;

s32 battle_effect_spawn_particle_motion(battle_effect_emitter_values_t* values, battle_effect_motion_t* motion);

/* One textured quad of a multi-part effect sprite. */
typedef struct battle_effect_sprite_part {
    u16 flags; /* 0x00; 0x0f CLUT row, 0x10 upper CLUT bank, 0x200 semi-transparent */
    u16 tpage; /* 0x02 */
    u8 u;
    u8 v;
    u8 width;
    u8 height;
    s32 corners[4]; /* 0x08; packed x/y of the four quad corners */
} battle_effect_sprite_part_t;

typedef char battle_effect_sprite_part_size_must_be_0x18[(sizeof(battle_effect_sprite_part_t) == 0x18) ? 1 : -1];

/* Colour word (its top byte is the part count) followed by the part list. */
typedef struct battle_effect_sprite_part_set {
    union {
        u32 packed;
        struct {
            u8 r;
            u8 g;
            u8 b;
            u8 count;
        } field;
    } color;
    u8 unknown_04[4];
    battle_effect_sprite_part_t* frames[1];
} battle_effect_sprite_part_set_t;

/* Provisional arc descriptor handed to battle_effect_trace_arc_trajectory_path. */
typedef struct battle_effect_arc {
    s16 unk_00; /* 0x00 */
    u16 yaw;    /* 0x02 */
    s16 pitch;  /* 0x04 */
} battle_effect_arc_t;

/* Arc endpoint handed to the per-arc obstacle test: the unit's raised tile
 * position followed by its id and height allowance. */
typedef struct battle_arc_endpoint {
    SVECTOR position;
    s32 unit_id;
    s32 height;
} battle_arc_endpoint_t;

/* Offset to the target plus its fixed-point horizontal length. */
typedef struct battle_arc_delta {
    s32 x;
    s32 y;
    s32 z;
    s32 distance;
} battle_arc_delta_t;

/* Query read by battle_effect_calculate_arcing_trajectory: the offset to the
 * target and both endpoints. */
typedef struct battle_arc_query {
    battle_arc_delta_t delta;
    battle_arc_endpoint_t source;
    battle_arc_endpoint_t target;
} battle_arc_query_t;

/* Provisional: unit list built in the scratchpad at 0x1f800000 by
 * battle_effect_list_valid_unit_targeting_data (0x801af770) and read by the
 * obstacle tests. */
typedef struct battle_effect_obstacle_unit_list {
    s32 count;                                  /* 0x00 */
    u8 battle_ids[22];                          /* 0x04 */
    SVECTOR positions[BATTLE_UNIT_SLOT_COUNT];  /* 0x1a; tile centre and height */
    s32 sprite_heights[BATTLE_UNIT_SLOT_COUNT]; /* 0xc4 */
} battle_effect_obstacle_unit_list_t;

/* Provisional: the tile reference g_battle_effect_trajectory_tile (0x801b8b68)
 * that battle_effect_test_position_for_obstacle fills in. */
typedef struct battle_effect_tile_ref {
    s16 x;     /* 0x00 */
    s16 layer; /* 0x02 */
    s16 y;     /* 0x04 */
} battle_effect_tile_ref_t;

extern battle_effect_tile_ref_t g_battle_effect_trajectory_tile;

/* 0x1c-byte G3 polygon initialised by SetPolyG3. */
typedef struct battle_effect_g3_prim {
    u8 data[0x1c];
} battle_effect_g3_prim_t;

/* 0xc88-byte arrow work buffer of the bow-arrow (0x801b0ffc) and projectile
 * (0x801b3938) secondary handlers: two sets of four G3 and thirteen G4
 * polygons. */
typedef struct battle_effect_arrow_buffer {
    battle_effect_g3_prim_t g3[2][4]; /* 0x000 */
    POLY_G4 g4[2][13];                /* 0x0e0 */
    u8 _unknown488[0x800];
} battle_effect_arrow_buffer_t;

typedef char battle_effect_arrow_buffer_size_must_be_0xc88[(sizeof(battle_effect_arrow_buffer_t) == 0xc88) ? 1 : -1];

/* Effect misc data, the timeline section at +0x1c of an effect file. The
 * keyframe scanner, the timeline frame handler and the camera keyframe
 * dispatchers are views of this ONE object: the per-keyframe selector
 * halfwords sit at 0x806, and the camera position/target/zoom channels at
 * 0x6d4/0x73a/0x7a0 run exactly through what the scanner's view spelled as a
 * single field_6b2[170] threshold array. The for-each-target thresholds are
 * therefore 17 entries at 0x6b2, one per keyframe, immediately followed by the
 * three 17-entry three-halfword channels. The two phases' particle channels,
 * sound tracks and colour tracks follow, then the 21-entry main and cleanup
 * camera tables. */
typedef struct effect_misc_data {
    u16 _unknown_00;
    u16 phase2_duration; /* 0x002; effect script 0x1e adds it to the phase-2 start */
    u16 phase1_duration; /* 0x004 */
    u16 spawn_delay;     /* 0x006; frames between child spawns (see effect_flags_section_t) */
    u8 _unknown_08[2];
    u16 phase2_delay; /* 0x00a; battle_effect_find_next_keyframe also bounds its scan with it */
    u8 _unknown_0c[0x6a6];
    s16 frame_thresholds[17];                               /* 0x6b2; per-keyframe frame threshold */
    s16 position[17][3];                                    /* 0x6d4; camera position channel */
    s16 target[17][3];                                      /* 0x73a; camera target channel */
    s16 zoom[17][3];                                        /* 0x7a0; camera zoom channel */
    u16 selector[17];                                       /* 0x806; per-keyframe selector mask */
    s16 last_keyframe_index;                                /* 0x828; highest valid keyframe index */
    battle_effect_keyframe_table_t particle_channels[2][5]; /* 0x82a */
    u8 sound_tracks[2][3][0x1e];                            /* 0xd2a */
    effect_timeline_color_tracks_t color_tracks[2];         /* 0xdde */
    effect_camera_keyframes_21_t main;                      /* 0x14e6 */
    effect_camera_keyframes_21_t cleanup;                   /* 0x16b6 */
} effect_misc_data_t;

typedef char
    effect_misc_data_selector_offset_must_be_0x806[((unsigned long)&((effect_misc_data_t*)0)->selector == 0x806) ? 1
                                                                                                                 : -1];
typedef char
    effect_misc_data_position_offset_must_be_0x6d4[((unsigned long)&((effect_misc_data_t*)0)->position == 0x6d4) ? 1
                                                                                                                 : -1];
typedef char
    effect_misc_data_main_offset_must_be_0x14e6[((unsigned long)&((effect_misc_data_t*)0)->main == 0x14e6) ? 1 : -1];
typedef char effect_misc_data_size_must_be_0x1886[(sizeof(effect_misc_data_t) == 0x1886) ? 1 : -1];

struct battle_effect_secondary_data;
struct main_sound_resource;
struct battle_effect_secondary_init;
struct battle_unit_misc_data;
struct battle_effect_flag_prefix;

extern s16 g_battle_effect_ability_ids[];
extern u8* g_battle_effect_animation_curve_data;
extern s16 g_battle_effect_arctan_angle_mod;
extern s32 g_battle_effect_buffer_index;
extern battle_effect_coord_data_t g_battle_effect_coord_data;
extern u16 g_battle_effect_free_slot_head;
extern struct battle_effect_secondary_data* g_battle_effect_current_secondary;
extern u32 g_battle_effect_current_slot_id;
extern effect_file_header_t* g_battle_effect_data_ptrs[];
extern void* g_battle_effect_file_ptrs[];
extern u8* g_battle_effect_frame_data;
extern u8* g_battle_effect_frame_data_start;
extern s32 g_battle_effect_gravity_modifier;
extern battle_effect_group_t g_battle_effect_groups[];
extern s32 g_battle_effect_inertia_threshold;
extern s16 g_battle_effect_load_countdown;
extern effect_misc_data_t* g_battle_effect_misc_data;
extern u8* g_battle_effect_motion_header;
extern u8* g_battle_effect_nibble_table;
extern u8* g_battle_effect_timing_channels;
extern u8* g_battle_effect_parameter_sets;
extern u8* g_battle_effect_parameter_sets_start;
extern void* g_battle_effect_polygon_depth_data;
extern u16 g_battle_effect_active_slot_tail;
extern s32 g_battle_effect_primitive_sizes[];
extern s32 g_battle_effect_saved_inertia_threshold;
extern s32 g_battle_effect_sound_data_base;
extern struct main_sound_resource* g_battle_effect_sound_section_ptr;
extern u8 g_battle_effect_target_id;
extern effect_flags_section_t* g_battle_effect_flags_section;
extern battle_effect_on_hit_vector_t on_hit_effects_data;
extern battle_effect_on_hit_vector_t on_hit_effects_data_second_section;

void battle_effect_add_random_vector_offsets(const VECTOR* source, const VECTOR* ranges, VECTOR* destination);
void battle_effect_add_random_vector_offsets_and_store_q12(
    const VECTOR* source, const VECTOR* ranges, VECTOR* destination);
void battle_effect_add_vectors(const s32* first, const s32* second, s32* out);
void battle_effect_advance_map_background_track(
    battle_effect_background_track_t* track, s16* keyframe, s16* remaining_frames);
void battle_effect_build_secondary_init(
    struct battle_unit_misc_data* unit, struct battle_effect_secondary_init* effect);
s32 battle_effect_calculate_arc_height_term(s32 distance);
s32 battle_effect_calculate_arcing_trajectory(battle_arc_query_t* query, s32* arc_a, s32* arc_b);
void battle_effect_call_play(void);
void battle_effect_copy_camera_angles_and_screen_coords(void);
void battle_effect_copy_on_hit_data_to_second_section(void);
void battle_effect_copy_second_section_to_on_hit_data(void);
extern void battle_effect_finalize_secondary(u8 target_id);
s32 battle_effect_find_next_keyframe(s16 frame, u32 mask, s32* out_index, s32* out_frame);
s32 battle_effect_get_bow_shot_angle(void);
s32 battle_effect_allocate_secondary_slot(void);
s32 battle_effect_get_ninja_ball(s32 arg);
s32 battle_effect_get_on_hit_sound_index(s32 channel);
s16 battle_effect_get_random_between_s16(s16 minimum, s16 maximum);
extern s32 battle_effect_get_secondary_phase(u8 target_id);
void battle_effect_init_flags_section_pointer(s16 effect_id);
void battle_effect_init_camera_and_screen_data(void);
s32 battle_effect_init_projectile_secondary(s32 animation, struct battle_effect_secondary_init* source);
s32 battle_effect_init_secondary(s32 animation_id, s32 sprite_palette, struct battle_effect_secondary_init* source);
void battle_effect_init_system(void);
void battle_effect_interpolate_s16(s16 from, s16 to, s32 t, s16* out);
void battle_effect_interpolate_svector(const s16* from, const s16* to, s32 t, s16* out);
void battle_effect_interpolate_vector_q13(const s32* from, const s32* to, s32 t, s32* out);
s32 battle_effect_lerp_and_store(s32 start, s32 end, s32 factor, s32* out);
void battle_effect_lerp_component_q13(s32 from, s32 to, s32 t, s32* out);
void battle_effect_list_valid_unit_targeting_data(s32 first, s32 second, battle_effect_obstacle_unit_list_t** out);
void battle_effect_mask_vector_low_12_bits(const s16* src, s16* dst);
void battle_effect_load_lba_and_size(s32 effect_id, s32* lba, s32* byte_length);
void battle_effect_play_and_wait_for_animation(void);
void battle_effect_remove_secondary(u8 id);
void battle_effect_replace_flags_0006(struct battle_effect_flag_prefix* object, s32 value);
void battle_effect_reset_render_state(void);
void battle_effect_restore_inertia_threshold(void);
void battle_effect_save_inertia_threshold(void);
void battle_effect_set_evade_type_data_and_weapon_element(struct battle_unit_misc_data* unit, s32 is_bow);
void battle_effect_set_inertia_threshold(s32 threshold);
void battle_effect_set_secondary(struct battle_unit_misc_data* unit, s32 effect);
void battle_effect_set_secondary_fall_dust(struct battle_unit_misc_data* unit);
void battle_effect_set_secondary_level_up(struct battle_unit_misc_data* unit);
void battle_effect_set_secondary_splash(struct battle_unit_misc_data* unit);
void battle_effect_set_secondary_teleport(struct battle_unit_misc_data* unit);
void battle_effect_shift_vector_left_12(const s32* src, s32* dst);
void battle_effect_shift_vector_right_12(const s32* src, s32* dst);
void battle_effect_start_group(s32 group, s32 animation);
void battle_effect_step_emitter_timeline(battle_keyframe_effect_state_t* state, battle_effect_keyframe_table_t* table,
    s16* index, s16* remaining, s16* step);
void battle_effect_step_motion(battle_effect_motion_t* motion);
void battle_effect_store_first_section_of_on_hit_data(battle_effect_on_hit_vector_t* src);
s32 battle_effect_trace_arc_trajectory_path(
    SVECTOR* origin, s32* height, s32* distance, battle_effect_arc_t* arc, void* obstacles);
s32 battle_effect_test_position_for_obstacle(
    battle_effect_obstacle_unit_list_t* list, VECTOR* position, s32* out_unit, battle_effect_tile_ref_t* tile_ref);
s32 battle_effect_try_init_data(void);
void battle_effect_update_active_slots(s32 group);
s32 battle_effect_update_secondary_effects(void);

void battle_effect_add_svector_to_vector(const s16* first, const s32* second, s32* out);
void battle_effect_add_svectors(const u16* first, const u16* second, u16* out);
void battle_effect_advance_coord_target_palette_track(
    battle_effect_palette_track_t* track, s16* keyframe, s16* remaining_frames);
void battle_effect_advance_target_palette_track(
    battle_effect_palette_track_t* track, s16* keyframe, s16* remaining_frames);
s32 battle_effect_calculate_launch_velocity(void);
s32 battle_effect_calculate_arc_trajectory_term(s32 a, s32 b);
s32 battle_effect_check_arc_trajectory_between_units(u8 source_id, u8 target_id);
s32 battle_effect_check_direct_trajectory_between_units(u8 source_id, u8 target_id);
void battle_effect_convert_svector_to_vector(s16* src, s32* dst);
void battle_effect_convert_tile_coords_to_world_coords(s16* in, s32* out);
void battle_effect_copy_misc_unit_screen_location(u32 misc_id, VECTOR* destination);
void battle_effect_copy_svector(u16* src, u16* dst);
void battle_effect_draw_item_drop_sprite(VECTOR* origin, s32 item_id, s32 angle);
void battle_effect_draw_projectile_model(battle_effect_rotation_vector_t* spin, VECTOR* position,
    battle_effect_rotation_vector_t* angles, VECTOR* scale, s32 parameter);
s32 battle_effect_end_secondary_on_current_unit(void);
void battle_effect_find_projectile_xyz_along_trajectory(s32* src, s32 total, s32 current, s32* dst);
s32 battle_effect_get_animation_timing_curve_value(battle_effect_timing_entry_t* entry);
s32 battle_effect_get_timing_curve_value(battle_effect_timing_entry_t* entry);
s32 battle_effect_init_data(s32 result);
void battle_effect_init_parameter_set(effect_list_node_t* state, s32 frame_group, s32 animation_index);
void battle_effect_init_trap_animation(s32 group, u16 palette_id, battle_effect_slot_t* slot);
s32 battle_effect_interpolate_s32_cos(s32 start, s32 end, s32 duration, s32 current);
u32 battle_effect_load_ability(s32 ability_id);
void battle_effect_render_particle_sprite(effect_list_node_t* particle, s32 red, s32 green, s32 blue);
void battle_effect_resolve_target_render_coords(
    s32 add_depth, s32 target_index, battle_effect_coord_data_t* coord_data, SVECTOR* out);
void battle_effect_run_keyframe_actions(u16 flags, u8 target, battle_keyframe_effect_state_t* state);
void battle_effect_run_keyframe_actions_all_targets(u16 flags, battle_keyframe_effect_state_t* state);
s32 battle_effect_set_secondary_teleport_by_misc_id(u32 misc_id);
s32 battle_effect_set_and_validate_arc_trajectory(s32 source_id, SVECTOR* tile_position, s32 target_id);
void battle_effect_start_new(s32 effect_id, s32 target_mode, u8* target_data);
void battle_effect_store_map_center_coordinates(VECTOR* destination);
void battle_effect_submit_sprite_to_ordering_table(
    battle_effect_sprite_part_set_t* set, s16* position, s16 angle, VECTOR* zoom, u32* ot);
/* Steps the projectile along its trajectory, testing each obstacle. */
s32 battle_effect_trace_projectile_path(VECTOR* delta, SVECTOR* origin, s32* distance, void* obstacles);
void battle_effect_update_on_hit_sound_timer(u8* schedule, s16* entry_index, s16* countdown);
extern s32 g_battle_effect_arc_trajectory_height;
extern s16 g_battle_effect_arrow_arc_caster_relative_yaw;
extern s32 g_battle_effect_arrow_arc_distance;
extern s32 g_battle_effect_arrow_arc_distance_step;
extern s32 g_battle_effect_arrow_arc_height;
extern s16 g_battle_effect_arrow_arc_launch_angle;
extern s32 g_battle_effect_arrow_arc_length;
extern u16 g_battle_effect_arrow_arc_yaw;
extern s32 g_battle_effect_camera_position_duration;
extern s32 g_battle_effect_camera_position_frame;
extern s32 g_battle_effect_camera_rotation_duration; /* angle track duration */
extern s32
    g_battle_effect_camera_rotation_frame; /* angle track counter; reset by battle_camera_execute_angle_command */
extern DVECTOR g_battle_effect_charge_line_direction_offsets[];
extern u8 g_battle_effect_charge_line_fade_levels[];
extern SVECTOR g_battle_effect_corner_bottom_left;
extern SVECTOR g_battle_effect_corner_bottom_right;
extern SVECTOR g_battle_effect_corner_top_left;
extern SVECTOR g_battle_effect_corner_top_right;
extern u8 g_battle_effect_ending_secondary_function_id; /* function byte to store; 5 = nothing */
extern s16 (*g_battle_effect_instructions[])(effect_record_t*);
extern MATRIX g_battle_effect_matrix; /* 0x801bc09c */
extern s32 g_battle_effect_otag;
/* Work-kind handlers, indexed by kind_flags >> 12. The int return type is
 * load-bearing even though the result is ignored: the value-returning call is
 * a second set of $2, so sched1 no longer boosts the final `$2 = 1` to the end
 * of its block, and the pc update is allocated $3 as in the target. */
extern s32 (*g_battle_effect_particle_render_handlers[])(effect_record_t* record, effect_work_record_t* work);
extern VECTOR g_battle_effect_projectile_delta;
extern s32 g_battle_effect_projectile_length;
extern s32 g_battle_effect_projectile_progress;
extern s32 g_battle_effect_projectile_speed;
extern s16 g_battle_effect_projectile_spin_passes;
extern s16 g_battle_effect_projectile_step_count;
/* Six RGB byte triples, packed into both CLUT rows as 15-bit colours. */
extern u8 g_battle_effect_reflect_palette_colors[];
/* Secondary-animation id per ability, indexed by the low halfword of the
 * caller's packed argument. */
extern u8 g_battle_effect_secondary_by_charge_animation[];
extern u8 g_battle_effect_sound_call_counts[]; /* per-channel call counts */
/* Provisional per-column brightness weights (out of 128). */
extern u8 g_battle_effect_summon_ring_brightness[];
extern battle_effect_target_t g_battle_effect_targets[];
extern SVECTOR g_battle_effect_trajectory_position; /* final projectile position */
extern s32 g_battle_effect_trajectory_source_id;
extern VECTOR g_battle_effect_trajectory_step; /* last step's movement */
extern s32 g_battle_effect_trajectory_tile_flags;
/* Self-relative animation script table: each halfword is a byte offset from
 * the table's own base to a battle_effect_anim_script_t. Declared as bytes
 * because both uses are byte-based -- the stored pointer is the table's own
 * base plus a byte offset -- and the halfword entries are read through a cast
 * at the use site. Declaring it `u16[]` and casting the symbol to `u8*` for
 * the address instead reserves an unused 8-byte frame in
 * battle_effect_init_trap_animation. */
extern u8 g_battle_effect_trap_animation_data[];
extern u8 g_battle_effect_trap_colors[][3];
/* Provisional: per-parameter animation-id table read with the secondary
 * effect's parameter halfword. */
extern u8 g_battle_effect_zodiac_poof_animations[];

void battle_effect_add_vector_and_store_q12(const s32* delta, s32* value, s32* out);
s32 battle_effect_get_random_between(s32 value_a, s32 value_b);
void battle_effect_interpolate_svector_cos(const s16* from, const s16* to, s32 duration, s32 current, s16* out);
void battle_effect_interpolate_svector_cos_q12(const s16* from, const s16* to, s32 duration, s32 current, s16* out);
void battle_effect_interpolate_svector_linear(const s16* from, const s16* to, s32 duration, s32 current, s16* out);
void battle_effect_interpolate_vector_q13_cos(const s32* from, const s32* to, s32 duration, s32 current, s32* out);
void battle_effect_interpolate_vector_q13_cos_q12(const s32* from, const s32* to, s32 duration, s32 current, s32* out);
void battle_effect_interpolate_vector_q13_linear(const s32* from, const s32* to, s32 duration, s32 current, s32* out);

#endif
