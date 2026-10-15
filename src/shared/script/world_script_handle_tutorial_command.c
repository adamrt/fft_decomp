#include "fft/world.h"

/*
 * Per-frame step of the tutorial script. Returns 1 while no tutorial runs or
 * the current one continues, and the END command's result when it ends.
 *
 * Reads pad 0 as the tutorial's own button input (the soft-reset combination
 * resets the game), advances the highlight brightness while a wait is timed,
 * and dispatches the command byte at g_world_script_tutorial_command_ptr:
 * values below WORLD_TUTORIAL_CMD_SHIFT_LIMIT are pad-input commands, the
 * rest display a message, draw the highlight ring, wait or end.
 */
s32 world_script_handle_tutorial_command(void) {
    s32 pad;
    u8 cmd;

    if (g_world_script_tutorial_id == 0) {
        return 1;
    }
    g_world_script_tutorial_button_input = PadRead(0);
    if (g_world_script_tutorial_button_input == MAIN_INPUT_RESET_BUTTONS) {
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
    } else if (cmd == WORLD_TUTORIAL_CMD_WAIT_FOR_BUTTON) {
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
