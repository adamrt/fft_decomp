#include "fft/battle.h"
#include "fft/event_small.h"
#include "psx/types.h"

s32 battle_poll_companion_executable_request(void) {
    if (g_battle_menu_companion_executable_request_index == 0) {
        return 0;
    }
    battle_menu_open_companion_executable();
    if (g_battle_menu_companion_executable_request_index != 0) {
        return 1;
    }
    small_text_init_battle_pointers();
    return 0;
}
