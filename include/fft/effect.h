#ifndef EFFECT_H
#define EFFECT_H

/* Ability effects shared by BATTLE and the effect overlays (EFFECT/E###.BIN): the effect context and its targets,
 * native callbacks, particles, script records and vector math, and the overlays' native handlers with their work
 * records. */

#include "fft/effect_file.h"
#include "fft/effect_timeline.h"
#include "fft/gfx.h"
#include "fft/unit.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* callbacks */
/* Native effect handlers receive the record, callback slot, emitter and frame. Individual handlers narrow the record
 * index or ignore unused arguments. Effect script opcode 06 puts a handler in one of the callback slots, and a
 * particle keyframe whose 16-bit action value names the slot binds it to the record. While a keyframe that names the
 * slot runs, the particle channel calls the handler once a frame in place of the particle emitter: emitter_index is
 * the keyframe's emitter record, which the handler reads through its own view of effect_geometry.h, and frame counts
 * from the start of the keyframe. The slot's phase (effect_record_t.callback_phase, effect_callback_phase_e) starts
 * at INIT, where the handler allocates its work block into work_slots; it then stays at UPDATE until the channel
 * sets DESTROY two frames before the keyframe ends. */
typedef s32 (*effect_callback_t)(s32 record_index, s32 callback_slot, s32 emitter_index, s32 frame);

/* Resident catalog entries used by effect script opcodes 06 and 07. */
enum { EFFECT_NATIVE_CALLBACK_COUNT = 198 };

/* Per-handler phase byte shared by every EFFECT overlay state machine. */
typedef enum effect_callback_phase {
    EFFECT_CALLBACK_PHASE_IDLE = 0,
    EFFECT_CALLBACK_PHASE_INIT = 1,
    EFFECT_CALLBACK_PHASE_UPDATE = 2,
    EFFECT_CALLBACK_PHASE_DESTROY = 3,
} effect_callback_phase_e;

/* Native callback at 0x801c44a0 in E000 (catalog ids 0, 24, 73, 92, 121); does nothing and leaves the result unset. */
s32 effect_noop(s32 unused_record_index, s32 unused_callback_slot, s32 unused_emitter_index, s32 unused_frame);

/* targets */
enum {
    EFFECT_TARGET_COUNT_MAX = UNIT_RUNTIME_SLOT_COUNT,
    EFFECT_CASTER_ENTRY = EFFECT_TARGET_COUNT_MAX, /* index of the caster's record in effect_context_t.targets */
    /* the target index that makes effect_resolve_target_world_coords resolve the caster */
    EFFECT_TARGET_INDEX_CASTER = -1,
};

/* The target_type of an effect target record. */
typedef enum effect_target_type {
    EFFECT_TARGET_TYPE_UNIT = 0,
    EFFECT_TARGET_TYPE_TILE = 1,
} effect_target_type_e;

/* Ten-byte target records of g_effect_context.targets.
 * Unit targets store a runtime id at 0x02; tile targets reuse that 16-bit value for X
 * and carry map Z/Y at 0x04/0x06. Entry EFFECT_CASTER_ENTRY is the
 * caster. */
typedef struct effect_target {
    u16 target_type; /* 0x00; effect_target_type_e */
    union {
        struct {
            u8 runtime_id;
            u8 result_animation; /* effect_hit_result_e */
        } unit;
        SVECTOR tile; /* x, layer, y */
    } data;           /* 0x02 */
} effect_target_t;
typedef char effect_target_size_must_be_10[(sizeof(effect_target_t) == 10) ? 1 : -1];

/* result_animation of a target in an effect init record. The records built by
 * effect_start_for_action use the first four; the weapon-hit
 * records (effect_secondary_build_init_from_action) use all but REFLECTED. */
typedef enum effect_hit_result {
    EFFECT_HIT_NORMAL = 0,
    EFFECT_HIT_CRITICAL = 1,
    EFFECT_HIT_REFLECTED = 2,
    EFFECT_HIT_MISSED = 3,    /* weapon hits: evaded by an accessory, a hand or the class */
    EFFECT_HIT_NO_EFFECT = 4, /* nullified, missed outright, canceled, grasped or caught */
    EFFECT_HIT_GOLEM = 5,
    EFFECT_HIT_BREAK = 6,     /* equipment broken */
    EFFECT_HIT_BREAK_ALT = 7, /* handled like BREAK; nothing stores it */
} effect_hit_result_e;

