#include "fft/battle.h"

s32 battle_effect_update_cycle(void) {
    s32 stage_result;
    s32 related_result;

    stage_result = battle_effect_update_stage();
    related_result = battle_effect_update_secondary_effects();
    battle_camera_update_effect_rotation_and_zoom();

    if (stage_result != 0 || related_result != 0) {
        return 1;
    }
    return 0;
}
