#include "fft/battle_camera.h"
#include "fft/battle_event.h"
#include "fft/battle_map.h"
#include "fft/battle_script.h"
#include "fft/main.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Yields the thread at least once, then until the camera stops moving. */
void camera_wait_until_idle(void) {
    do {
        thread_yield();
    } while (camera_is_active() != 0);
}
