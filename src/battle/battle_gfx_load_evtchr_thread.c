#include "fft/battle.h"
#include "fft/main_heap.h"
#include "fft/main_runtime.h"
#include "psx/types.h"

/*
 * Loads one 0x7800-byte EVTCHR bank into a fresh allocation on the main
 * stack, publishing it through g_battle_event_pending_loaded_evtchr_slot / g_battle_event_loaded_evtchr_buffer before
 * releasing it. Parameter 2 selects the bank; each occupies 15 sectors from 0x1d4c.
 */
void battle_gfx_load_evtchr_thread(void) {
    s32 parameter;
    s32 bank;
    void* buffer;

    parameter = battle_thread_get_current_parameter_1();
    bank = battle_thread_get_current_parameter_2();
    battle_thread_set_current_task_id(NATIVE_THREAD_TASK_LOAD_EVTCHR);
    buffer = game_malloc(0x7800);
    do {
        battle_thread_yield();
        g_battle_thread_call_target = (void (*)(void))main_file_call_build_header;
    } while (battle_thread_call_on_main_stack((bank * 15) + 0x1d4c, 0x7800, buffer) != 0);
    do {
        battle_thread_yield();
        g_battle_thread_call_target = (void (*)(void))main_file_is_still_loading;
    } while (battle_thread_call_on_main_stack() != 0);
    g_battle_event_pending_loaded_evtchr_slot = parameter;
    g_battle_event_loaded_evtchr_buffer = buffer;
    battle_thread_yield();
    main_heap_free(buffer);
    battle_thread_exit_current();
}
