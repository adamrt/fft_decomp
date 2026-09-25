#include "fft/battle.h"

/* Begin one effect-rendering frame.
 *
 * The renderer alternates packet storage and remembers the arena offset so
 * the frame's transient primitives can be reclaimed together. */
void battle_effect_start_render_frame(void) {
    SetGeomOffset(0, 0);
    g_battle_effect_frame_parity = 1 - g_battle_effect_frame_parity;
    g_battle_effect_frame_start_prim_buffer_offset = g_battle_effect_prim_buffer_offset;
}
