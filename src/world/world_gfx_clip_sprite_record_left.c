#include "fft/world.h"
#include "psx/types.h"

/* Trims `delta` texels from the leading edge of both sprite halves: the
 * texture origin advances (u, or v when the half is not mirrored) and the
 * width shrinks to match. */
void world_gfx_clip_sprite_record_left(world_gfx_sprite_record_t* record, s32 delta) {
    if (record->halves[0].u0 != record->halves[0].u1) {
        record->halves[0].u0 += delta;
        record->halves[1].u0 += delta;
    } else {
        record->halves[0].v0 += delta;
        record->halves[1].v0 += delta;
    }
    record->halves[0].width -= delta;
    record->halves[1].width -= delta;
}
