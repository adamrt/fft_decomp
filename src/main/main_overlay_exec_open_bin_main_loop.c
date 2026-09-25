#include "fft/main.h"

int main_overlay_exec_open_bin_main_loop(int mode) {
    main_file_load_checked_to_address(0x14ff0, 0x36800, g_main_heap_low_overlay_load_address);
    return open_system_run_main_loop(mode);
}
