#include "fft/script_variables.h"
#include "fft/world.h"
#include "psx/types.h"

s32 world_shop_obtain_gil(s32 delta) {
    s32 gil;

    gil = world_script_get_variable(EVENT_SCRIPT_VAR_WAR_FUNDS) + delta;
    if (gil < 0) {
        gil = 0;
    }
    if (gil > 0x5F5E0FF) {
        gil = 0x5F5E0FF;
    }
    world_script_set_variable(EVENT_SCRIPT_VAR_WAR_FUNDS, gil);
    return gil;
}
