#include "fft/event_require.h"
#include "psx/types.h"

void require_file_load_sync(s32 lba, s32 size, void* destination) {
    do {
        g_battle_thread_call_target = (void (*)(void))main_file_call_build_header;
        if (battle_thread_call_on_main_stack(lba, size, destination) == 0) {
            break;
        }
        battle_thread_yield();
    } while (1);

    do {
        g_battle_thread_call_target = (void (*)(void))main_file_is_still_loading;
        if (battle_thread_call_on_main_stack() == 0) {
            break;
        }
        battle_thread_yield();
    } while (1);
}
