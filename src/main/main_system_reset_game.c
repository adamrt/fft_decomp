#include "fft/main.h"

void main_system_reset_game(void) {
    main_system_stop_display_and_audio_for_game_reset();
    main_restore_game_loop_stack_pointer(&g_main_system_game_loop_stack_pointer);
}
