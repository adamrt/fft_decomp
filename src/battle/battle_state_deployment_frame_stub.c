#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_camera.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_script.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "fft/script.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Empty. battle_state_run_deployment calls it each frame while the map is still being set up. */
void battle_state_deployment_frame_stub(void) {
}
