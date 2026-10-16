#ifndef BATTLE_EFFECT_PARTICLE_H
#define BATTLE_EFFECT_PARTICLE_H

/* BATTLE effect particles: emitter motion, the work-record and sprite-node pools, and their rendering. */

#include "fft/battle_effect.h"
#include "fft/battle_effect_geometry.h"
#include "fft/gfx.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* particle motion */
/* The particle updater reserves -1 for animation-driven lifetime. */
enum { EFFECT_PARTICLE_LIFETIME_ANIMATION_DRIVEN = -1 };

extern MATRIX g_battle_effect_emitter_matrix;
/* Particle gravity; secondary handlers save it while using (0, ONE, 0). */
extern vec3s32_t g_battle_effect_gravity;
extern s32 g_battle_effect_gravity_modifier;
extern s32 g_battle_effect_gravity_scale;
extern s32 g_battle_effect_inertia_threshold;
/* Vertex work allocated by Banish/Altima and filled by the map renderer. */
extern VECTOR* g_battle_effect_map_vertices;
extern MATRIX g_battle_effect_particle_heading_matrix;
extern vec3s32_t g_battle_effect_saved_gravity;
extern s32 g_battle_effect_saved_inertia_threshold;

void battle_effect_integrate_particle_motion(effect_work_record_t* list);
void battle_effect_interpolate_emitter_spawn_position(const effect_geometry_record_t* source, s32 factor, VECTOR* out);
void battle_effect_interpolate_emitter_spawn_spread(const effect_geometry_record_t* source, s32 factor, VECTOR* out);
void battle_effect_interpolate_emitter_velocity_base_angles(
    const effect_geometry_record_t* source, s32 factor, VECTOR* out);
void battle_effect_interpolate_emitter_velocity_direction_spread(
    const effect_geometry_record_t* source, s32 factor, VECTOR* out);
void battle_effect_restore_gravity(void);
void battle_effect_restore_inertia_threshold(void);
void battle_effect_save_gravity(void);
void battle_effect_save_inertia_threshold(void);
void battle_effect_set_gravity(const vec3s32_t* src);
void battle_effect_set_inertia_threshold(s32 threshold);
/* parent is the particle a child emitter spawns from, read only for the
 * EFFECT_EMITTER_ORIGIN_PARENT_PARTICLE origin: the particle update passes
 * the particle, the EFFECT overlay's particle handlers a record built on
 * their stack that holds a position, and the timeline none. The emitter index
 * arrives as a full 32-bit value. The function is declared with an int result it
 * never sets and no caller reads; a void declaration changes effect_script.c's code. */
s32 battle_effect_spawn_emitter_particles(s16 record_index, s32 frame, s32 emitter_index, effect_work_record_t* parent);

/* particle pools */
/* Effect work pool: g_battle_effect_work_records is the 256-record allocation, g_battle_effect_free_work_record_head
 * the free-chain head, g_battle_effect_work_record_count the in-use count and g_battle_effect_work_record_peak its
 * peak. */
extern effect_work_record_t* g_battle_effect_work_records;
extern effect_work_record_t* g_battle_effect_free_work_record_head;
extern s32 g_battle_effect_work_record_count;
extern s32 g_battle_effect_work_record_peak;
extern effect_list_node_t g_battle_effect_list_nodes[EFFECT_SPRITE_NODE_CAPACITY + 1];
extern u16 g_battle_effect_active_list_node_head;
extern u16 g_battle_effect_free_list_node_head;
extern s32 g_battle_effect_sprite_count;
extern s32 g_battle_effect_sprite_count_peak;

void battle_effect_free_work_record(s16 record_index, effect_work_record_t* work);
effect_work_record_t* battle_effect_get_list_node(s16 record_index, s16 requested_index);

/* rendering */
/* Ordering-table depth clamp shared by the EFFECT renderers: a depth below MIN
 * becomes MIN and one at or beyond LIMIT (the OT slot count) becomes MAX. */
enum {
    EFFECT_OT_DEPTH_MIN = 4,
    EFFECT_OT_DEPTH_MAX = 383,
    EFFECT_OT_DEPTH_LIMIT = 384,
    EFFECT_SPRITE_OT_DEPTH_MAX = 382, /* the farthest depth of a particle sprite */
};

/* primitive_type of battle_effect_alloc_primitive: an index into g_battle_effect_primitive_sizes. */
typedef enum effect_primitive_type {
    EFFECT_PRIMITIVE_FT4 = 0,
    EFFECT_PRIMITIVE_DR_MODE = 1,
} effect_primitive_type_e;

/* Size of the per-effect arena used by battle_effect_alloc_primitive. */
enum { EFFECT_PRIMITIVE_BUFFER_SIZE = 32768 };

/* Initial Z coordinate of each particle sprite's four corner vectors. */
enum { EFFECT_CORNER_Z_DEFAULT = 512 };

/* The four corners of a particle sprite quad. Four-byte alignment preserves the original 32-bit
 * copies into x/y. */
extern SVECTOR g_battle_effect_corner_bottom_left __attribute__((aligned(4)));
extern SVECTOR g_battle_effect_corner_bottom_right __attribute__((aligned(4)));
extern SVECTOR g_battle_effect_corner_top_left __attribute__((aligned(4)));
extern SVECTOR g_battle_effect_corner_top_right __attribute__((aligned(4)));
extern s32 g_battle_effect_frame_parity;
extern s32 g_battle_effect_frame_start_prim_buffer_offset;
extern MATRIX g_battle_effect_matrix;
extern u32* g_battle_effect_otag;
/* Work-kind handlers, indexed by kind_flags >> 12. The int return type is
 * load-bearing even though the result is ignored: the value-returning call is
 * a second set of $2, so sched1 no longer boosts the final `$2 = 1` to the end
 * of its block, and the pc update is allocated $3 as in the target. */
extern s32 (*g_battle_effect_particle_render_handlers[])(effect_record_t* record, effect_work_record_t* work);
extern u8* g_battle_effect_prim_buffer;
extern s32 g_battle_effect_prim_buffer_offset;
extern s32 g_battle_effect_primitive_sizes[3];

s32 battle_effect_get_prim_buffer_offset(void);
void battle_effect_reset_render_state(void);
void battle_effect_start_render_frame(void);

#endif
