#include "fft/battle.h"
#include "psx/types.h"

void battle_thread_set_parameters_4(s32 slot, s32 first, s32 second, s32 third, s32 fourth) {
    native_thread_t* thread = &g_battle_threads[slot];
    thread->function_parameter_1 = first;
    thread->function_parameter_2 = second;
    thread->function_parameter_3 = third;
    thread->function_parameter_4 = fourth;
}
