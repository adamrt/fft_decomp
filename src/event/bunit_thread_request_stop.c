#include "psx/types.h"

void bunit_thread_request_stop(s32 thread_id) {
    battle_thread_set_parameters(thread_id, 0, 0, 1);
}
