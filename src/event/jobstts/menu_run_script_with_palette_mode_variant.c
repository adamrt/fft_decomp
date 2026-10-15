#include "fft/event_jobstts.h"
#include "fft/menu.h"
#include "psx/types.h"

/* Select the palette set for `event_mode` and run the command stream. */
void menu_run_script_with_palette_mode_variant(menu_instruction_t* script, s32 flags, s32 event_mode) {
    menu_set_palette_mode(event_mode);
    menu_cmd_run_stream_variant(script, flags);
}