/* Complete 0xc8-byte effect context: built on the stack for secondary effects
 * and copied whole to g_effect_context for the ability effect. */
typedef struct effect_context {
    u16 target_count;   /* 0x00; effect targets: one child record each, spawn_delay apart */
    u8 spectator_count; /* 0x02; entries in spectator_ids */
    u8 math_skill_flag; /* 0x03; enables the effect flags' spawn_delay_override */
    effect_target_t targets[EFFECT_TARGET_COUNT_MAX + 1]; /* 0x04; caster at EFFECT_CASTER_ENTRY */
    /* 0xae; at most 15 units with stats, excluding targets and the caster. */
    u8 spectator_ids[UNIT_RUNTIME_SLOT_COUNT - 1];
    u8 _unused_bd;
    SVECTOR target_tile; /* 0xbe; post-action destination tile, x/layer/y */
    u16 used_weapon_id;  /* 0xc6 */
} effect_context_t;
typedef char effect_context_size_must_be_200[(sizeof(effect_context_t) == 200) ? 1 : -1];

extern effect_context_t g_effect_context;

void effect_copy_unit_world_coords_to_vector(u32 runtime_id, VECTOR* destination);
void effect_get_world_coords_by_battle_id(
    s32 adjust_for_tile_height, u32 battle_id, effect_context_t* effect_coordinates, SVECTOR* destination);
void effect_resolve_target_world_coords(s32 add_depth, s32 target_index, effect_context_t* coord_data, SVECTOR* out);

/* particles */
/* Motion state of an effect slot (effect_slot_t +0x98) and of a
 * particle (effect_work_record_t +0x08), stepped once a frame by
 * effect_slot_step_motion and effect_particle_integrate_motion:
 * position gains velocity; velocity keeps (inertia -
 * g_effect_inertia_threshold) / inertia of itself, gains
 * acceleration * ONE / inertia and gains g_effect_gravity scaled by
 * weight (4.12 fixed point); acceleration gains acceleration_step. */
typedef struct effect_motion {
    s16 inertia;                 /* 0x00 */
    s16 weight;                  /* 0x02; scales the gravity vector */
    vec3s32_t position;          /* 0x04; 20.12 fixed */
    vec3s32_t velocity;          /* 0x10 */
    vec3s32_t acceleration;      /* 0x1c */
    vec3s32_t acceleration_step; /* 0x28 */
} effect_motion_t;
typedef char effect_motion_size_must_be_52[(sizeof(effect_motion_t) == 52) ? 1 : -1];

/* effect_list_node_t.kind: how effect_particle_render_sprite places the sprite, and the
 * FRAME_CHANGED bit it sets when the sequence picks another frame. */
enum {
    EFFECT_LIST_NODE_FRAME_CHANGED = 0x1,
    EFFECT_LIST_NODE_PLACEMENT_MASK = 0x6,
};

/* Placement subfield of effect_list_node_t.kind. */
typedef enum effect_list_node_placement {
    EFFECT_LIST_NODE_WORLD = 0x0,          /* projected position, scaled by the camera zoom */
    EFFECT_LIST_NODE_SCREEN_OFFSET = 0x2,  /* screen position from the camera translation, scaled by the zoom */
    EFFECT_LIST_NODE_WORLD_UNSCALED = 0x4, /* projected position, no zoom */
    EFFECT_LIST_NODE_SCREEN = 0x6,         /* screen position as stored, no zoom */
} effect_list_node_placement_e;

/* A sprite node's heap block (effect_render_alloc_sprite_block): an eight-byte prefix, then one
 * pointer for each sprite part. */
typedef struct effect_sprite_block {
    struct {
        rgb24_t color;
        u8 sprite_count;
    } header;
    u8 _unused_04[4];
    struct effect_sprite_part* frames[0]; /* 0x08; one pointer per sprite part */
} effect_sprite_block_t;
typedef char effect_sprite_block_size_must_be_8[sizeof(effect_sprite_block_t) == 8 ? 1 : -1];

