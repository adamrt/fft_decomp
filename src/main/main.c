#include "fft/main_runtime.h"
#include "psx/types.h"

void main(void) {
    main_boot_run_startup();
    main_system_store_stack_pointer(&g_main_system_game_loop_stack_pointer);
    main_system_run_game_loop();
    PadStop();
    StopCallback();
}
