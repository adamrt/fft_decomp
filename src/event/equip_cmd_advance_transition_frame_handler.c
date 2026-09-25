#include "fft/event_equip.h"
#include "psx/types.h"

u8* equip_cmd_advance_transition_frame_handler(u8* data) {
    s32 current;
    s16 previous;
    s32 unused[2];

    current = g_equip_gfx_transition_frame;
    previous = current;
    if (current < 4) {
        g_equip_gfx_transition_frame = previous + 1;
    }
    return data + 1;
}