/* One 0x24-byte sprite node of g_effect_list_nodes: the animated sprite of one particle
 * (effect_work_record_t.data). Links are node indexes into that array; node 0
 * is the sentinel. g_effect_active_list_node_head is the list head and
 * g_effect_free_list_node_head the free-list head. */
typedef struct effect_list_node {
    u16 prev;                  /* 0x00 */
    u16 next;                  /* 0x02 */
    u16 index;                 /* 0x04: this node's own array index */
    s16 kind;                  /* 0x06: EFFECT_LIST_NODE_* */
    s16 sprite_offset_x;       /* 0x08: added to x when drawn */
    s16 sprite_offset_y;       /* 0x0a: added to y when drawn */
    s16 x;                     /* 0x0c: from the owning work record's position */
    s16 y;                     /* 0x0e */
    s16 z;                     /* 0x10 */
    s16 screen_rotation_angle; /* 0x12: effect_particle_update_node_from_work stores the velocity heading here */
    u8 depth_mode;             /* 0x14: effect_sprite_depth_mode_e */
    u8 _padding_15;            /* aligns frame_timer */
    s16 frame_timer;           /* 0x16: counts down by 2; the next opcode runs at <= 0 */
    u8* sequence_data;         /* 0x18: base of the u8 animation sequence */
    u16 sequence_offset;       /* 0x1c: byte offset of the next sequence opcode */
    u8 frame_group_offset;     /* 0x1e: byte offset into the frame-group offset table */
    u8 sprite_frame_index;     /* 0x1f: frame id from the sequence opcode */
    effect_sprite_block_t* sprite_block; /* 0x20 */
} effect_list_node_t;
typedef char effect_list_node_size_must_be_36[sizeof(effect_list_node_t) == 36 ? 1 : -1];

static inline effect_sequence_instruction_t* effect_get_current_sequence_instruction(effect_list_node_t* node) {
    return (effect_sequence_instruction_t*)(node->sequence_data + node->sequence_offset);
}

/* 0x58-byte particle on an effect record's work list. The emitter initializes
 * its motion, homing and child-emitter state; the integrator advances it. */
typedef struct effect_work_record {
    struct effect_work_record* prev; /* 0x00 */
    struct effect_work_record* next; /* 0x04 */
    effect_motion_t motion;          /* 0x08 */
    vec3s16_t target;                /* 0x3c */
    s16 lifetime;                    /* 0x42; frames or EFFECT_PARTICLE_LIFETIME_ANIMATION_DRIVEN */
    u8 _unused_44;
    u8 homing_curve_selector;      /* 0x45; zero disables, otherwise curve row + 1 */
    rgb24_t color_curves;          /* 0x46; zero-based RGB animation curve indices */
    u8 _padding_49;                /* aligns homing_strength */
    s16 homing_strength;           /* 0x4a */
    s16 kind_flags;                /* 0x4c; EFFECT_EMITTER_* placement and EFFECT_WORK_KIND_* */
    u16 flags;                     /* 0x4e; EFFECT_EMITTER_FLAG_* */
    s16 curve_frame;               /* 0x50 */
    u8 child_emitter_on_death;     /* 0x52 */
    u8 child_emitter_mid_life;     /* 0x53 */
    struct effect_list_node* data; /* 0x54; sprite node, or heap work for EFFECT_WORK_KIND_HEAP_BLOCK */
} effect_work_record_t;
typedef char effect_work_record_size_must_be_88[sizeof(effect_work_record_t) == 88 ? 1 : -1];

/* script records */
/* The effect_record_t.flags bits. */
enum { EFFECT_RECORD_FLAG_ALLOCATED = 0x1 };

/* Slots of a script record, and the sizes of the script record, sprite node and work record pools. */
enum {
    EFFECT_SCRIPT_CHILD_SLOT_COUNT = 4,
    EFFECT_SCRIPT_COUNTER_COUNT = 4,
    EFFECT_RECORD_CALLBACK_SLOT_COUNT = 4,
    EFFECT_STATE_RECORD_CAPACITY = 16, /* record 0 is the list sentinel */
    EFFECT_SPRITE_NODE_CAPACITY = 256, /* node 0 is the list sentinel */
    EFFECT_WORK_RECORD_CAPACITY = 256,
};

