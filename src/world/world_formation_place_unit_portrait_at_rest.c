#include "fft/world.h"
#include "psx/types.h"

void world_formation_place_unit_portrait_at_rest(s16 unit_index) {
    s16 pos[2];

    pos[0] = 0x8B;
    pos[1] = 0x9E;
    world_formation_draw_unit_sprite(unit_index, (world_menu_point_t*)pos, (s8*)0, 6);
}
