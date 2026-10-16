#ifndef BATTLE_EFFECT_PROJECTILE_H
#define BATTLE_EFFECT_PROJECTILE_H

/* BATTLE effect targets, elements, projectiles and effect models. */

#include "fft/battle_effect.h"

/* target_type of an effect target record. */
typedef enum effect_target_type {
    EFFECT_TARGET_TYPE_UNIT = 0,
    EFFECT_TARGET_TYPE_TILE = 1,
} effect_target_type_e;

/* The projectile models of g_battle_effect_model_files. */
typedef enum effect_model {
    EFFECT_MODEL_ARROW = 0,
    EFFECT_MODEL_STONE = 1,
    EFFECT_MODEL_REFLECT = 2,
    EFFECT_MODEL_SHURIKEN = 3, /* drawn for the thrown items of ITEM_TYPE_SHURIKEN */
} effect_model_e;

/* What battle_effect_get_element_index makes of an element mask: the position of its first element in
 * g_battle_effect_element_masks, 0 for no element. It picks the CLUT of the weapon hit and
 * spell charge particles and the effect of a thrown ball. */
typedef enum effect_element_index {
    EFFECT_ELEMENT_INDEX_NONE = 0,
    EFFECT_ELEMENT_INDEX_FIRE = 1,
    EFFECT_ELEMENT_INDEX_WATER = 2,
    EFFECT_ELEMENT_INDEX_EARTH = 3,
    EFFECT_ELEMENT_INDEX_WIND = 4,
    EFFECT_ELEMENT_INDEX_HOLY = 5,
    EFFECT_ELEMENT_INDEX_DARK = 6,
    EFFECT_ELEMENT_INDEX_ICE = 7,
    EFFECT_ELEMENT_INDEX_LIGHTNING = 8,
} effect_element_index_e;

/* Opcodes (top header byte) of the effect model commands that battle_effect_decode_next_model_command
 * decodes: each draws the primitive it is named for. Nothing in the code tells the two GT3 opcodes
 * apart except that battle_effect_draw_projectile_model draws only the second. */
enum {
    EFFECT_MODEL_COMMAND_G3 = 0x31,
    EFFECT_MODEL_COMMAND_GT3_35 = 0x35,
    EFFECT_MODEL_COMMAND_GT3_37 = 0x37,
    EFFECT_MODEL_COMMAND_G4 = 0x39,
};

/* targets */
extern effect_secondary_init_t g_battle_effect_context;
extern u8 g_battle_effect_palette_target_runtime_ids[];
/* Tile target of the current effect: emitter origin CURSOR_TILE, homing CURSOR_TILE and camera source
 * CURSOR_TILE. */
extern effect_target_t g_battle_effect_target_tile;
void battle_effect_build_secondary_init(struct battle_unit* unit, struct effect_secondary_init* effect);
void battle_effect_copy_unit_world_coords_to_vector(u32 runtime_id, VECTOR* destination);
void battle_effect_get_world_coords_by_battle_id(
    s32 adjust_for_tile_height, u32 battle_id, effect_secondary_init_t* effect_coordinates, SVECTOR* destination);
void battle_effect_resolve_target_world_coords(
    s32 add_depth, s32 target_index, effect_secondary_init_t* coord_data, SVECTOR* out);

/* elements */
extern u8 g_battle_effect_element_masks[];
s32 battle_effect_get_element_index(s32 elements);

/* projectiles */
extern s32 g_battle_effect_arc_trajectory_height;
extern s16 g_battle_effect_arctan_angle_mod;
extern s16 g_battle_effect_arrow_arc_caster_relative_yaw;
extern s32 g_battle_effect_arrow_arc_distance;
extern s32 g_battle_effect_arrow_arc_distance_step;
extern s32 g_battle_effect_arrow_arc_height;
extern s16 g_battle_effect_arrow_arc_launch_angle;
extern s32 g_battle_effect_arrow_arc_length;
extern u16 g_battle_effect_arrow_arc_yaw;
extern VECTOR g_battle_effect_projectile_delta;
extern s32 g_battle_effect_projectile_length;
extern s32 g_battle_effect_projectile_progress;
extern s32 g_battle_effect_projectile_speed;
extern s16 g_battle_effect_projectile_spin_passes;
extern s16 g_battle_effect_projectile_step_count;
extern SVECTOR g_battle_effect_trajectory_destination;
extern s32 g_battle_effect_trajectory_hit_unit_id;
extern SVECTOR g_battle_effect_trajectory_origin;
extern SVECTOR g_battle_effect_trajectory_position; /* final projectile position */
extern s32 g_battle_effect_trajectory_source_id;
extern VECTOR g_battle_effect_trajectory_step; /* last step's movement */
extern s32 g_battle_effect_trajectory_step_count;
extern map_tile_coordinates_t g_battle_effect_trajectory_tile;
extern s32 g_battle_effect_trajectory_tile_flags;
s32 battle_effect_check_arc_trajectory_between_units(u8 source_id, u8 target_id);
s32 battle_effect_check_direct_trajectory_between_units(u8 source_id, u8 target_id);
s32 battle_effect_check_direct_trajectory_to_target(s32 source_id, SVECTOR* tile_position, s32 target_id);
s32 battle_effect_get_bow_shot_angle(void);
s32 battle_effect_set_and_validate_arc_trajectory(s32 source_id, SVECTOR* tile_position, s32 target_id);
void battle_effect_set_arrow_trajectory(s32 source_id, SVECTOR* tile_position, s32 target_id);

/* models */
extern s32 g_battle_effect_model_command_word_offset;
extern psyq_tmd_packet_t* g_battle_effect_model_commands;
extern psyq_tmd_header_t* g_battle_effect_model_files[];
extern SVECTOR* g_battle_effect_model_normals;
extern psyq_tmd_object_t* g_battle_effect_model_object;
extern SVECTOR* g_battle_effect_model_vertices;
extern MATRIX g_battle_effect_projectile_matrix;
void battle_effect_decode_next_model_command(effect_decoded_model_command_t* out);
s32 battle_effect_init_model_sections(psyq_tmd_header_t* resource, s32 unused_1);

#endif
