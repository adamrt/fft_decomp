/* The menu command that advances the transition frame. */

#include "fft/menu_script.h"
#include "fft/menu_window.h"
#include "psx/types.h"

/* Command 0x16: advance the window zoom transition frame (up to 4, the open window), then skip
 * the one-byte command. */
menu_instruction_t* menu_opcode_16_advance_window_scale_step(menu_instruction_t* data) {
    if (g_gfx_transition_frame < 4) {
        g_gfx_transition_frame++;
    }
    return (menu_instruction_t*)((u8*)data + 1);
}
