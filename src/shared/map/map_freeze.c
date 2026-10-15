#include "fft/battle_map.h"

/* Pause every map texture animation (their modes are saved and cleared) until
 * map_unfreeze. Ability effects and the event script use the pair. */
void map_freeze(void) {
    map_dispatch_map_data_command(MAP_DATA_COMMAND_DISABLE_ALL_TEXTURE_ANIMATIONS, 0, 0, 0);
}
