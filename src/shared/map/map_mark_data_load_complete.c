#include "fft/battle_map.h"

/* Mark the asynchronous map-data sequence ready for its next init stage. */
void map_mark_data_load_complete(void) {
    g_map_data_load_complete = 1;
}
