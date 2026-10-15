#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_effect.h"
#include "fft/battle_map.h"
#include "fft/battle_status.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/* Clear the range and the mark of all 512 targeting panels. */
void target_clear_panel_data(void) {
    s32 i = 0;
    target_panel_t* panel = g_target_panel_data;
    for (; i < 512; i++) {
        panel->remaining_range = 0;
        panel->mark = 0;
        panel++;
    }
}
