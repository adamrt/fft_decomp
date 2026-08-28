#include "fft/main_runtime.h"
#include "fft/wldcore.h"

s32 wldcore_script_calculate_save_data_completion_percent(void) {
    s32 count = 0;
    s32 i = 0;

    do {
        if (wldcore_test_bit(g_main_saved_data_bits, i) != 0) {
            count++;
        }
        i++;
    } while (i < 0x1000);
    return count * 100 / 76;
}
