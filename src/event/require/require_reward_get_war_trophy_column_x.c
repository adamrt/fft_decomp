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

/* Screen x of war-trophy entry `value`'s column, given the entry count
 * `index`: one column at 0xd0 for fewer than five entries, otherwise 0x9c
 * for even entries and 0x104 for odd ones. */
s32 require_reward_get_war_trophy_column_x(s32 value, s32 index) {
    s32 result = 0xd0;

    if (index >= 5) {
        s32 alternate = 0x9c;

        if (value & 1) {
            alternate = 0x104;
        }
        result = alternate;
    }
    return result;
}
