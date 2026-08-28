#include "fft/bunit.h"
#include "psx/types.h"

/* BUNIT.OUT 001c85f8 - Invoke menu action `menu_id`+`state`, but clear `state` to
 * 0 whenever the override flag is set (also passed to bunit_text_set_palette_and_metrics). */
void bunit_menu_dispatch_with_override(s32 menu_id, s32 state, s32 override) {
    s32 effective_state = state;
    if (override != 0) {
        effective_state = 0;
    }
    bunit_text_set_palette_and_metrics(override);
    bunit_cmd_run_stream((u8*)menu_id, effective_state);
}
