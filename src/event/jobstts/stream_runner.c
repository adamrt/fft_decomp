/* Command stream dispatch and the palette-mode script run. */

#include "fft/event_jobstts.h"
#include "fft/gfx_primitive.h"
#include "fft/menu_gfx.h"
#include "fft/menu_script.h"
#include "fft/menu_window.h"
#include "psx/types.h"

/* Dispatch every command of a command stream through the handler table until
 * the end opcode, after resetting the ordering-table index, its lock and the
 * semi-transparency state; `flags` is the input the handlers see. Then
 * advances the transition frame, up to 10. */
void menu_script_run(menu_instruction_t* instruction, s32 flags) {
    g_gfx_otag_index_locked = 0;
    g_menu_gfx_otag_index = 0;
    g_menu_gfx_semitransparency = 0;
    g_menu_script_input = flags;
    while (instruction->header.opcode != STATUS_SCREEN_SCRIPT_END) {
        instruction = g_menu_opcode_handlers[instruction->header.opcode](instruction);
    }
    if (g_gfx_transition_frame < 10) {
        g_gfx_transition_frame = g_gfx_transition_frame + 1;
    }
}

/* Select the palette set for `event_mode` and run the command stream. */
void menu_run_script_with_palette_mode(menu_instruction_t* script, s32 flags, s32 event_mode) {
    menu_set_palette_mode(event_mode);
    menu_script_run(script, flags);
}
