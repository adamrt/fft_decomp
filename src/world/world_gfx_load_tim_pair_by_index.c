#include "fft/main_file.h"
#include "fft/world.h"

/* Load the 1-based `index`th TIM from each of the two tables into the record. */
void world_gfx_load_tim_pair_by_index(s32 index, world_gfx_tim_pair_record_t* record) {
    s32 slot = index - 1;

    record->tim_pointers[0] = main_file_get_bin_as_tim(
        g_world_gfx_tim_pair_primary_files[slot * 2], g_world_gfx_tim_pair_primary_files[(slot * 2) + 1]);
    record->tim_pointers[1] = main_file_get_bin_as_tim(
        g_world_gfx_tim_pair_secondary_files[slot * 2], g_world_gfx_tim_pair_secondary_files[(slot * 2) + 1]);
}
