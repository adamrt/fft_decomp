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

/* Look up a native overlay callback by its resident catalog index. */
effect_callback_t effect_get_native_callback(s16 callback_id) {
    return g_effect_native_callbacks[callback_id];
}
