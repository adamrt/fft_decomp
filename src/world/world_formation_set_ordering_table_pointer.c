#include "fft/world.h"
#include "psx/types.h"

void world_formation_set_ordering_table_pointer(void* ordering_table) {
    g_world_formation_ordering_table = ordering_table;
}
