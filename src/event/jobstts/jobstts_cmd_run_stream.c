#include "fft/event_jobstts.h"
#include "fft/menu.h"
#include "psx/types.h"

/* Dispatch every command of a command stream through the handler table until
 * the end opcode, after resetting the ordering-table index, its lock and the
 * semi-transparency state; `flags` is the input the handlers see. Then
 * advances the transition frame, up to 10. */
void jobstts_cmd_run_stream(menu_instruction_t* instruction, s32 flags) {
    g_jobstts_gfx_otag_index_locked = 0;
    g_menu_gfx_otag_index = 0;
    g_jobstts_gfx_semitransparency = 0;
    g_menu_cmd_stream_input = flags;
    while (instruction->header.opcode != JOBSTTS_CMD_END) {
        instruction = g_menu_cmd_handlers[instruction->header.opcode](instruction);
    }
    if (g_jobstts_gfx_transition_frame < 10) {
        g_jobstts_gfx_transition_frame = g_jobstts_gfx_transition_frame + 1;
    }
}
