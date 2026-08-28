#include "psx/types.h"

extern s32 battle_return_one(void);

void world_script_wait_for_unit_ready(s32 misc_id) {
    /* The target passes misc_id to this argument-less callee. */
    while (((s32 (*)(s32))battle_return_one)(misc_id) == 0) {
        world_thread_yield();
    }
}
