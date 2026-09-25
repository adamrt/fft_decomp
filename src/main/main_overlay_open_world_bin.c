#include "fft/main.h"

void main_overlay_open_world_bin(s32 mode) {
    main_file_load_data_from_disc(0x14925, 0x1E0, g_main_heap_world_overlay_load_address, mode - 1);
}
