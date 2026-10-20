/* A disc read through the main stack that yields every frame until the load has finished. */

#include "fft/file.h"
#include "fft/thread.h"
#include "psx/types.h"

/* Read `size` bytes from sector `lba` to `destination` through the main
 * stack, yielding every frame until the request is accepted and then until
 * the load has finished. */
void file_load_sync(s32 lba, s32 size, void* destination) {
    while (1) {
        g_thread_call_target = (void (*)(void))file_bytes_read_request;
        if (thread_call_on_main_stack(lba, size, destination) == 0) {
            break;
        }
        thread_yield();
    }
    while (1) {
        g_thread_call_target = (void (*)(void))file_load_state_get;
        if (thread_call_on_main_stack() == 0) {
            break;
        }
        thread_yield();
    }
}
