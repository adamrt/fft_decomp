#include "fft/menu.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/*
 * Advance daily proposition progress and packed countdown bytes.
 *
 * Records without flag 0x04 increment their elapsed byte. Set flag 0x04 when
 * it equals the assigned duration or flag 0x02 is set. Nonzero low-six-bit
 * countdowns decrease from at most 32, retaining their high 2 bits.
 */
void wldcore_proposition_advance_daily_counters(void) {
    s32 i;
    u8* counter;

    for (i = 0; i < g_main_save_proposition_count; i++) {
        if (!(g_main_active_propositions[i].flags & WLDCORE_PROPOSITION_PROGRESS_RETURNED)) {
            g_main_active_propositions[i].elapsed_days++;
            if (g_main_active_propositions[i].assigned_days == g_main_active_propositions[i].elapsed_days
                || (g_main_active_propositions[i].flags & WLDCORE_PROPOSITION_PROGRESS_RECALLED)) {
                g_main_active_propositions[i].flags |= WLDCORE_PROPOSITION_PROGRESS_RETURNED;
            }
        }
    }
    counter = g_main_proposition_states;
    do {
        u32 value = *counter;
        s32 days = value & WLDCORE_PROPOSITION_STATE_DAYS_MASK;
        if (days != 0) {
            if (days > 32) {
                days = 32;
            }
            days--;
            *counter = (value & (WLDCORE_PROPOSITION_STATE_FAILURE | WLDCORE_PROPOSITION_STATE_BAD_FAILURE)) | days;
        }
        counter++;
    } while ((s32)counter < (s32)(g_main_proposition_states + 96)); /* Target uses signed SLT. */
}
