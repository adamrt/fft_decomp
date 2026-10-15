#include "fft/thread.h"
#include "fft/world.h"
#include "psx/libc.h"
#include "psx/pad.h"
#include "psx/types.h"

void world_formation_ability_menu_idle_thread(void) {
    for (;;) {
        thread_wait_frames(1);
    }
}
