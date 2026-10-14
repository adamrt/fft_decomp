#include "fft/battle_menu.h"

ai_command_t* menu_get_selected_command_address(void) {
    return &g_menu_selected_command;
}
