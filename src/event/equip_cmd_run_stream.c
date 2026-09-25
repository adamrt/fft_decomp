#include "fft/event_equip.h"
#include "psx/types.h"

/* Dispatch every command of a sprite command stream through the opcode
   handler table until the 0x19 end-of-stream opcode. */
void equip_cmd_run_stream(u8* stream, s32 input) {
    g_equip_gfx_sprite_ot_index = 0;
    g_equip_gfx_semitransparency = 0;
    g_equip_cmd_stream_input = input;
    g_equip_gfx_zoom_draw_area_active = 0;
    if (stream[0] != 0x19) {
        do {
            /* The target calls through without arguments. */
            stream = ((u8 * (*)(void)) g_equip_cmd_handlers[stream[0]])();
        } while (stream[0] != 0x19);
    }
}
