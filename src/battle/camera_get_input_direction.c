#include "fft/battle_camera.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Returns the camera-relative cursor move direction (0x000-0xE00 in 0x200
 * steps) for the d-pad state, or -1 when none applies or a 0x9F0 button bit
 * is pressed.
 *
 * `source` (0-2) selects the input source. `input` stays unset when `source` or
 * the controller mode is out of range: the direction tests then read whatever
 * the caller left in $a1. */
s32 camera_get_input_direction(s32 source) {
    s32 direction;
    u32 input;
    u32 previous;
    u32 pressed;
    u32 divisor;
    u32 threshold;

    direction = -1;
    switch (g_battle_cursor_input_mode) {
    case BATTLE_CURSOR_INPUT_OPTION_A:
    case BATTLE_CURSOR_INPUT_OPTION_B:
        switch (source) {
        case CAMERA_INPUT_HELD:
            input = g_input_previous_buttons;
            break;
        case CAMERA_INPUT_PRESSED:
            input = g_main_input_pressed_buttons;
            switch (g_main_game_options.fields.cursor_repeat_speed) {
            case 0:
                divisor = 3;
                threshold = 15;
                break;
            case 1:
                divisor = 6;
                threshold = 30;
                break;
            case 2:
            default:
                divisor = 12;
                threshold = 60;
                break;
            }
            if (threshold < g_main_input_stable_frames && g_main_input_stable_frames % divisor == 0) {
                input = g_input_previous_buttons;
            }
            break;
        case CAMERA_INPUT_RELEASED:
            input = g_main_input_released_buttons;
            break;
        }
        break;
    case BATTLE_CURSOR_INPUT_FREE_CURSOR:
        switch (source) {
        case CAMERA_INPUT_HELD:
            input = g_input_previous_buttons | g_main_input_previous_copy_0;
            break;
        case CAMERA_INPUT_PRESSED:
            previous = g_input_previous_buttons | g_main_input_previous_copy_0;
            pressed = g_main_input_pressed_buttons;
            input = 0;
            if (pressed & PSX_PAD_UP) {
                if (previous & PSX_PAD_RIGHT) {
                    input = PSX_PAD_UP | PSX_PAD_RIGHT;
                } else if (previous & PSX_PAD_LEFT) {
                    input = PSX_PAD_UP | PSX_PAD_LEFT;
                }
            } else if (pressed & PSX_PAD_DOWN) {
                if (previous & PSX_PAD_RIGHT) {
                    input = PSX_PAD_RIGHT | PSX_PAD_DOWN;
                } else if (previous & PSX_PAD_LEFT) {
                    input = PSX_PAD_DOWN | PSX_PAD_LEFT;
                }
            } else if (pressed & PSX_PAD_LEFT) {
                if (previous & PSX_PAD_UP) {
                    input = PSX_PAD_UP | PSX_PAD_LEFT;
                } else if (previous & PSX_PAD_DOWN) {
                    input = PSX_PAD_DOWN | PSX_PAD_LEFT;
                }
            } else if (pressed & PSX_PAD_RIGHT) {
                if (previous & PSX_PAD_UP) {
                    input = PSX_PAD_UP | PSX_PAD_RIGHT;
                } else if (previous & PSX_PAD_DOWN) {
                    input = PSX_PAD_RIGHT | PSX_PAD_DOWN;
                }
            } else if (g_main_input_pressed_copy_1 & PSX_PAD_UP) {
                if (!((pressed | g_main_input_pressed_copy_0) & PSX_PAD_UP) && (previous & PSX_PAD_UP)) {
                    input = PSX_PAD_UP;
                }
            } else if (g_main_input_pressed_copy_1 & PSX_PAD_DOWN) {
                if (!((pressed | g_main_input_pressed_copy_0) & PSX_PAD_DOWN) && (previous & PSX_PAD_DOWN)) {
                    input = PSX_PAD_DOWN;
                }
            } else if (g_main_input_pressed_copy_1 & PSX_PAD_LEFT) {
                if (!((pressed | g_main_input_pressed_copy_0) & PSX_PAD_LEFT) && (previous & PSX_PAD_LEFT)) {
                    input = PSX_PAD_LEFT;
                }
            } else if (g_main_input_pressed_copy_1 & PSX_PAD_RIGHT) {
                if (!((pressed | g_main_input_pressed_copy_0) & PSX_PAD_RIGHT) && (previous & PSX_PAD_RIGHT)) {
                    input = PSX_PAD_RIGHT;
                }
            }
            switch (g_main_game_options.fields.cursor_repeat_speed) {
            case 0:
                divisor = 6;
                threshold = 6;
                break;
            case 1:
                divisor = 10;
                threshold = 15;
                break;
            case 2:
            default:
                divisor = 16;
                threshold = 30;
                break;
            }
            if (threshold < g_main_input_stable_frames && g_main_input_stable_frames % divisor == 0) {
                input = g_input_previous_buttons | g_main_input_previous_copy_0;
            }
            break;
        case CAMERA_INPUT_RELEASED:
            input = g_main_input_released_buttons;
            break;
        }
        break;
    }

    switch (g_battle_cursor_input_mode) {
    case BATTLE_CURSOR_INPUT_OPTION_A:
        switch (g_camera_render_state.vy & 0xC00) {
        case BATTLE_FACING_SOUTH:
            if ((input & (PSX_PAD_UP | PSX_PAD_RIGHT)) == (PSX_PAD_UP | PSX_PAD_RIGHT)) {
                direction = BATTLE_FACING_NORTH_EAST;
            } else if ((input & (PSX_PAD_UP | PSX_PAD_LEFT)) == (PSX_PAD_UP | PSX_PAD_LEFT)) {
                direction = BATTLE_FACING_NORTH_WEST;
            } else if ((input & (PSX_PAD_RIGHT | PSX_PAD_DOWN)) == (PSX_PAD_RIGHT | PSX_PAD_DOWN)) {
                direction = BATTLE_FACING_SOUTH_EAST;
            } else if ((input & (PSX_PAD_DOWN | PSX_PAD_LEFT)) == (PSX_PAD_DOWN | PSX_PAD_LEFT)) {
                direction = BATTLE_FACING_SOUTH_WEST;
            } else if (input & PSX_PAD_UP) {
                direction = BATTLE_FACING_NORTH;
            } else if (input & PSX_PAD_DOWN) {
                direction = BATTLE_FACING_SOUTH;
            } else if (input & PSX_PAD_LEFT) {
                direction = BATTLE_FACING_WEST;
            } else if (input & PSX_PAD_RIGHT) {
                direction = BATTLE_FACING_EAST;
            }
            break;
        case BATTLE_FACING_WEST:
            if ((input & (PSX_PAD_UP | PSX_PAD_RIGHT)) == (PSX_PAD_UP | PSX_PAD_RIGHT)) {
                direction = BATTLE_FACING_NORTH_WEST;
            } else if ((input & (PSX_PAD_UP | PSX_PAD_LEFT)) == (PSX_PAD_UP | PSX_PAD_LEFT)) {
                direction = BATTLE_FACING_SOUTH_WEST;
            } else if ((input & (PSX_PAD_RIGHT | PSX_PAD_DOWN)) == (PSX_PAD_RIGHT | PSX_PAD_DOWN)) {
                direction = BATTLE_FACING_NORTH_EAST;
            } else if ((input & (PSX_PAD_DOWN | PSX_PAD_LEFT)) == (PSX_PAD_DOWN | PSX_PAD_LEFT)) {
                direction = BATTLE_FACING_SOUTH_EAST;
            } else if (input & PSX_PAD_UP) {
                direction = BATTLE_FACING_WEST;
            } else if (input & PSX_PAD_DOWN) {
                direction = BATTLE_FACING_EAST;
            } else if (input & PSX_PAD_LEFT) {
                direction = BATTLE_FACING_SOUTH;
            } else if (input & PSX_PAD_RIGHT) {
                direction = BATTLE_FACING_NORTH;
            }
            break;
        case BATTLE_FACING_NORTH:
            if ((input & (PSX_PAD_UP | PSX_PAD_RIGHT)) == (PSX_PAD_UP | PSX_PAD_RIGHT)) {
                direction = BATTLE_FACING_SOUTH_WEST;
            } else if ((input & (PSX_PAD_UP | PSX_PAD_LEFT)) == (PSX_PAD_UP | PSX_PAD_LEFT)) {
                direction = BATTLE_FACING_SOUTH_EAST;
            } else if ((input & (PSX_PAD_RIGHT | PSX_PAD_DOWN)) == (PSX_PAD_RIGHT | PSX_PAD_DOWN)) {
                direction = BATTLE_FACING_NORTH_WEST;
            } else if ((input & (PSX_PAD_DOWN | PSX_PAD_LEFT)) == (PSX_PAD_DOWN | PSX_PAD_LEFT)) {
                direction = BATTLE_FACING_NORTH_EAST;
            } else if (input & PSX_PAD_UP) {
                direction = BATTLE_FACING_SOUTH;
            } else if (input & PSX_PAD_DOWN) {
                direction = BATTLE_FACING_NORTH;
            } else if (input & PSX_PAD_LEFT) {
                direction = BATTLE_FACING_EAST;
            } else if (input & PSX_PAD_RIGHT) {
                direction = BATTLE_FACING_WEST;
            }
            break;
        case BATTLE_FACING_EAST:
            if ((input & (PSX_PAD_UP | PSX_PAD_RIGHT)) == (PSX_PAD_UP | PSX_PAD_RIGHT)) {
                direction = BATTLE_FACING_SOUTH_EAST;
            } else if ((input & (PSX_PAD_UP | PSX_PAD_LEFT)) == (PSX_PAD_UP | PSX_PAD_LEFT)) {
                direction = BATTLE_FACING_NORTH_EAST;
            } else if ((input & (PSX_PAD_RIGHT | PSX_PAD_DOWN)) == (PSX_PAD_RIGHT | PSX_PAD_DOWN)) {
                direction = BATTLE_FACING_SOUTH_WEST;
            } else if ((input & (PSX_PAD_DOWN | PSX_PAD_LEFT)) == (PSX_PAD_DOWN | PSX_PAD_LEFT)) {
                direction = BATTLE_FACING_NORTH_WEST;
            } else if (input & PSX_PAD_UP) {
                direction = BATTLE_FACING_EAST;
            } else if (input & PSX_PAD_DOWN) {
                direction = BATTLE_FACING_WEST;
            } else if (input & PSX_PAD_LEFT) {
                direction = BATTLE_FACING_NORTH;
            } else if (input & PSX_PAD_RIGHT) {
                direction = BATTLE_FACING_SOUTH;
            }
            break;
        }
        break;
    case BATTLE_CURSOR_INPUT_OPTION_B:
        switch (g_camera_render_state.vy & 0xC00) {
        case BATTLE_FACING_SOUTH:
            if ((input & (PSX_PAD_UP | PSX_PAD_RIGHT)) == (PSX_PAD_UP | PSX_PAD_RIGHT)) {
                direction = BATTLE_FACING_NORTH_WEST;
            } else if ((input & (PSX_PAD_UP | PSX_PAD_LEFT)) == (PSX_PAD_UP | PSX_PAD_LEFT)) {
                direction = BATTLE_FACING_SOUTH_WEST;
            } else if ((input & (PSX_PAD_RIGHT | PSX_PAD_DOWN)) == (PSX_PAD_RIGHT | PSX_PAD_DOWN)) {
                direction = BATTLE_FACING_NORTH_EAST;
            } else if ((input & (PSX_PAD_DOWN | PSX_PAD_LEFT)) == (PSX_PAD_DOWN | PSX_PAD_LEFT)) {
                direction = BATTLE_FACING_SOUTH_EAST;
            } else if (input & PSX_PAD_UP) {
                direction = BATTLE_FACING_WEST;
            } else if (input & PSX_PAD_DOWN) {
                direction = BATTLE_FACING_EAST;
            } else if (input & PSX_PAD_LEFT) {
                direction = BATTLE_FACING_SOUTH;
            } else if (input & PSX_PAD_RIGHT) {
                direction = BATTLE_FACING_NORTH;
            }
            break;
        case BATTLE_FACING_WEST:
            if ((input & (PSX_PAD_UP | PSX_PAD_RIGHT)) == (PSX_PAD_UP | PSX_PAD_RIGHT)) {
                direction = BATTLE_FACING_SOUTH_WEST;
            } else if ((input & (PSX_PAD_UP | PSX_PAD_LEFT)) == (PSX_PAD_UP | PSX_PAD_LEFT)) {
                direction = BATTLE_FACING_SOUTH_EAST;
            } else if ((input & (PSX_PAD_RIGHT | PSX_PAD_DOWN)) == (PSX_PAD_RIGHT | PSX_PAD_DOWN)) {
                direction = BATTLE_FACING_NORTH_WEST;
            } else if ((input & (PSX_PAD_DOWN | PSX_PAD_LEFT)) == (PSX_PAD_DOWN | PSX_PAD_LEFT)) {
                direction = BATTLE_FACING_NORTH_EAST;
            } else if (input & PSX_PAD_UP) {
                direction = BATTLE_FACING_SOUTH;
            } else if (input & PSX_PAD_DOWN) {
                direction = BATTLE_FACING_NORTH;
            } else if (input & PSX_PAD_LEFT) {
                direction = BATTLE_FACING_EAST;
            } else if (input & PSX_PAD_RIGHT) {
                direction = BATTLE_FACING_WEST;
            }
            break;
        case BATTLE_FACING_NORTH:
            if ((input & (PSX_PAD_UP | PSX_PAD_RIGHT)) == (PSX_PAD_UP | PSX_PAD_RIGHT)) {
                direction = BATTLE_FACING_SOUTH_EAST;
            } else if ((input & (PSX_PAD_UP | PSX_PAD_LEFT)) == (PSX_PAD_UP | PSX_PAD_LEFT)) {
                direction = BATTLE_FACING_NORTH_EAST;
            } else if ((input & (PSX_PAD_RIGHT | PSX_PAD_DOWN)) == (PSX_PAD_RIGHT | PSX_PAD_DOWN)) {
                direction = BATTLE_FACING_SOUTH_WEST;
            } else if ((input & (PSX_PAD_DOWN | PSX_PAD_LEFT)) == (PSX_PAD_DOWN | PSX_PAD_LEFT)) {
                direction = BATTLE_FACING_NORTH_WEST;
            } else if (input & PSX_PAD_UP) {
                direction = BATTLE_FACING_EAST;
            } else if (input & PSX_PAD_DOWN) {
                direction = BATTLE_FACING_WEST;
            } else if (input & PSX_PAD_LEFT) {
                direction = BATTLE_FACING_NORTH;
            } else if (input & PSX_PAD_RIGHT) {
                direction = BATTLE_FACING_SOUTH;
            }
            break;
        case BATTLE_FACING_EAST:
            if ((input & (PSX_PAD_UP | PSX_PAD_RIGHT)) == (PSX_PAD_UP | PSX_PAD_RIGHT)) {
                direction = BATTLE_FACING_NORTH_EAST;
            } else if ((input & (PSX_PAD_UP | PSX_PAD_LEFT)) == (PSX_PAD_UP | PSX_PAD_LEFT)) {
                direction = BATTLE_FACING_NORTH_WEST;
            } else if ((input & (PSX_PAD_RIGHT | PSX_PAD_DOWN)) == (PSX_PAD_RIGHT | PSX_PAD_DOWN)) {
                direction = BATTLE_FACING_SOUTH_EAST;
            } else if ((input & (PSX_PAD_DOWN | PSX_PAD_LEFT)) == (PSX_PAD_DOWN | PSX_PAD_LEFT)) {
                direction = BATTLE_FACING_SOUTH_WEST;
            } else if (input & PSX_PAD_UP) {
                direction = BATTLE_FACING_NORTH;
            } else if (input & PSX_PAD_DOWN) {
                direction = BATTLE_FACING_SOUTH;
            } else if (input & PSX_PAD_LEFT) {
                direction = BATTLE_FACING_WEST;
            } else if (input & PSX_PAD_RIGHT) {
                direction = BATTLE_FACING_EAST;
            }
            break;
        }
        break;
    case BATTLE_CURSOR_INPUT_FREE_CURSOR:
        switch (g_camera_render_state.vy & 0xC00) {
        case BATTLE_FACING_SOUTH:
            if ((input & (PSX_PAD_UP | PSX_PAD_RIGHT)) == (PSX_PAD_UP | PSX_PAD_RIGHT)) {
                direction = BATTLE_FACING_NORTH;
            } else if ((input & (PSX_PAD_UP | PSX_PAD_LEFT)) == (PSX_PAD_UP | PSX_PAD_LEFT)) {
                direction = BATTLE_FACING_WEST;
            } else if ((input & (PSX_PAD_RIGHT | PSX_PAD_DOWN)) == (PSX_PAD_RIGHT | PSX_PAD_DOWN)) {
                direction = BATTLE_FACING_EAST;
            } else if ((input & (PSX_PAD_DOWN | PSX_PAD_LEFT)) == (PSX_PAD_DOWN | PSX_PAD_LEFT)) {
                direction = BATTLE_FACING_SOUTH;
            } else if (input & PSX_PAD_UP) {
                direction = BATTLE_FACING_NORTH_WEST;
            } else if (input & PSX_PAD_DOWN) {
                direction = BATTLE_FACING_SOUTH_EAST;
            } else if (input & PSX_PAD_LEFT) {
                direction = BATTLE_FACING_SOUTH_WEST;
            } else if (input & PSX_PAD_RIGHT) {
                direction = BATTLE_FACING_NORTH_EAST;
            }
            break;
        case BATTLE_FACING_WEST:
            if ((input & (PSX_PAD_UP | PSX_PAD_RIGHT)) == (PSX_PAD_UP | PSX_PAD_RIGHT)) {
                direction = BATTLE_FACING_WEST;
            } else if ((input & (PSX_PAD_UP | PSX_PAD_LEFT)) == (PSX_PAD_UP | PSX_PAD_LEFT)) {
                direction = BATTLE_FACING_SOUTH;
            } else if ((input & (PSX_PAD_RIGHT | PSX_PAD_DOWN)) == (PSX_PAD_RIGHT | PSX_PAD_DOWN)) {
                direction = BATTLE_FACING_NORTH;
            } else if ((input & (PSX_PAD_DOWN | PSX_PAD_LEFT)) == (PSX_PAD_DOWN | PSX_PAD_LEFT)) {
                direction = BATTLE_FACING_EAST;
            } else if (input & PSX_PAD_UP) {
                direction = BATTLE_FACING_SOUTH_WEST;
            } else if (input & PSX_PAD_DOWN) {
                direction = BATTLE_FACING_NORTH_EAST;
            } else if (input & PSX_PAD_LEFT) {
                direction = BATTLE_FACING_SOUTH_EAST;
            } else if (input & PSX_PAD_RIGHT) {
                direction = BATTLE_FACING_NORTH_WEST;
            }
            break;
        case BATTLE_FACING_NORTH:
            if ((input & (PSX_PAD_UP | PSX_PAD_RIGHT)) == (PSX_PAD_UP | PSX_PAD_RIGHT)) {
                direction = BATTLE_FACING_SOUTH;
            } else if ((input & (PSX_PAD_UP | PSX_PAD_LEFT)) == (PSX_PAD_UP | PSX_PAD_LEFT)) {
                direction = BATTLE_FACING_EAST;
            } else if ((input & (PSX_PAD_RIGHT | PSX_PAD_DOWN)) == (PSX_PAD_RIGHT | PSX_PAD_DOWN)) {
                direction = BATTLE_FACING_WEST;
            } else if ((input & (PSX_PAD_DOWN | PSX_PAD_LEFT)) == (PSX_PAD_DOWN | PSX_PAD_LEFT)) {
                direction = BATTLE_FACING_NORTH;
            } else if (input & PSX_PAD_UP) {
                direction = BATTLE_FACING_SOUTH_EAST;
            } else if (input & PSX_PAD_DOWN) {
                direction = BATTLE_FACING_NORTH_WEST;
            } else if (input & PSX_PAD_LEFT) {
                direction = BATTLE_FACING_NORTH_EAST;
            } else if (input & PSX_PAD_RIGHT) {
                direction = BATTLE_FACING_SOUTH_WEST;
            }
            break;
        case BATTLE_FACING_EAST:
            if ((input & (PSX_PAD_UP | PSX_PAD_RIGHT)) == (PSX_PAD_UP | PSX_PAD_RIGHT)) {
                direction = BATTLE_FACING_EAST;
            } else if ((input & (PSX_PAD_UP | PSX_PAD_LEFT)) == (PSX_PAD_UP | PSX_PAD_LEFT)) {
                direction = BATTLE_FACING_NORTH;
            } else if ((input & (PSX_PAD_RIGHT | PSX_PAD_DOWN)) == (PSX_PAD_RIGHT | PSX_PAD_DOWN)) {
                direction = BATTLE_FACING_SOUTH;
            } else if ((input & (PSX_PAD_DOWN | PSX_PAD_LEFT)) == (PSX_PAD_DOWN | PSX_PAD_LEFT)) {
                direction = BATTLE_FACING_WEST;
            } else if (input & PSX_PAD_UP) {
                direction = BATTLE_FACING_NORTH_EAST;
            } else if (input & PSX_PAD_DOWN) {
                direction = BATTLE_FACING_SOUTH_WEST;
            } else if (input & PSX_PAD_LEFT) {
                direction = BATTLE_FACING_NORTH_WEST;
            } else if (input & PSX_PAD_RIGHT) {
                direction = BATTLE_FACING_SOUTH_EAST;
            }
            break;
        }
        break;
    }
    if (g_main_input_pressed_buttons
        & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE | PSX_PAD_SELECT | PSX_PAD_START)) {
        direction = -1;
    }
    return direction;
}
