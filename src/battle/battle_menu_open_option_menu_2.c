#include "fft/battle.h"
#include "psx/types.h"

void battle_menu_open_option_menu_2(void) {
    battle_menu_set_option_menu_open();
    battle_menu_request_open_companion_executable(1);
    option_entrypoint(2);
}
