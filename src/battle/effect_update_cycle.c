#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "fft/battle_map.h"
#include "fft/battle_menu.h"
#include "fft/data.h"
#include "fft/main.h"
#include "psx/libgpu.h"
#include "psx/suzuki.h"
#include "psx/types.h"

/* The effect system's frame: the effect file, the secondary effects and the
 * effect camera moves. Returns 1 while an effect of either kind runs. */
s32 effect_update_cycle(void) {
    s32 stage_result;
    s32 related_result;

    stage_result = effect_update_stage();
    related_result = effect_update_secondary_effects();
    camera_update_effect_rotation_and_zoom();

    if (stage_result != 0 || related_result != 0) {
        return 1;
    }
    return 0;
}
