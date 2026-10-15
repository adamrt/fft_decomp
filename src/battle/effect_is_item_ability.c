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

/* 1 when the ability's entry in g_effect_ability_files carries
 * EFFECT_ABILITY_FLAG_ITEM. A negative entry (an ability without an effect
 * file) is complemented before the test. */
s32 effect_is_item_ability(s32 ability_id) {
    s32 entry;
    entry = g_effect_ability_files[ability_id];
    if (entry < 0) {
        entry = ~entry;
    }
    return (entry & EFFECT_ABILITY_FLAG_ITEM) > 0;
}
