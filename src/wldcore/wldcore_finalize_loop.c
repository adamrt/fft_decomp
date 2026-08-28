#include "fft/main_runtime.h"
#include "fft/main_sound.h"
#include "fft/script_variables.h"
#include "fft/wldcore.h"
#include "fft/world.h"
#include "psx/etc.h"
#include "psx/gpu.h"

/* Finalize one WLDCORE loop pass and publish the world status. */
void wldcore_finalize_loop(void) {
    DrawSync(0);
    VSync(0);
    SetDispMask(0);
    wldcore_sound_wait_for_queue_drain();
    if ((g_main_system_flags & 0x200000) == 0) {
        main_sound_unload_scenario_music_and_tunes();
    }
    world_script_set_variable(EVENT_SCRIPT_VAR_CURRENT_MAP, g_wldcore_next_map_id[0]);
}
