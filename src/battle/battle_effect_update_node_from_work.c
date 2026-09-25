#include "fft/battle.h"
#include "psx/types.h"

/* Copy a work record's position, heading, and colour onto its list node.
 *
 * With kind_flags bit 1 the velocity is rotated into camera space and its
 * screen heading stored on the node. Colour-curve records take each channel
 * from its g_effect_palette_table row at the current palette frame; others
 * use neutral 0x80. */
void battle_effect_update_node_from_work(s16 record_index, effect_work_record_t* work) {
    SVECTOR direction;
    VECTOR rotated;

    ((effect_list_node_t*)work->data)->x = work->position[0] >> 12;
    ((effect_list_node_t*)work->data)->y = work->position[1] >> 12;
    ((effect_list_node_t*)work->data)->z = work->position[2] >> 12;
    if (work->kind_flags & EFFECT_WORK_KIND_FLAG_ORIENT_TO_VELOCITY) {
        SetRotMatrix(&g_battle_camera_matrix);
        SetTransMatrix(&g_battle_effect_particle_heading_matrix);
        direction.vx = work->velocity[0] >> 3;
        direction.vy = work->velocity[1] >> 3;
        direction.vz = work->velocity[2] >> 3;
        /* The flag word lands in the output vector's pad word; the target frame
         * has no separate flag slot. */
        RotTrans(&direction, &rotated, (long*)&rotated.pad);
        ((effect_list_node_t*)work->data)->screen_rotation_angle = ratan2(rotated.vy, rotated.vx);
    }
    if (work->flags & EFFECT_WORK_FLAG_COLOUR_CURVES) {
        battle_effect_render_particle_sprite(work->data,
            g_effect_palette_table[work->colour_curves[0]].factor[work->palette_frame],
            g_effect_palette_table[work->colour_curves[1]].factor[work->palette_frame],
            g_effect_palette_table[work->colour_curves[2]].factor[work->palette_frame]);
    } else {
        battle_effect_render_particle_sprite(work->data, 0x80, 0x80, 0x80);
    }
}
