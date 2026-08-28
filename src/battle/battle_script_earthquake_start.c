#include "fft/battle.h"
#include "fft/thread.h"
#include "psx/types.h"

/*
 * Earthquake thread task (task id 0xd): two independent frame counters step
 * the shared shake accumulator at 0x80165ff0.  Parameters: 0x00 first
 * increment, 0x01 its period in yielded frames, 0x02 second increment, 0x03
 * its period.  The loop never terminates; the task is torn down externally,
 * so the emitted epilogue is unreachable.
 */
void battle_script_earthquake_start(void) {
    u8* parameters;
    s32 first_counter;
    s32 second_counter;

    battle_thread_set_current_task_id(NATIVE_THREAD_TASK_EARTHQUAKE);
    parameters = (u8*)battle_thread_get_current_parameter_1();
    first_counter = 0;
    second_counter = 0;
    for (;;) {
        battle_thread_yield();
        first_counter++;
        if (first_counter == parameters[1]) {
            g_battle_camera_shake_z_offset += parameters[0];
            first_counter = 0;
        }
        second_counter++;
        if (second_counter == parameters[3]) {
            g_battle_camera_shake_z_offset += parameters[2];
            second_counter = 0;
        }
    }
}
