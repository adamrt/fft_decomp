#include "psx/gte.h"
#include "psx/types.h"

/* Scales the misc unit's partial X/Z/Y step (words 10-12, offsets 0x28-0x30)
 * by a walk speed in ONE-based fixed point. */
void battle_move_interpolate_partial(s32* velocity_words, s32 scale) {
    velocity_words[10] = (scale * velocity_words[10]) / ONE;
    velocity_words[11] = (scale * velocity_words[11]) / ONE;
    velocity_words[12] = (scale * velocity_words[12]) / ONE;
}
