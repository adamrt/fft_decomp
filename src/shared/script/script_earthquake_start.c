#include "fft/battle_camera.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "psx/types.h"

/* Two independent frame counters apply the two shake pulses. The task loops
 * until another instruction tears it down. */
void script_earthquake_start(void) {
    event_earthquake_parameters_t* parameters;
    s32 first_counter;
    s32 second_counter;

    thread_set_current_task_id(THREAD_TASK_EARTHQUAKE);
    parameters = (event_earthquake_parameters_t*)battle_thread_get_current_parameter_1();
    first_counter = 0;
    second_counter = 0;
    for (;;) {
        thread_yield();
        first_counter++;
        if (first_counter == parameters->pulses[0].frames) {
            g_camera_shake_z_offset += parameters->pulses[0].z_delta;
            first_counter = 0;
        }
        second_counter++;
        if (second_counter == parameters->pulses[1].frames) {
            g_camera_shake_z_offset += parameters->pulses[1].z_delta;
            second_counter = 0;
        }
    }
}
