#include "fft/menu.h"
#include "fft/menu_script.h"
#include "psx/types.h"

/* Command 0x16: step the portrait transition, then skip the one-byte command. */
menu_instruction_t* menu_cmd_advance_window_scale_step_handler(menu_instruction_t* data) {
    if (g_gfx_transition_frame < 4) {
        g_gfx_transition_frame++;
    }
    return (menu_instruction_t*)((u8*)data + 1);
}
