#include "fft/battle_gfx.h"
#include "psx/gte.h"

void battle_gfx_init_render_frame(void) {
    SetGeomOffset(0, 0);
    /* The target loads the counter unsigned (lhu). */
    g_battle_gfx_previous_counter = *(u16*)&g_battle_gfx_counter;
}