/* One running effect script: a 0xf8-byte record of g_effect_state_records.
 * The root record runs the effect file's script from its start; the parent
 * timeline (opcode 29) starts one child record per effect target, and opcode
 * 02 starts a child the script keeps track of. Record 0 is the list sentinel,
 * so an index of 0 means none. */
typedef struct effect_record {
    s16 next_index;   /* 0x00: next record on the allocated or the available list */
    s16 record_index; /* 0x02: this record's array index */
    s16 parent_index; /* 0x04: the record that started this one; 0 for the root */
    s16 pc;           /* 0x06: byte offset of the current instruction in script */
    const u8* script; /* 0x08: borrowed script byte stream, shared with child records */
    s16 child_indices[EFFECT_SCRIPT_CHILD_SLOT_COUNT]; /* 0x0c: child records started by opcode 02 */
    s16 counters[EFFECT_SCRIPT_COUNTER_COUNT];         /* 0x14: selected by the instruction's top two parameter bits */
    s16 work_count;                                    /* 0x1c: particles on work_head */
    u16 flags;                                         /* 0x1e: EFFECT_RECORD_FLAG_ALLOCATED */
    s16 frame_counter; /* 0x20: frames the script has run, 0 on its first; the frame of script-spawned emitters */
    u8 callback_phase[EFFECT_RECORD_CALLBACK_SLOT_COUNT]; /* 0x22: effect_callback_phase_e of each callback handler */
    effect_record_timeline_t timeline;                    /* 0x26; target, frame and timeline counters */
    effect_work_record_t* work_head;                      /* 0xd0: its particles */
    effect_callback_t callbacks[EFFECT_RECORD_CALLBACK_SLOT_COUNT]; /* 0xd4: native callback handlers */
    void* work_slots[EFFECT_RECORD_CALLBACK_SLOT_COUNT];            /* 0xe4: each callback handler's allocated work */
    effect_callback_t last_native_callback; /* 0xf4: callback last called directly by script opcode 07 */
} effect_record_t;
typedef char effect_record_size_must_be_248[sizeof(effect_record_t) == 248 ? 1 : -1];

/* pc is a signed byte offset into the shared, 16-bit-aligned script stream. */
static inline const effect_script_instruction_t* effect_get_current_script_instruction(effect_record_t* record) {
    return (const effect_script_instruction_t*)(record->script + record->pc);
}

static inline u16 effect_get_current_script_header(effect_record_t* record) {
    /* Loading the 16-bit value here preserves the retail base/offset register order. */
    return *(const u16*)(record->script + record->pc);
}

/* vector math */
void effect_vector_load(const vec3s16_t* src, VECTOR* dst);
void effect_vector_load_svector(const vec3s16_t* src, SVECTOR* dst);
void effect_vector_add(const VECTOR* first, const VECTOR* second, VECTOR* out);
void effect_vector_add_svectors(const SVECTOR* first, const SVECTOR* second, SVECTOR* out);
void effect_vector_add_to_svector(const VECTOR* left, const VECTOR* right, SVECTOR* result);
void effect_vector_add_to_svector_rot_y_90(const VECTOR* left, const VECTOR* right, SVECTOR* result);
void effect_vector_add_to_svector_rot_y_180(const VECTOR* left, const VECTOR* right, SVECTOR* result);
void effect_vector_add_to_svector_rot_y_270(const VECTOR* left, const VECTOR* right, SVECTOR* result);
void effect_vector_add_and_store_q12(const VECTOR* delta, VECTOR* value, VECTOR* out);
void effect_vector_add_rotations_mod_q12(const SVECTOR* first, const SVECTOR* second, SVECTOR* out);
void effect_vector_add_random_offsets(const VECTOR* source, const VECTOR* ranges, VECTOR* destination);
void effect_vector_add_random_offsets_and_store_q12(const VECTOR* source, const VECTOR* ranges, VECTOR* destination);
void effect_vector_add_random_rotation_offsets(const SVECTOR* source, const SVECTOR* ranges, SVECTOR* destination);
void effect_vector_scale_q12(const VECTOR* src, VECTOR* dst);
void effect_vector_shift_right_12(const VECTOR* src, VECTOR* dst);
void effect_vector_mask_low_12_bits(const SVECTOR* src, SVECTOR* dst);
s16 effect_select_larger_magnitude_s16(s16 first, s16 second);
s16 effect_select_largest_magnitude_s16(s16 first, s16 second, s16 third);
s32 effect_interpolate_s32_cos(s32 start, s32 end, s32 duration, s32 current);
void effect_interpolate_svector_linear(const SVECTOR* from, const SVECTOR* to, s32 duration, s32 current, SVECTOR* out);
void effect_interpolate_svector_cos(const SVECTOR* from, const SVECTOR* to, s32 duration, s32 current, SVECTOR* out);
void effect_interpolate_svector_cos_out_and_back(
    const SVECTOR* from, const SVECTOR* to, s32 duration, s32 current, SVECTOR* out);
