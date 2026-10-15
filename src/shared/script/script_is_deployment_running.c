#include "fft/battle.h"
#include "fft/battle_map.h"
#include "fft/battle_script.h"
#include "fft/map.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "psx/types.h"

/* Run one event frame, then return whether the deployment thread (borrowed slot
 * 6) is still running. */
s32 script_is_deployment_running(u32* ot, u32 buttons) {
    script_run_event_frame_variant(ot, buttons);
    return thread_is_running(THREAD_BORROWED_SLOT_DEPLOYMENT);
}
