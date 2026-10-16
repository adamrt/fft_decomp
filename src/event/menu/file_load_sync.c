#include "fft/main.h"
#include "fft/thread.h"
#include "psx/types.h"

/* Read `size` bytes from sector `lba` to `destination` through the main
 * stack, yielding every frame until the request is accepted and then until
 * the load has finished. */
void file_load_sync(s32 lba, s32 size, void* destination) {
    while (1) {
        g_thread_call_target = (void (*)(void))main_file_call_build_header;
        if (thread_call_on_main_stack(lba, size, destination) == 0) {
            break;
        }
        thread_yield();
    }
    while (1) {
        g_thread_call_target = (void (*)(void))main_file_is_still_loading;
        if (thread_call_on_main_stack() == 0) {
            break;
        }
        thread_yield();
    }
}
