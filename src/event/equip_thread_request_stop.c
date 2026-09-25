#include "fft/event_equip.h"

void equip_thread_request_stop(s32 id) {
    battle_thread_set_parameters(id, 0, 0, 1);
}
