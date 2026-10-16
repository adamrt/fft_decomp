#ifndef BATTLE_EFFECT_H
#define BATTLE_EFFECT_H

/* BATTLE ability effects: the effect context, script records and particles shared with the effect overlays. */

#include "fft/battle_effect_file.h"
#include "fft/battle_effect_timeline.h"
#include "fft/gfx.h"
#include "fft/unit.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* callbacks */
/* Native effect handlers receive the record, callback slot, emitter and frame.
 * Individual handlers narrow the record index or ignore unused arguments. */
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

/* targets */
enum {
    EFFECT_TARGET_COUNT_MAX = BATTLE_UNIT_RUNTIME_SLOT_COUNT,
    EFFECT_CASTER_ENTRY = EFFECT_TARGET_COUNT_MAX, /* index of the caster's record in effect_secondary_init_t.targets */
    /* the target index that makes battle_effect_resolve_target_world_coords resolve the caster */
    EFFECT_TARGET_INDEX_CASTER = -1,
};

/* target_type of an effect target record. */
typedef enum effect_target_type {
    EFFECT_TARGET_TYPE_UNIT = 0,
    EFFECT_TARGET_TYPE_TILE = 1,
} effect_target_type_e;

/* Ten-byte target records of g_battle_effect_context.targets.
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
 * battle_effect_store_targets_and_display_types use the first four; the weapon-hit
 * records (battle_effect_build_secondary_init_from_action) use all but REFLECTED. */
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
 * and copied whole to g_battle_effect_context for the ability effect. */
typedef struct effect_secondary_init {
    u16 target_count;   /* 0x00; effect targets: one child record each, spawn_delay apart */
    u8 spectator_count; /* 0x02; entries in spectator_ids */
    u8 math_skill_flag; /* 0x03; enables the effect flags' spawn_delay_override */
    effect_target_t targets[EFFECT_TARGET_COUNT_MAX + 1]; /* 0x04; caster at EFFECT_CASTER_ENTRY */
    /* 0xae; living units that are neither targets nor the caster; also accessed
     * through the separately bound g_battle_effect_palette_target_runtime_ids. */
    u8 spectator_ids[14];
    effect_target_t target_tile; /* 0xbc; post-action destination tile */
    u16 used_weapon_id;          /* 0xc6 */
} effect_secondary_init_t;
typedef char effect_secondary_init_size_must_be_200[(sizeof(effect_secondary_init_t) == 200) ? 1 : -1];

extern effect_secondary_init_t g_battle_effect_context;
extern u8 g_battle_effect_palette_target_runtime_ids[];
/* Tile target of the current effect: emitter origin CURSOR_TILE, homing CURSOR_TILE and camera source
 * CURSOR_TILE. */
extern effect_target_t g_battle_effect_target_tile;

void battle_effect_copy_unit_world_coords_to_vector(u32 runtime_id, VECTOR* destination);
void battle_effect_get_world_coords_by_battle_id(
    s32 adjust_for_tile_height, u32 battle_id, effect_secondary_init_t* effect_coordinates, SVECTOR* destination);
void battle_effect_resolve_target_world_coords(
    s32 add_depth, s32 target_index, effect_secondary_init_t* coord_data, SVECTOR* out);

/* post-action messages */
/* Queue of post-action "effect message" popups (g_battle_action_post_effect_msgs), filled by
 * battle_action_queue_post_effect_messages_for_unit and drained newest-first by
 * battle_action_show_next_effect_message. The stride is 8: the producer indexes the array
 * directly and the consumer walks it by `idx * 8`, and only these three fields are ever read or
 * written. */
typedef struct battle_post_effect_msg {
    u8 code;           /* 0x00 message code passed to menu_init_system_function */
    u8 unit;           /* 0x01 runtime ID of the acting unit */
    u8 _padding_02[2]; /* 0x02; aligns value */
    s32 value;         /* 0x04 reported amount */
} battle_post_effect_msg_t;

/* particles */
/* Motion state of an effect slot (effect_slot_t +0x98) and of a
 * particle (effect_work_record_t +0x08), stepped once a frame by
 * battle_effect_step_motion and battle_effect_integrate_particle_motion:
 * position gains velocity; velocity keeps (inertia -
 * g_battle_effect_inertia_threshold) / inertia of itself, gains
 * acceleration * ONE / inertia and gains g_battle_effect_gravity scaled by
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

/* effect_list_node_t.kind: how battle_effect_render_particle_sprite places the sprite, and the
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

/* A sprite node's heap block (battle_effect_alloc_sprite_block): an eight-byte prefix, then one
 * pointer for each sprite part. */
typedef struct effect_sprite_block {
    struct {
        rgb24_t color;
        u8 sprite_count;
    } header;
    u8 _unused_04[4];
    struct effect_sprite_part* frames[0]; /* 0x08; one pointer per sprite part */
} effect_sprite_block_t;
typedef char assert_effect_sprite_block_prefix_size[sizeof(effect_sprite_block_t) == 8 ? 1 : -1];