void effect_interpolate_vector_q13_linear(const VECTOR* from, const VECTOR* to, s32 duration, s32 current, VECTOR* out);
void effect_interpolate_vector_q13_cos(const VECTOR* from, const VECTOR* to, s32 duration, s32 current, VECTOR* out);
void effect_interpolate_vector_q13_cos_out_and_back(
    const VECTOR* from, const VECTOR* to, s32 duration, s32 current, VECTOR* out);

/* start + (end - start) * factor / 256, the division an arithmetic shift. */
s32 effect_lerp_q8(s32 start, s32 end, s32 factor);

/* map control */
s32 effect_map_freeze(s32 record_index, s32 callback_slot, s32 unused_emitter_index, s32 unused_frame);
s32 effect_map_unfreeze(s32 record_index, s32 callback_slot, s32 unused_emitter_index, s32 unused_frame);
s32 effect_map_destruction_update(s32 record_index, s32 callback_slot, s32 unused_emitter_index, s32 unused_frame);
s32 effect_map_set_3d_objects_state_1(s32 record_index, s32 callback_slot, s32 unused_emitter_index, s32 unused_frame);
s32 effect_map_set_3d_objects_state_2(s32 record_index, s32 callback_slot, s32 unused_emitter_index, s32 unused_frame);
s32 effect_map_split_mesh_and_set_3d_objects_state_3(
    s32 record_index, s32 callback_slot, s32 unused_emitter_index, s32 unused_frame);
s32 effect_map_set_timed_transform_duration(
    s32 record_index, s32 callback_slot, s32 unused_emitter_index, s32 unused_frame);

/* texture pages */
/* Fixed texture-page bits shared by the native effect renderers. */
enum {
    EFFECT_RENDER_TEXTURE_PAGE_BASE = 0x86,
    EFFECT_RENDER_TEXTURE_PAGE_BLEND_SHIFT = 5,
};

/* particle ribbon */
/* The homing ribbon renderer has short 9-point trails and long 17-point trails. */
enum {
    EFFECT_PARTICLE_RIBBON_SHORT_SLOTS = 8,
    EFFECT_PARTICLE_RIBBON_SHORT_POINTS = 9,
    EFFECT_PARTICLE_RIBBON_LONG_SLOTS = 4,
    EFFECT_PARTICLE_RIBBON_LONG_POINTS = 17,
};

extern u32 g_effect_particle_ribbon_short_brightness_table[][EFFECT_PARTICLE_RIBBON_SHORT_POINTS];
extern u32 g_effect_particle_ribbon_long_brightness_table[][EFFECT_PARTICLE_RIBBON_LONG_POINTS];

s32 effect_particle_ribbon_short_update(s32 record_index, s32 callback_slot, s32 emitter_index, s32 frame);
s32 effect_particle_ribbon_long_update(s32 record_index, s32 callback_slot, s32 emitter_index, s32 frame);

/* polar mesh */
enum {
    POLAR_MESH_QUADRANT_COUNT = 4,
    POLAR_MESH_RING_COUNT = 5,
    POLAR_MESH_SEGMENTS_PER_QUADRANT = 4,
    POLAR_MESH_QUADS_PER_QUADRANT = POLAR_MESH_RING_COUNT * POLAR_MESH_SEGMENTS_PER_QUADRANT,
    POLAR_MESH_QUAD_COUNT = POLAR_MESH_QUADRANT_COUNT * POLAR_MESH_QUADS_PER_QUADRANT,
    POLAR_MESH_OUTER_RING_OFFSET = (POLAR_MESH_RING_COUNT - 1) * POLAR_MESH_SEGMENTS_PER_QUADRANT,
    POLAR_MESH_ANGLE_STEP = ONE / (POLAR_MESH_QUADRANT_COUNT * POLAR_MESH_SEGMENTS_PER_QUADRANT),
};

