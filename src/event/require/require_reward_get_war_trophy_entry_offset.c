#include "fft/battle_gfx.h"
#include "fft/battle_menu.h"
#include "fft/battle_script.h"
#include "fft/event_helpmenu.h"
#include "fft/event_require.h"
#include "fft/gfx.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Screen y of war-trophy row `value`, given the entry count `index`: 20
 * pixels per entry in the single column, per pair of entries in two
 * columns, from y 0x78. */
s32 require_reward_get_war_trophy_entry_offset(s32 value, s32 index) {
    if (index < 5) {
        return value * 20 + 0x78;
    }
    return (value / 2) * 20 + 0x78;
}
