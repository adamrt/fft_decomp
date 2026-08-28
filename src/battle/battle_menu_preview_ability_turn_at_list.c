#include "fft/battle.h"
#include "psx/types.h"

void battle_menu_preview_ability_turn_at_list(void) {
    battle_menu_build_ability_preview_at_list();
    battle_script_pulse_tutorial_wait_value(0xFA);
    battle_menu_run_scrolling_ability_list_thread();
}
