#include "fft/world.h"

s32 world_shop_get_gil_minus_fitting_room_cost(void) {
    s32 gil;

    gil = world_shop_obtain_gil(0);
    return gil - world_shop_add_fitting_room_cost(0);
}
