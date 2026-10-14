#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Copy a work record's position, heading, and color onto its list node.
 *
 * With kind_flags bit 1 the velocity is rotated into camera space and its
 * screen heading stored on the node. Color-curve records take each channel
 * from its animation curve at the record's curve frame; others use neutral
 * 0x80. */
void effect_update_node_from_work(s16 record_index, effect_work_record_t* work) {
    SVECTOR direction;
    VECTOR rotated;

    work->data->x = work->motion.position.x >> 12;
    work->data->y = work->motion.position.y >> 12;
    work->data->z = work->motion.position.z >> 12;
    if (work->kind_flags & EFFECT_EMITTER_ORIENT_TO_VELOCITY) {
        SetRotMatrix(&g_camera_matrix);
        SetTransMatrix(&g_effect_particle_heading_matrix);
        setVector(&direction, work->motion.velocity.x >> 3, work->motion.velocity.y >> 3, work->motion.velocity.z >> 3);
        /* The 32-bit flag value lands in the output vector's 32-bit pad field; the target frame
         * has no separate flag slot. */
        RotTrans(&direction, &rotated, &rotated.pad);
        work->data->screen_rotation_angle = ratan2(rotated.vy, rotated.vx);
    }
    if (work->flags & EFFECT_EMITTER_FLAG_COLOR_CURVES) {
        effect_render_particle_sprite(work->data,
            g_effect_animation_curve_data->curves[work->color_curves.r][work->curve_frame],
            g_effect_animation_curve_data->curves[work->color_curves.g][work->curve_frame],
            g_effect_animation_curve_data->curves[work->color_curves.b][work->curve_frame]);
    } else {
        effect_render_particle_sprite(work->data, 0x80, 0x80, 0x80);
    }
}
