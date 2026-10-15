#include "fft/battle_map.h"
#include "psx/types.h"

/* Advance the current map's load by one step. Nonzero while it is still loading. */
s32 map_poll_data_load(void) {
    return map_load_data(g_map_id, MAP_LOAD_COMMAND_POLL);
}
