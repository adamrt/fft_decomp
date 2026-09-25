#include "fft/battle.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Returns the camera-relative cursor move direction (0x000-0xE00 in 0x200
 * steps) for the d-pad state, or -1 when none applies or a 0x9F0 button bit
 * is pressed.
 *
 * `mode` (0-2) selects the input source. The target reads `input` from $a1
 * without setting it when `mode` or the controller mode is out of range;
 * callers pass only `mode`. */
s32 battle_camera_get_input_direction(s32 mode, u32 input) {
    s32 direction;
    u32 previous;
    u32 pressed;
    u32 divisor;
    u32 threshold;

    direction = -1;
    switch (g_battle_controller_input) {
    case 0:
    case 1:
        switch (mode) {
        case 0:
            input = g_controller_input_previous;
            break;
        case 1:
            input = g_controller_input_pressed;
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
            if (threshold < g_controller_stable_frames && g_controller_stable_frames % divisor == 0) {
                input = g_controller_input_previous;
            }
            break;
        case 2:
            input = g_controller_input_released;
            break;
        }
        break;
    case 2:
        switch (mode) {
        case 0:
            input = g_controller_input_previous | g_controller_previous_copy_0;
            break;
        case 1:
            previous = g_controller_input_previous | g_controller_previous_copy_0;
            pressed = g_controller_input_pressed;
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
            } else if (g_controller_pressed_copy_1 & PSX_PAD_UP) {
                if (!((pressed | g_controller_pressed_copy_0) & PSX_PAD_UP) && (previous & PSX_PAD_UP)) {
                    input = PSX_PAD_UP;
                }
            } else if (g_controller_pressed_copy_1 & PSX_PAD_DOWN) {
                if (!((pressed | g_controller_pressed_copy_0) & PSX_PAD_DOWN) && (previous & PSX_PAD_DOWN)) {
                    input = PSX_PAD_DOWN;
                }
            } else if (g_controller_pressed_copy_1 & PSX_PAD_LEFT) {
                if (!((pressed | g_controller_pressed_copy_0) & PSX_PAD_LEFT) && (previous & PSX_PAD_LEFT)) {
                    input = PSX_PAD_LEFT;
                }
            } else if (g_controller_pressed_copy_1 & PSX_PAD_RIGHT) {
                if (!((pressed | g_controller_pressed_copy_0) & PSX_PAD_RIGHT) && (previous & PSX_PAD_RIGHT)) {
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
            if (threshold < g_controller_stable_frames && g_controller_stable_frames % divisor == 0) {
                input = g_controller_input_previous | g_controller_previous_copy_0;
            }
            break;
        case 2:
            input = g_controller_input_released;
            break;
        }
        break;
    }

    switch (g_battle_controller_input) {
    case 0:
        switch (g_battle_camera_render_state.vy & 0xC00) {
        case 0:
            if ((input & (PSX_PAD_UP | PSX_PAD_RIGHT)) == (PSX_PAD_UP | PSX_PAD_RIGHT)) {
                direction = 0xA00;
            } else if ((input & (PSX_PAD_UP | PSX_PAD_LEFT)) == (PSX_PAD_UP | PSX_PAD_LEFT)) {
                direction = 0x600;
            } else if ((input & (PSX_PAD_RIGHT | PSX_PAD_DOWN)) == (PSX_PAD_RIGHT | PSX_PAD_DOWN)) {
                direction = 0xE00;
            } else if ((input & (PSX_PAD_DOWN | PSX_PAD_LEFT)) == (PSX_PAD_DOWN | PSX_PAD_LEFT)) {
                direction = 0x200;
            } else if (input & PSX_PAD_UP) {
                direction = 0x800;
            } else if (input & PSX_PAD_DOWN) {
                direction = 0;
            } else if (input & PSX_PAD_LEFT) {
                direction = 0x400;
            } else if (input & PSX_PAD_RIGHT) {
                direction = 0xC00;
            }
            break;
        case 0x400:
            if ((input & (PSX_PAD_UP | PSX_PAD_RIGHT)) == (PSX_PAD_UP | PSX_PAD_RIGHT)) {
                direction = 0x600;
            } else if ((input & (PSX_PAD_UP | PSX_PAD_LEFT)) == (PSX_PAD_UP | PSX_PAD_LEFT)) {
                direction = 0x200;
            } else if ((input & (PSX_PAD_RIGHT | PSX_PAD_DOWN)) == (PSX_PAD_RIGHT | PSX_PAD_DOWN)) {
                direction = 0xA00;
            } else if ((input & (PSX_PAD_DOWN | PSX_PAD_LEFT)) == (PSX_PAD_DOWN | PSX_PAD_LEFT)) {
                direction = 0xE00;
            } else if (input & PSX_PAD_UP) {
                direction = 0x400;
            } else if (input & PSX_PAD_DOWN) {
                direction = 0xC00;
            } else if (input & PSX_PAD_LEFT) {
                direction = 0;
            } else if (input & PSX_PAD_RIGHT) {
                direction = 0x800;
            }
            break;
        case 0x800:
            if ((input & (PSX_PAD_UP | PSX_PAD_RIGHT)) == (PSX_PAD_UP | PSX_PAD_RIGHT)) {
                direction = 0x200;
            } else if ((input & (PSX_PAD_UP | PSX_PAD_LEFT)) == (PSX_PAD_UP | PSX_PAD_LEFT)) {
                direction = 0xE00;
            } else if ((input & (PSX_PAD_RIGHT | PSX_PAD_DOWN)) == (PSX_PAD_RIGHT | PSX_PAD_DOWN)) {
                direction = 0x600;
            } else if ((input & (PSX_PAD_DOWN | PSX_PAD_LEFT)) == (PSX_PAD_DOWN | PSX_PAD_LEFT)) {
                direction = 0xA00;
            } else if (input & PSX_PAD_UP) {
                direction = 0;
            } else if (input & PSX_PAD_DOWN) {
                direction = 0x800;
            } else if (input & PSX_PAD_LEFT) {
                direction = 0xC00;
            } else if (input & PSX_PAD_RIGHT) {
                direction = 0x400;
            }
            break;
        case 0xC00:
            if ((input & (PSX_PAD_UP | PSX_PAD_RIGHT)) == (PSX_PAD_UP | PSX_PAD_RIGHT)) {
                direction = 0xE00;
            } else if ((input & (PSX_PAD_UP | PSX_PAD_LEFT)) == (PSX_PAD_UP | PSX_PAD_LEFT)) {
                direction = 0xA00;
            } else if ((input & (PSX_PAD_RIGHT | PSX_PAD_DOWN)) == (PSX_PAD_RIGHT | PSX_PAD_DOWN)) {
                direction = 0x200;
            } else if ((input & (PSX_PAD_DOWN | PSX_PAD_LEFT)) == (PSX_PAD_DOWN | PSX_PAD_LEFT)) {
                direction = 0x600;
            } else if (input & PSX_PAD_UP) {
                direction = 0xC00;
            } else if (input & PSX_PAD_DOWN) {
                direction = 0x400;
            } else if (input & PSX_PAD_LEFT) {
                direction = 0x800;
            } else if (input & PSX_PAD_RIGHT) {
                direction = 0;
            }
            break;
        }
        break;
    case 1:
        switch (g_battle_camera_render_state.vy & 0xC00) {
        case 0:
            if ((input & (PSX_PAD_UP | PSX_PAD_RIGHT)) == (PSX_PAD_UP | PSX_PAD_RIGHT)) {
                direction = 0x600;
            } else if ((input & (PSX_PAD_UP | PSX_PAD_LEFT)) == (PSX_PAD_UP | PSX_PAD_LEFT)) {
                direction = 0x200;
            } else if ((input & (PSX_PAD_RIGHT | PSX_PAD_DOWN)) == (PSX_PAD_RIGHT | PSX_PAD_DOWN)) {
                direction = 0xA00;
            } else if ((input & (PSX_PAD_DOWN | PSX_PAD_LEFT)) == (PSX_PAD_DOWN | PSX_PAD_LEFT)) {
                direction = 0xE00;
            } else if (input & PSX_PAD_UP) {
                direction = 0x400;
            } else if (input & PSX_PAD_DOWN) {
                direction = 0xC00;
            } else if (input & PSX_PAD_LEFT) {
                direction = 0;
            } else if (input & PSX_PAD_RIGHT) {
                direction = 0x800;
            }
            break;
        case 0x400:
            if ((input & (PSX_PAD_UP | PSX_PAD_RIGHT)) == (PSX_PAD_UP | PSX_PAD_RIGHT)) {
                direction = 0x200;
            } else if ((input & (PSX_PAD_UP | PSX_PAD_LEFT)) == (PSX_PAD_UP | PSX_PAD_LEFT)) {
                direction = 0xE00;
            } else if ((input & (PSX_PAD_RIGHT | PSX_PAD_DOWN)) == (PSX_PAD_RIGHT | PSX_PAD_DOWN)) {
                direction = 0x600;
            } else if ((input & (PSX_PAD_DOWN | PSX_PAD_LEFT)) == (PSX_PAD_DOWN | PSX_PAD_LEFT)) {
                direction = 0xA00;
            } else if (input & PSX_PAD_UP) {
                direction = 0;
            } else if (input & PSX_PAD_DOWN) {
                direction = 0x800;
            } else if (input & PSX_PAD_LEFT) {
                direction = 0xC00;
            } else if (input & PSX_PAD_RIGHT) {
                direction = 0x400;
            }
            break;
        case 0x800:
            if ((input & (PSX_PAD_UP | PSX_PAD_RIGHT)) == (PSX_PAD_UP | PSX_PAD_RIGHT)) {
                direction = 0xE00;
            } else if ((input & (PSX_PAD_UP | PSX_PAD_LEFT)) == (PSX_PAD_UP | PSX_PAD_LEFT)) {
                direction = 0xA00;
            } else if ((input & (PSX_PAD_RIGHT | PSX_PAD_DOWN)) == (PSX_PAD_RIGHT | PSX_PAD_DOWN)) {
                direction = 0x200;
            } else if ((input & (PSX_PAD_DOWN | PSX_PAD_LEFT)) == (PSX_PAD_DOWN | PSX_PAD_LEFT)) {
                direction = 0x600;
            } else if (input & PSX_PAD_UP) {
                direction = 0xC00;
            } else if (input & PSX_PAD_DOWN) {
                direction = 0x400;
            } else if (input & PSX_PAD_LEFT) {
                direction = 0x800;
            } else if (input & PSX_PAD_RIGHT) {
                direction = 0;
            }
            break;
        case 0xC00:
            if ((input & (PSX_PAD_UP | PSX_PAD_RIGHT)) == (PSX_PAD_UP | PSX_PAD_RIGHT)) {
                direction = 0xA00;
            } else if ((input & (PSX_PAD_UP | PSX_PAD_LEFT)) == (PSX_PAD_UP | PSX_PAD_LEFT)) {
                direction = 0x600;
            } else if ((input & (PSX_PAD_RIGHT | PSX_PAD_DOWN)) == (PSX_PAD_RIGHT | PSX_PAD_DOWN)) {
                direction = 0xE00;
            } else if ((input & (PSX_PAD_DOWN | PSX_PAD_LEFT)) == (PSX_PAD_DOWN | PSX_PAD_LEFT)) {
                direction = 0x200;
            } else if (input & PSX_PAD_UP) {
                direction = 0x800;
            } else if (input & PSX_PAD_DOWN) {
                direction = 0;
            } else if (input & PSX_PAD_LEFT) {
                direction = 0x400;
            } else if (input & PSX_PAD_RIGHT) {
                direction = 0xC00;
            }
            break;
        }
        break;
    case 2:
        switch (g_battle_camera_render_state.vy & 0xC00) {
        case 0:
            if ((input & (PSX_PAD_UP | PSX_PAD_RIGHT)) == (PSX_PAD_UP | PSX_PAD_RIGHT)) {
                direction = 0x800;
            } else if ((input & (PSX_PAD_UP | PSX_PAD_LEFT)) == (PSX_PAD_UP | PSX_PAD_LEFT)) {
                direction = 0x400;
            } else if ((input & (PSX_PAD_RIGHT | PSX_PAD_DOWN)) == (PSX_PAD_RIGHT | PSX_PAD_DOWN)) {
                direction = 0xC00;
            } else if ((input & (PSX_PAD_DOWN | PSX_PAD_LEFT)) == (PSX_PAD_DOWN | PSX_PAD_LEFT)) {
                direction = 0;
            } else if (input & PSX_PAD_UP) {
                direction = 0x600;
            } else if (input & PSX_PAD_DOWN) {
                direction = 0xE00;
            } else if (input & PSX_PAD_LEFT) {
                direction = 0x200;
            } else if (input & PSX_PAD_RIGHT) {
                direction = 0xA00;
            }
            break;
        case 0x400:
            if ((input & (PSX_PAD_UP | PSX_PAD_RIGHT)) == (PSX_PAD_UP | PSX_PAD_RIGHT)) {
                direction = 0x400;
            } else if ((input & (PSX_PAD_UP | PSX_PAD_LEFT)) == (PSX_PAD_UP | PSX_PAD_LEFT)) {
                direction = 0;
            } else if ((input & (PSX_PAD_RIGHT | PSX_PAD_DOWN)) == (PSX_PAD_RIGHT | PSX_PAD_DOWN)) {
                direction = 0x800;
            } else if ((input & (PSX_PAD_DOWN | PSX_PAD_LEFT)) == (PSX_PAD_DOWN | PSX_PAD_LEFT)) {
                direction = 0xC00;
            } else if (input & PSX_PAD_UP) {
                direction = 0x200;
            } else if (input & PSX_PAD_DOWN) {
                direction = 0xA00;
            } else if (input & PSX_PAD_LEFT) {
                direction = 0xE00;
            } else if (input & PSX_PAD_RIGHT) {
                direction = 0x600;
            }
            break;
        case 0x800:
            if ((input & (PSX_PAD_UP | PSX_PAD_RIGHT)) == (PSX_PAD_UP | PSX_PAD_RIGHT)) {
                direction = 0;
            } else if ((input & (PSX_PAD_UP | PSX_PAD_LEFT)) == (PSX_PAD_UP | PSX_PAD_LEFT)) {
                direction = 0xC00;
            } else if ((input & (PSX_PAD_RIGHT | PSX_PAD_DOWN)) == (PSX_PAD_RIGHT | PSX_PAD_DOWN)) {
                direction = 0x400;
            } else if ((input & (PSX_PAD_DOWN | PSX_PAD_LEFT)) == (PSX_PAD_DOWN | PSX_PAD_LEFT)) {
                direction = 0x800;
            } else if (input & PSX_PAD_UP) {
                direction = 0xE00;
            } else if (input & PSX_PAD_DOWN) {
                direction = 0x600;
            } else if (input & PSX_PAD_LEFT) {
                direction = 0xA00;
            } else if (input & PSX_PAD_RIGHT) {
                direction = 0x200;
            }
            break;
        case 0xC00:
            if ((input & (PSX_PAD_UP | PSX_PAD_RIGHT)) == (PSX_PAD_UP | PSX_PAD_RIGHT)) {
                direction = 0xC00;
            } else if ((input & (PSX_PAD_UP | PSX_PAD_LEFT)) == (PSX_PAD_UP | PSX_PAD_LEFT)) {
                direction = 0x800;
            } else if ((input & (PSX_PAD_RIGHT | PSX_PAD_DOWN)) == (PSX_PAD_RIGHT | PSX_PAD_DOWN)) {
                direction = 0;
            } else if ((input & (PSX_PAD_DOWN | PSX_PAD_LEFT)) == (PSX_PAD_DOWN | PSX_PAD_LEFT)) {
                direction = 0x400;
            } else if (input & PSX_PAD_UP) {
                direction = 0xA00;
            } else if (input & PSX_PAD_DOWN) {
                direction = 0x200;
            } else if (input & PSX_PAD_LEFT) {
                direction = 0x600;
            } else if (input & PSX_PAD_RIGHT) {
                direction = 0xE00;
            }
            break;
        }
        break;
    }
    if (g_controller_input_pressed
        & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE | PSX_PAD_SELECT | PSX_PAD_START)) {
        direction = -1;
    }
    return direction;
}
