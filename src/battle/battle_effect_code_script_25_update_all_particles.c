#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "fft/effect.h"
#include "psx/gte_inline.h"
#include "psx/types.h"

/* Defined in battle_effect_integrate_particle_motion.c. */
typedef struct battle_effect_particle battle_effect_particle_t;

extern void battle_effect_integrate_particle_motion(battle_effect_particle_t* list);

/*
 * Effect-script opcode 0x25: update every particle on the record's work list.
 *
 * Loads the camera matrix into the GTE and integrates the list's motion. Each
 * particle then ends its life once it is within a non-zero homing threshold
 * of its anchor on all three axes, runs its kind handler, spawns its mid-life
 * child and advances its palette column. When its countdown (or, for lifetime
 * -1, its animation) ends it spawns its death child and is freed.
 *
 * The loop's locals are declared in its body: the block note ahead of the
 * exit test stops GCC from rolling the test to the loop end, which keeps the
 * target's top-tested loop. The death child is tested with an if/else chain
 * whose arms are identical (cross-jumping merges them): `mode == 1 || mode ==
 * 2` is folded into an unsigned range test that the target does not contain.
 */
s32 battle_effect_code_script_25_update_all_particles(effect_record_t* record) {
    effect_work_record_t* work;

    work = record->work_head;
    gte_SetRotMatrix(&g_battle_camera_matrix);
    gte_SetTransMatrix(&g_battle_camera_matrix);
    battle_effect_integrate_particle_motion((battle_effect_particle_t*)record->work_head);

    for (;;) {
        effect_work_record_t* node;
        s32 threshold;
        s32 delta;

        if (work == 0) {
            break;
        }
        node = work;
        threshold
            = (node->flags & EFFECT_WORK_BEHAVIOR_HOMING_THRESHOLD_MASK) >> EFFECT_WORK_BEHAVIOR_HOMING_THRESHOLD_SHIFT;
        work = node->next;
        if (threshold != 0) {
            delta = (node->position[0] >> 12) - node->anchor_x;
            if (delta < threshold && -threshold < delta) {
                delta = (node->position[1] >> 12) - node->anchor_y;
                if (delta < threshold && -threshold < delta) {
                    delta = (node->position[2] >> 12) - node->anchor_z;
                    if (delta < threshold && -threshold < delta) {
                        node->lifetime = EFFECT_PARTICLE_LIFETIME_ANIMATION_DRIVEN;
                    }
                }
            }
        }

        g_battle_effect_particle_render_handlers[node->kind_flags >> 12](record, node);

        if ((node->flags & EFFECT_WORK_BEHAVIOR_MID_LIFE_CHILD_MASK) == EFFECT_WORK_BEHAVIOR_MID_LIFE_CHILD_MODE_1
            || (node->flags & EFFECT_WORK_BEHAVIOR_MID_LIFE_CHILD_MASK) == EFFECT_WORK_BEHAVIOR_MID_LIFE_CHILD_MODE_2) {
            battle_effect_spawn_emitter_particles(
                record->record_index, record->emitter_index, node->child_emitter_index_alt, node);
        }

        node->palette_frame++;
        if (node->palette_frame == EFFECT_ANIMATION_CURVE_FRAME_BYTES) {
            node->palette_frame = 0;
        }

        if (node->lifetime == EFFECT_PARTICLE_LIFETIME_ANIMATION_DRIVEN) {
            if (node->data->frame_timer != 0) {
                continue;
            }
        } else if (--node->lifetime != 0) {
            continue;
        }

        if ((node->flags & EFFECT_WORK_BEHAVIOR_DEATH_CHILD_MASK) == EFFECT_WORK_BEHAVIOR_DEATH_CHILD_MODE_1) {
            battle_effect_spawn_emitter_particles(
                record->record_index, record->emitter_index, node->child_emitter_index, node);
        } else if ((node->flags & EFFECT_WORK_BEHAVIOR_DEATH_CHILD_MASK) == EFFECT_WORK_BEHAVIOR_DEATH_CHILD_MODE_2) {
            battle_effect_spawn_emitter_particles(
                record->record_index, record->emitter_index, node->child_emitter_index, node);
        }
        battle_effect_free_particle(record->record_index, node);
    }

    record->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}
