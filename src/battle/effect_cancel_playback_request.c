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

/* Drops a requested effect that has not been released. */
void effect_cancel_playback_request(void) {
    if (g_effect_playback_phase == EFFECT_PLAYBACK_PHASE_REQUESTED) {
        g_effect_playback_phase = EFFECT_PLAYBACK_PHASE_IDLE;
    }
}