/* Work block: two POLY_GT4 buffers of four quadrants of 5x4 quads. */
typedef struct effect_polar_mesh_work {
    s32 buffer;                               /* 0x0000: 0 or 1, flipped after each update */
    POLY_GT4 quads[2][POLAR_MESH_QUAD_COUNT]; /* 0x0004: quadrant * 20 + ring * 4 + segment */
    u8 _unused_2084[0x60];
    s32 scroll_u; /* 0x20e4: 20.12 fixed, wrapped to half_width */
    s32 scroll_v; /* 0x20e8: 20.12 fixed, wrapped to half_height */
    u8 _unused_20ec[0x04];
    s16 base_u;        /* 0x20f0 */
    s16 base_v;        /* 0x20f2 */
    s32 frame;         /* 0x20f4: curve column, advanced once per update */
    s32 destroy_delay; /* 0x20f8: the destroy phase waits one call before freeing */
} effect_polar_mesh_work_t;
typedef char effect_polar_mesh_work_size_must_be_8444[sizeof(effect_polar_mesh_work_t) == 8444 ? 1 : -1];

/* Overlay-local vertex brightness rows (12-bit fixed): brightness_row selects the row, the ring (0-5) the column. */
extern u32 g_effect_polar_mesh_brightness_table[][6];
extern u32 g_effect_anchored_polar_mesh_brightness_table[][6];

s32 effect_polar_mesh_update(s32 record_index, s32 callback_slot, s32 emitter_index, s32 frame);
s32 effect_anchored_polar_mesh_update(s32 record_index, s32 callback_slot, s32 emitter_index, s32 frame);

/* textured quad grids */
/* E230 links a second, 4x4 quad grid after its 8x8 one: vertex brightness rows (12-bit fixed) of that copy,
 * indexed by brightness setting and quad row edge. The shared renderer declares its own table, whose width
 * follows the build's grid size. */
extern u32 g_effect_textured_quad_grid_4x4_brightness_table[][5];

s32 effect_textured_quad_grid_4x4_update(s32 record_index, s32 callback_slot, s32 emitter_index, s32 frame);

/* ring and tube meshes */
enum {
    EFFECT_RADIAL_MESH_QUAD_CAPACITY = 64,
    RING_MESH_SEGMENTS = 32, /* quads around one ring */
    RING_MESH_QUADRANT_COUNT = 4,
    RING_MESH_SEGMENTS_PER_QUADRANT = RING_MESH_SEGMENTS / RING_MESH_QUADRANT_COUNT,
    RING_MESH_ARC_POINTS = RING_MESH_SEGMENTS_PER_QUADRANT + 1,
};

/* Shared 64-quad buffers: two 32-segment rings, an 8-by-8 ring stack or an 8-by-8 tube. */
typedef struct effect_radial_mesh_work {
    s32 buffer;                                          /* 0x0000: 0 or 1, flipped after each update */
    POLY_GT4 quads[2][EFFECT_RADIAL_MESH_QUAD_CAPACITY]; /* 0x0004: indexed by each mesh topology */
    s16 spin;                                            /* 0x1a04: ring rotation, modulo one turn */
    s16 v_scroll;                                        /* 0x1a06: 8.8 fixed, wrapped to v_span */
    s32 radius_offset;                                   /* 0x1a08: 24.8 fixed, added to the radius */
    s32 radius_speed;                                    /* 0x1a0c */
    u8 _unused_1a10[0x08];
    s32 destroy_delay; /* 0x1a18: the destroy phase waits one call before freeing */
} effect_radial_mesh_work_t;
typedef char effect_radial_mesh_work_size_must_be_6684[sizeof(effect_radial_mesh_work_t) == 6684 ? 1 : -1];

/* Overlay-local vertex brightness rows (12-bit fixed): brightness_row selects
 * the row, the lattice row (0-2) the column. */
extern u32 g_effect_ring_mesh_brightness_table[][3];

/* Overlay-local vertex brightness rows (12-bit fixed): brightness_row selects
 * the row, the lattice row (0-8) the column. */
