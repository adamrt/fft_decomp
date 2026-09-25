#include "fft/main.h"

void main_overlay_exec_open_bin_ending(void) {
    main_file_load_checked_to_address(0x14ff0, 0x36800, g_main_heap_low_overlay_load_address);
    open_system_run_ending_cutscene();
}
