#include "fft/event_bunit.h"
#include "psx/types.h"

void bunit_gfx_init_scaled_draw_area_packets(u8* primitive) {
    *(s16*)(primitive + 0x1c) = 0;
    *(s16*)(primitive + 0x26) = 0;
    battle_copy_bytes(primitive + 0x20, &g_bunit_gfx_draw_area_template, 8);
}
