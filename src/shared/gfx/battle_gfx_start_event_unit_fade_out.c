#include "fft/battle_gfx.h"
#include "psx/types.h"

/* Calls battle_gfx_start_unit_fade_out with the same
 * arguments; the event instructions that remove a unit start its fade-out here. */
void battle_gfx_start_event_unit_fade_out(s32 runtime_id, s32 mode, s16 red, s16 green, s16 blue) {
    battle_gfx_start_unit_fade_out(runtime_id, mode, red, green, blue);
}
