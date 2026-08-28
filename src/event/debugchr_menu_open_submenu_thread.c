#include "fft/battle_text.h"
#include "fft/debugchr.h"
#include "fft/thread.h"
#include "psx/pad.h"
#include "psx/types.h"

void debugchr_menu_open_submenu_thread(void) {
    u8* state;
    u8* parameters;
    u32* input;
    u16 thread_id;
    u16 parameter_0;
    u16 parameter_1;
    u16 parameter_2;

    state = ((u8**)g_battle_threads)[g_battle_current_thread_id << 8];
    (*(void (**)(void))(*(u8**)(state + 0x30)))();
    input = battle_script_get_controller_input_pointer(0);
    parameter_1 = *(u16*)(state + 0x1c);
    parameters = *(u8**)(state + 0x30);
    parameter_0 = *(u16*)(state + 0x2c);
    g_debugchr_input_controller = input;
    thread_id = *(u16*)(parameters + 6);
    parameter_2 = *(u16*)(parameters + 4);
    *input = PSX_PAD_CIRCLE;
    battle_menu_handle_action(state, 0);
    battle_thread_start(thread_id, battle_text_character_handling_thread);
    battle_thread_set_parameters_4(thread_id, parameter_0, parameter_1, parameter_2, parameter_2);
    battle_thread_wait_until_inactive(g_battle_current_thread_id - 1);
    battle_thread_wait_until_inactive(thread_id);
    battle_thread_exit_current();
}
