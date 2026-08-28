#include "fft/battle.h"
#include "psx/types.h"

s32 battle_script_is_deployment_running(u32* ot, u32 buttons) {
    battle_script_run_event_frame(ot, buttons);
    return battle_thread_is_running(6);
}
