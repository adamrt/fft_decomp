#include "fft/battle_event.h"
#include "fft/battle_gfx.h"
#include "fft/thread.h"
#include "psx/types.h"

/*
 * Loads one 0x7800-byte block of EVTCHR.BIN into a fresh allocation on the main
 * stack, publishing it through g_battle_event_pending_loaded_evtchr_slot / g_battle_event_loaded_evtchr_buffer before
 * releasing it. Parameter 2 selects the block; each occupies 15 sectors from 0x1d4c.
 */
void gfx_load_evtchr_thread(void) {
    s32 parameter;
    s32 block;
    evtchr_file_record_t* buffer;

    parameter = thread_get_current_parameter_1_variant();
    block = thread_get_current_parameter_2_variant();
    thread_set_current_task_id(THREAD_TASK_LOAD_EVTCHR);
    buffer = main_heap_alloc(0x7800);
    do {
        thread_yield();
        g_thread_call_target = (void (*)(void))main_file_call_build_header;
    } while (thread_call_on_main_stack((block * 15) + DISC_SECTOR_EVENT_EVTCHR_BIN, 0x7800, buffer) != 0);
    do {
        thread_yield();
        g_thread_call_target = (void (*)(void))main_file_is_still_loading;
    } while (thread_call_on_main_stack() != 0);
    g_script_pending_loaded_evtchr_slot = parameter;
    g_script_loaded_evtchr_buffer = buffer;
    thread_yield();
    main_heap_free(buffer);
    thread_exit_current();
}
