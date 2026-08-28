#include "fft/world.h"
#include "psx/types.h"

s32 world_shop_add_fitting_room_cost(s32 delta) {
    return g_world_shop_fitting_room_cost = delta + g_world_shop_fitting_room_cost;
}
