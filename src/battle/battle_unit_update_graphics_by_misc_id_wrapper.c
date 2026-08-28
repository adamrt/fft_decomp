#include "fft/battle.h"
#include "psx/types.h"

void battle_unit_update_graphics_by_misc_id_wrapper(u32 misc_id) {
    /* The target passes misc_id without the u16 zero-extension the prototype adds. */
    ((void (*)(u32))battle_unit_update_display_by_misc_id)(misc_id);
}