extern u32 g_effect_ring_stack_mesh_brightness_table[][9];

/* Overlay-local vertex brightness rows (12-bit fixed). */
extern u32 g_effect_single_ring_mesh_brightness_table[][2];

/* Overlay-local vertex brightness rows (12-bit fixed): brightness_row selects
 * the row, the lattice row (0-8) the column. */
extern u32 g_effect_tube_mesh_brightness_table[][9];

/* Overlay-local ring scale per lattice row pair (12-bit fixed): row 4 - i and
 * row 4 + i share g_effect_tube_mesh_ring_scales[i]. */
extern s32 g_effect_tube_mesh_ring_scales[5];

s32 effect_ring_mesh_update(s32 record_index, s32 callback_slot, s32 emitter_index, s32 frame);
s32 effect_ring_stack_mesh_update(s32 record_index, s32 callback_slot, s32 emitter_index, s32 frame);
s32 effect_single_ring_mesh_update(s32 record_index, s32 callback_slot, s32 emitter_index, s32 frame);
s32 effect_tube_mesh_update(s32 record_index, s32 callback_slot, s32 emitter_index, s32 frame);

/* dome mesh */
/* Lattice of the spinning dome renderer: eight rows of sixteen segments. */
enum {
    DOME_MESH_ROWS = 8,
    DOME_MESH_SEGMENTS = 16,
    DOME_MESH_LATTICE_COMPONENTS = 3,
    DOME_MESH_QUAD_COUNT = DOME_MESH_ROWS * DOME_MESH_SEGMENTS,
    DOME_MESH_LATTICE_POINT_BYTES = sizeof(vec3s16_t),
    DOME_MESH_LATTICE_ROW_BYTES = (DOME_MESH_SEGMENTS + 1) * DOME_MESH_LATTICE_POINT_BYTES,
};

/* Overlay-local vertex brightness rows (12-bit fixed): brightness_row selects
 * the row, the lattice row (0-8) the column. */
extern u32 g_effect_dome_mesh_brightness_table[][9];

s32 effect_dome_mesh_update(s32 record_index, s32 callback_slot, s32 emitter_index, s32 frame);

/* map vertex particles */
typedef struct effect_map_vertex_location {
    u8 vertex_index;    /* one of the mesh part's four cached vertices */
    u8 mesh_part_index; /* zero-based among map mesh parts 1-8 */
} effect_map_vertex_location_t;

/* Eight animated map mesh parts each provide four cached vertices. */
enum {
    EFFECT_PARTICLE_VERTEX_EMITTER_COUNT = 16,
    EFFECT_MAP_VERTEX_PART_COUNT = 8,
    EFFECT_MAP_VERTEX_COUNT_PER_PART = 4,
    EFFECT_MAP_VERTEX_WORK_BYTES = EFFECT_MAP_VERTEX_PART_COUNT * EFFECT_MAP_VERTEX_COUNT_PER_PART * sizeof(VECTOR),
};

extern effect_map_vertex_location_t g_effect_banish_vertex_locations[12];

/* Three entries, though effect_altima_transformation_map_vertex_particles_update bounds the vertex
 * group by 12 like the Banish handler: groups 4 to 12 would read past the table. */
extern effect_map_vertex_location_t g_effect_altima_transformation_vertex_locations[3];

s32 effect_banish_map_vertex_particles_update(s32 record_index, s32 callback_slot, s32 unused_emitter_index, s32 frame);
s32 effect_altima_transformation_map_vertex_particles_update(
    s32 record_index, s32 callback_slot, s32 unused_emitter_index, s32 frame);

/* tentacle mesh */

extern u32 g_effect_tentacle_mesh_brightness_table[][9];

s32 effect_tentacle_mesh_update(s32 record_index, s32 callback_slot, s32 emitter_index, s32 frame);
s32 effect_particle_trails_update(s32 record_index, s32 callback_slot, s32 emitter_index, s32 frame);

/* wave mesh */

/* Overlay-local vertex brightness rows (12-bit fixed): brightness_row selects
 * the row, the quad row (0-8) the column. */
extern u32 g_effect_wave_mesh_brightness_table[][9];

s32 effect_wave_mesh_update(s32 record_index, s32 callback_slot, s32 emitter_index, s32 frame);

#endif
