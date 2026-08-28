#include "fft/main_file.h"
#include "fft/world.h"
#include "psx/types.h"

void world_gfx_load_tim_by_table_index(s32 index) {
    main_file_get_bin_as_tim(g_world_gfx_tim_file_table[index * 2], g_world_gfx_tim_file_table[(index * 2) + 1]);
}
