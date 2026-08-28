#include "fft/open.h"

void open_controller_dispatch_current(void) {
    s32 controller;

    controller = g_open_current_controller_index;
    g_open_controller_handlers[g_open_controller_handler_indices[controller - 1]](
        &g_open_controller_records[controller]);
}
