#include "fft/effect.h"

/* Stores scratchpad xyz triple group * 17 + point plus offset into output.
 *
 * Only the low halfword of each offset component is read (lhu), hence the
 * effect_wide_vector_t view. */
void effect_add_scratchpad_xyz_components(s32 group, s32 point, const VECTOR* offset, SVECTOR* output) {
    const effect_wide_vector_t* wide = (const effect_wide_vector_t*)offset;
    int point_offset = point * 3;
    int group_offset = group * 51;
    int element = group_offset + point_offset;
    u16* scratchpad = (u16*)0x1f800000;

    output->vx = scratchpad[element] + wide->x;
    output->vy = scratchpad[element + 1] + wide->y;
    output->vz = scratchpad[element + 2] + wide->z;
}
