#include "fft/equip.h"
#include "psx/types.h"

void equip_thread_stop_and_clear_state(s32 id) {
    equip_thread_request_stop(id);
    g_equip_thread_state = 0;
}
