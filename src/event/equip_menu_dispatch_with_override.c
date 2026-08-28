#include "fft/equip.h"
#include "psx/types.h"

/* Run the menu command stream `menu` with `state`, forcing state 0 whenever `override` is set. */
void equip_menu_dispatch_with_override(s32 menu, s32 state, s32 override) {
    s32 effective_state;

    effective_state = state;
    if (override != 0) {
        effective_state = 0;
    }
    equip_text_set_palette_and_metrics(override);
    equip_cmd_run_stream((u8*)menu, effective_state);
}
