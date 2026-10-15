#include "fft/world.h"
#include "psx/types.h"

/* Return the resident text resource; index 6 has no data. */
u8* world_text_get_resident_data(world_text_data_e index) {
    return g_world_text_resident_data[index];
}
