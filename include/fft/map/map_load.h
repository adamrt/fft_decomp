#ifndef MAP_LOAD_H
#define MAP_LOAD_H

/* Battle map loading: the commands of the loader that reads a map's GNS file and the resource files it selects. */

#include "psx/types.h"

/* loader */
/* Commands of map_update_load, read only while the GNS phase is idle. */
typedef enum map_load_command {
    MAP_LOAD_COMMAND_LOAD_GNS = 0x73, /* always read the GNS; no current caller issues this */
    MAP_LOAD_COMMAND_START = 0x75,
    MAP_LOAD_COMMAND_POLL = 0x76,       /* an unhandled command only advances the load */
    MAP_LOAD_COMMAND_START_HELD = 0x99, /* as START, but hold dispatch until MAP_DATA_COMMAND_RELEASE_GNS_HOLD */
    MAP_LOAD_COMMAND_POLL_HELD = 0x9c
} map_load_command_e;

/* Other returns are the GNS phase: 0 idle, 1 loading resources, 15 starting. */
enum { MAP_LOAD_RESULT_HELD = 0x9b };

s32 map_update_load(s32 map_id, map_load_command_e command);
s32 map_start_load(void);
s32 map_poll_load(void);
s32 map_mesh_load_palette(s32 palette_mode);
s32 map_update_init_sequence(s32 map_id, s32 step);
s32 map_update_deployment_init_sequence(s32 map_id, s32 step, s32 deployment_running);

#endif
