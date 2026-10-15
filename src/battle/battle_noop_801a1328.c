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

/* Calls the empty main_noop_800449f8 with (1, 0x320) from a 0x100-byte frame it
 * does not use. No code calls it. */
void battle_noop_801a1328(void) {
    s32 unused[0x40];
    main_system_report_stub(1, 0x320);
}