/* One 0x24-byte sprite node of g_battle_effect_list_nodes: the animated sprite of one particle
 * (effect_work_record_t.data). Links are node indexes into that array; node 0
 * is the sentinel. g_battle_effect_active_list_node_head is the list head and
 * g_battle_effect_free_list_node_head the free-list head. */
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
    s16 screen_rotation_angle; /* 0x12: battle_effect_update_node_from_work stores the velocity heading here */
    u8 depth_mode;             /* 0x14: effect_sprite_depth_mode_e */
    u8 _padding_15;            /* aligns frame_timer */
    s16 frame_timer;           /* 0x16: counts down by 2; the next opcode runs at <= 0 */
    u8* sequence_data;         /* 0x18: base of the u8 animation sequence */
    u16 sequence_offset;       /* 0x1c: byte offset of the next sequence opcode */
    u8 frame_group_offset;     /* 0x1e: byte offset into the frame-group offset table */
    u8 sprite_frame_index;     /* 0x1f: frame id from the sequence opcode */
    effect_sprite_block_t* sprite_block; /* 0x20 */
} effect_list_node_t;

static __inline__ effect_sequence_instruction_t* effect_get_current_sequence_instruction(effect_list_node_t* node) {
    return (effect_sequence_instruction_t*)(node->sequence_data + node->sequence_offset);
}
typedef char assert_effect_list_node_size[sizeof(effect_list_node_t) == 36 ? 1 : -1];

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
typedef char assert_effect_work_record_size[sizeof(effect_work_record_t) == 88 ? 1 : -1];

/* script records */
/* effect_record_t.flags. */
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

/* One running effect script: a 0xf8-byte record of g_battle_effect_state_records.
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

/* pc is a signed byte offset into the shared, 16-bit-aligned script stream. */
static __inline__ const effect_script_instruction_t* effect_get_current_script_instruction(effect_record_t* record) {
    return (const effect_script_instruction_t*)(record->script + record->pc);
}

static __inline__ u16 effect_get_current_script_header(effect_record_t* record) {
    /* Loading the 16-bit value here preserves the retail base/offset register order. */
    return *(const u16*)(record->script + record->pc);
}

typedef char assert_effect_record_size[sizeof(effect_record_t) == 248 ? 1 : -1];

/* vector math */
void battle_effect_add_random_rotation_offsets(const SVECTOR* source, const SVECTOR* ranges, SVECTOR* destination);
void battle_effect_add_random_vector_offsets(const VECTOR* source, const VECTOR* ranges, VECTOR* destination);
void battle_effect_add_random_vector_offsets_and_store_q12(
    const VECTOR* source, const VECTOR* ranges, VECTOR* destination);
void battle_effect_add_rotation_vectors_mod_q12(const SVECTOR* first, const SVECTOR* second, SVECTOR* out);
void battle_effect_add_svectors(const SVECTOR* first, const SVECTOR* second, SVECTOR* out);
void battle_effect_add_vector_and_store_q12(const VECTOR* delta, VECTOR* value, VECTOR* out);
void battle_effect_add_vectors(const VECTOR* first, const VECTOR* second, VECTOR* out);
void battle_effect_add_vectors_to_svector(const VECTOR* left, const VECTOR* right, SVECTOR* result);
void battle_effect_add_vectors_to_svector_rot_y_180(const VECTOR* left, const VECTOR* right, SVECTOR* result);
void battle_effect_add_vectors_to_svector_rot_y_270(const VECTOR* left, const VECTOR* right, SVECTOR* result);
void battle_effect_add_vectors_to_svector_rot_y_90(const VECTOR* left, const VECTOR* right, SVECTOR* result);
s32 battle_effect_interpolate_s32_cos(s32 start, s32 end, s32 duration, s32 current);
void battle_effect_interpolate_svector_cos(
    const SVECTOR* from, const SVECTOR* to, s32 duration, s32 current, SVECTOR* out);
void battle_effect_interpolate_svector_cos_q12(
    const SVECTOR* from, const SVECTOR* to, s32 duration, s32 current, SVECTOR* out);
void battle_effect_interpolate_svector_linear(
    const SVECTOR* from, const SVECTOR* to, s32 duration, s32 current, SVECTOR* out);
void battle_effect_interpolate_vector_q13_cos(
    const VECTOR* from, const VECTOR* to, s32 duration, s32 current, VECTOR* out);
void battle_effect_interpolate_vector_q13_cos_q12(
    const VECTOR* from, const VECTOR* to, s32 duration, s32 current, VECTOR* out);
void battle_effect_interpolate_vector_q13_linear(
    const VECTOR* from, const VECTOR* to, s32 duration, s32 current, VECTOR* out);
void battle_effect_load_svector(const vec3s16_t* src, SVECTOR* dst);
void battle_effect_load_vector(const vec3s16_t* src, VECTOR* dst);
void battle_effect_mask_vector_low_12_bits(const SVECTOR* src, SVECTOR* dst);
s16 battle_effect_select_larger_magnitude_s16(s16 first, s16 second);
s16 battle_effect_select_largest_magnitude_s16(s16 first, s16 second, s16 third);
void battle_effect_scale_vector_fixed12(const VECTOR* src, VECTOR* dst);
void battle_effect_shift_vector_right_12(const VECTOR* src, VECTOR* dst);

#endif
