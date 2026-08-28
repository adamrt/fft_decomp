#include "fft/world.h"
#include "psx/types.h"

/* Shifts both sprite halves right by `delta` and pulls the trailing texture
 * edge back (u1, or v1 when the half is not mirrored). */
void world_gfx_clip_sprite_record_right(world_gfx_sprite_record_t* record, s32 delta) {
    record->halves[0].x += delta;
    record->halves[1].x += delta;
    if (record->halves[0].u0 != record->halves[0].u1) {
        record->halves[0].u1 -= delta;
        record->halves[1].u1 -= delta;
    } else {
        record->halves[0].v1 -= delta;
        record->halves[1].v1 -= delta;
    }
}
