#include "fft/battle.h"
#include "fft/etc.h"
#include "fft/thread.h"
#include "psx/types.h"

void battle_script_show_graphic(void) {
    battle_thread_set_current_task_id(NATIVE_THREAD_TASK_SHOW_GRAPHIC);
    battle_menu_request_open_companion_executable(0xC);
    etc_graphic_show_async();
}
