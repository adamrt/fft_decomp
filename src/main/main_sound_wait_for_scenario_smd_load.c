#include "fft/main_file.h"
#include "fft/main_runtime.h"
#include "psx/types.h"

s32 main_sound_wait_for_scenario_smd_load(s32 scenario) {
    s32 result;

    result = main_sound_open_scenario_smd_files(scenario);
    if (result != 0) {
        while (main_sound_poll_scenario_smd_load() != 0) {
            VSync(0);
            main_noop_800449ec();
            main_file_poll_load(&g_main_file_cd_state);
        }
    }
    return result;
}
