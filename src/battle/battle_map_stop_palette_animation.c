#include "fft/map.h"
#include "psx/types.h"

enum {
    MAP_PALETTE_ANIMATION_COUNT = 16,
};

void battle_map_stop_palette_animation(s32 mesh_index, s32 animation_index, s32 stop_all) {
    s32 index;

    if (stop_all == 1) {
        index = MAP_PALETTE_ANIMATION_COUNT - 1;
        do {
            g_battle_map_palette_state.banks[mesh_index].animations[index].active = 0;
            index--;
        } while (index >= 0);
    } else {
        g_battle_map_palette_state.banks[mesh_index].animations[animation_index].active = 0;
    }
}
