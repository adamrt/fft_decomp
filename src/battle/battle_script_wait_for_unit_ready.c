#include "fft/battle.h"
#include "psx/types.h"

void battle_script_wait_for_unit_ready(s32 misc_id) {
    /* The target passes misc_id to the argument-less callee. */
    while (((s32 (*)(s32))battle_return_one)(misc_id) == 0) {
        battle_thread_yield();
    }
}
