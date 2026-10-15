#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "psx/gte_inline.h"
#include "psx/types.h"

/*
 * Effect-script opcode 0x25: update every particle on the record's work list.
 *
 * Loads the camera matrix into the GTE and integrates the list's motion. Each
 * particle then ends its life once it is within a non-zero homing threshold
 * of its anchor on all three axes, runs its kind handler, spawns its mid-life
 * child and advances its curve frame. When its countdown (or, for lifetime
 * -1, its animation) ends it spawns its death child and is freed.
 *
 * The loop's locals are declared in its body: the block note ahead of the
 * exit test stops GCC from rolling the test to the loop end, which keeps the
 * target's top-tested loop. The death child is tested with an if/else chain
 * whose arms are identical (cross-jumping merges them): `mode == 1 || mode ==
 * 2` is folded into an unsigned range test that the target does not contain.
 */
effect_script_result_e effect_code_script_25_update_all_particles(effect_record_t* record) {
    effect_work_record_t* work;

    work = record->work_head;
    gte_SetRotMatrix(&g_camera_matrix);
    gte_SetTransMatrix(&g_camera_matrix);
    effect_integrate_particle_motion(record->work_head);

    for (;;) {
        effect_work_record_t* node;
        s32 threshold;
        s32 delta;

        if (work == 0) {
            break;
        }
        node = work;
        threshold
            = (node->flags & EFFECT_EMITTER_FLAG_ARRIVAL_DISTANCE_MASK) >> EFFECT_EMITTER_FLAG_ARRIVAL_DISTANCE_SHIFT;
        work = node->next;
        if (threshold != 0) {
            delta = (node->motion.position.x >> 12) - node->target.x;
            if (delta < threshold && -threshold < delta) {
                delta = (node->motion.position.y >> 12) - node->target.y;
                if (delta < threshold && -threshold < delta) {
                    delta = (node->motion.position.z >> 12) - node->target.z;
                    if (delta < threshold && -threshold < delta) {
                        node->lifetime = EFFECT_PARTICLE_LIFETIME_ANIMATION_DRIVEN;
                    }
                }
            }
        }

        g_effect_particle_render_handlers[(u16)node->kind_flags >> 12](record, node);

        if ((node->flags & EFFECT_EMITTER_FLAG_CHILD_MID_LIFE_MASK) == EFFECT_EMITTER_FLAG_CHILD_MID_LIFE_MODE_1
            || (node->flags & EFFECT_EMITTER_FLAG_CHILD_MID_LIFE_MASK) == EFFECT_EMITTER_FLAG_CHILD_MID_LIFE_MODE_2) {
            effect_spawn_emitter_particles(
                record->record_index, record->frame_counter, node->child_emitter_mid_life, node);
        }

        node->curve_frame++;
        if (node->curve_frame == EFFECT_ANIMATION_CURVE_FRAMES) {
            node->curve_frame = 0;
        }

        if (node->lifetime == EFFECT_PARTICLE_LIFETIME_ANIMATION_DRIVEN) {
            if (node->data->frame_timer != 0) {
                continue;
            }
        } else if (--node->lifetime != 0) {
            continue;
        }

        if ((node->flags & EFFECT_EMITTER_FLAG_CHILD_ON_DEATH_MASK) == EFFECT_EMITTER_FLAG_CHILD_ON_DEATH_MODE_1) {
            effect_spawn_emitter_particles(
                record->record_index, record->frame_counter, node->child_emitter_on_death, node);
        } else if ((node->flags & EFFECT_EMITTER_FLAG_CHILD_ON_DEATH_MASK)
            == EFFECT_EMITTER_FLAG_CHILD_ON_DEATH_MODE_2) {
            effect_spawn_emitter_particles(
                record->record_index, record->frame_counter, node->child_emitter_on_death, node);
        }
        effect_free_work_record(record->record_index, node);
    }

    record->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}
