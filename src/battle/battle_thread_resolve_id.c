#include "fft/battle.h"
#include "psx/types.h"

s32 battle_thread_resolve_id(s32 requested_thread_id) {
    s32 i;

    if (requested_thread_id < 0x10) {
        return requested_thread_id;
    }
    i = 1;
    do {
        if (battle_thread_is_running_8014cc94(i) == 0) {
            return i;
        }
        i++;
    } while (i < 0x10);
    /* The target returns the void callee's leftover $v0. */
    return ((s32 (*)(void))battle_thread_exit_current)();
}
