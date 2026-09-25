#include "fft/world.h"

/* L1 + R1 + Select + Start restarts the game. */
#define TUTORIAL_RESET_BUTTONS 0x90C

s32 world_script_handle_tutorial_command(void) {
    s32 pad;
    u8 cmd;

    if (g_world_script_tutorial_id == 0) {
        return 1;
    }
    g_world_script_tutorial_button_input = PadRead(0);
    if (g_world_script_tutorial_button_input == TUTORIAL_RESET_BUTTONS) {
        world_game_reset();
    }
    g_world_script_tutorial_buttons = g_world_script_tutorial_button_input;
    if (g_world_script_tutorial_wait_timer != 0) {
        world_script_advance_tutorial_highlight_brightness();
    }
    cmd = *g_world_script_tutorial_command_ptr;
    if (cmd < WORLD_TUTORIAL_CMD_SHIFT_LIMIT) {
        world_script_handle_tutorial_command_shift();
    } else if (cmd == WORLD_TUTORIAL_CMD_DISPLAY_MESSAGE) {
        world_script_handle_tutorial_command_display_message();
    } else if (cmd == WORLD_TUTORIAL_CMD_DRAW_CIRCLE) {
        world_script_handle_tutorial_command_highlight_ring();
    } else if (cmd == WORLD_TUTORIAL_CMD_WAIT) {
        world_script_handle_tutorial_command_wait();
    } else if (cmd == WORLD_TUTORIAL_CMD_WAIT_TIME) {
        world_script_handle_tutorial_command_wait_time();
    } else if (cmd == WORLD_TUTORIAL_CMD_WAIT_FOR_MESSAGE_2) {
        world_script_handle_tutorial_command_wait_for_button();
    } else if (cmd == WORLD_TUTORIAL_CMD_END) {
        return world_script_handle_tutorial_command_end();
    } else if (cmd == WORLD_TUTORIAL_CMD_WAIT_FOR_MESSAGE) {
        world_script_handle_tutorial_command_wait_for_message();
    } else if (cmd == WORLD_TUTORIAL_CMD_CHANGE_DIALOG) {
        world_script_handle_tutorial_command_change_dialog();
    }
    return 1;
}
