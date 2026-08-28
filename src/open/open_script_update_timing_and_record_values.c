#include "fft/open.h"

void open_script_update_timing_and_record_values(void) {
    s32 step;
    s32 work;
    s32 counter;
    s32 index;
    s32 value;
    open_script_record_t* record;
    s32* y_pointer;

    step = g_open_script_state.dispatch.timing_step;
    if (step == 0) {
        return;
    }

    if (step > 0) {
        counter = g_open_script_timing_fraction;
        work = step + counter;
        step = work & 0xff;
        counter = g_open_script_external_counter;
        work >>= 8;
        g_open_script_timing_fraction = step;
        counter += work;
    } else {
        counter = g_open_script_timing_fraction;
        work = counter - step;
        step = work & 0xff;
        counter = g_open_script_external_counter;
        work >>= 8;
        g_open_script_timing_fraction = step;
        counter += work;
        work = -work;
    }
    g_open_script_external_counter = counter;

    index = 0;
    record = g_open_script_state.records;
    y_pointer = &record->y;
    do {
        if (record->flags & 1) {
            value = *y_pointer;
            value += work;
            *y_pointer = value;
            if ((u32)(value - 0x10) >= 0xf1) {
                record->flags ^= 1;
            }
        }
        /* Keeps reorg from stealing `index++` into the flags-test delay slot. */
        __asm__ volatile("");
        record++;
        index++;
        y_pointer = (s32*)((u8*)y_pointer + sizeof(*record));
    } while (index < 16);
}
