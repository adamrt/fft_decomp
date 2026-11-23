#ifndef EFFECT_PARTICLE_H
#define EFFECT_PARTICLE_H

/* BATTLE effect particles: emitter motion, the work-record and sprite-node pools, and their rendering. */

#include "fft/effect/effect.h"
#include "fft/effect/effect_emitter_data.h"
#include "fft/gfx/gfx.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* particle motion */
/* The particle updater reserves -1 for animation-driven lifetime. */
enum { EFFECT_PARTICLE_LIFETIME_ANIMATION_DRIVEN = -1 };

void effect_set_gravity(const vec3s32_t* src);
void effect_save_gravity(void);
void effect_restore_gravity(void);
void effect_set_inertia_threshold(s32 threshold);
void effect_save_inertia_threshold(void);
void effect_restore_inertia_threshold(void);
void effect_emitter_interpolate_spawn_position(const effect_geometry_record_t* source, s32 factor, VECTOR* out);
void effect_emitter_interpolate_spawn_spread(const effect_geometry_record_t* source, s32 factor, VECTOR* out);
void effect_emitter_interpolate_velocity_angles(const effect_geometry_record_t* source, s32 factor, VECTOR* out);
void effect_emitter_interpolate_velocity_spread(const effect_geometry_record_t* source, s32 factor, VECTOR* out);
void effect_particle_integrate_motion(effect_work_record_t* list);

/* parent is the particle a child emitter spawns from, read only for the
 * EFFECT_EMITTER_ORIGIN_PARENT_PARTICLE origin: the particle update passes
 * the particle, the EFFECT overlay's particle handlers a record built on
 * their stack that holds a position, and the timeline none. The emitter index
 * arrives as a full 32-bit value. The function is declared with an int result it
 * never sets and no caller reads; a void declaration changes the effect script interpreter's code. */
s32 effect_emitter_spawn_particles(s16 record_index, s32 frame, s32 emitter_index, const effect_work_record_t* parent);

/* particle pools */
void effect_particle_free_record(s16 record_index, effect_work_record_t* work);

/* rendering */
/* Ordering-table depth clamp shared by the EFFECT renderers: a depth below MIN
 * becomes MIN and one at or beyond LIMIT (the OT slot count) becomes MAX. */
enum {
    EFFECT_OT_DEPTH_MIN = 4,
    EFFECT_OT_DEPTH_MAX = 383,
    EFFECT_OT_DEPTH_LIMIT = 384,
    EFFECT_SPRITE_OT_DEPTH_MAX = 382 /* the farthest depth of a particle sprite */
};

/* Size of the per-effect arena used by alloc_primitive. */
enum { EFFECT_PRIMITIVE_BUFFER_SIZE = 32768 };

/* Initial Z coordinate of each particle sprite's four corner vectors. */
enum { EFFECT_CORNER_Z_DEFAULT = 512 };

/* Work-kind handlers, indexed by kind_flags >> 12. The int return type is
 * load-bearing even though the result is ignored: the value-returning call is
 * a second set of $2, so sched1 no longer boosts the final `$2 = 1` to the end
 * of its block, and the pc update is allocated $3 as in the target. */
extern s32 (*g_effect_particle_render_handlers[4])(effect_record_t* record, effect_work_record_t* work);
extern MATRIX g_effect_matrix;
extern u32* g_effect_otag;

/* The four corners of a particle sprite quad. Four-byte alignment preserves the original 32-bit copies into x/y. */
extern SVECTOR g_effect_corner_top_left __attribute__((aligned(4)));
extern SVECTOR g_effect_corner_top_right __attribute__((aligned(4)));
extern SVECTOR g_effect_corner_bottom_left __attribute__((aligned(4)));
extern SVECTOR g_effect_corner_bottom_right __attribute__((aligned(4)));
void effect_render_init_frame(void);
void effect_render_reset_state(void);

#endif
