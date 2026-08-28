#include "fft/event.h"
#include "fft/world.h"
#include "psx/types.h"

void world_script_wait_for_value(const u8* parameters) {
    s32 variable_id = world_script_load_halfword(parameters);
    s16 test_value = world_script_load_halfword(parameters + 2);

    do {
        if (world_script_get_variable(variable_id) >= test_value) {
            break;
        }
        world_thread_yield();
    } while (1);
}
