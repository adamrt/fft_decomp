#include "fft/main_heap.h"

s32 main_overlay_open_world_and_wldcore(s32 load_world) {
    main_file_load_data_from_disc(0x14849, 0xDC, g_main_heap_low_overlay_load_address, 0);
    if (load_world != 0) {
        main_file_load_data_from_disc(0x14925, 0x1E0, g_main_heap_world_overlay_load_address, 1);
    }
    return wldcore_entrypoint();
}
