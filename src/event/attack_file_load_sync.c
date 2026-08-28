#include "fft/main_runtime.h"
#include "psx/types.h"

void attack_file_load_sync(s32 lba, s32 size, void* destination) {
    while (1) {
        g_battle_thread_call_target = (void (*)(void))main_file_call_build_header;
        if (battle_thread_call_on_main_stack(lba, size, destination) == 0) {
            break;
        }
        battle_thread_yield();
    }
    while (1) {
        g_battle_thread_call_target = (void (*)(void))main_file_is_still_loading;
        if (battle_thread_call_on_main_stack() == 0) {
            break;
        }
        battle_thread_yield();
    }
}
