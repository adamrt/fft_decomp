#include "fft/world.h"
#include "psx/types.h"

s32 world_menu_get_thread_activity(void) {
    s32 i;

    if (world_thread_is_running_80100164(3) != 0) {
        return 2;
    }
    for (i = 4; i < 9; i++) {
        if (world_thread_is_running_80100164(i) != 0) {
            break;
        }
    }
    return (i != 9) << 2;
}
