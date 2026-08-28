#include "fft/battle.h"
#include "psx/types.h"

/* Mark the asynchronous map-data sequence ready for its next init stage. */
void battle_map_mark_data_load_complete(void) {
    g_battle_map_data_load_complete = 1;
}
