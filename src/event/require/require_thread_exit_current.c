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

void require_thread_exit_current(void) {
    thread_exit_current();
}
