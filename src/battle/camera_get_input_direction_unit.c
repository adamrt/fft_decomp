#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_map.h"
#include "fft/battle_target.h"
#include "fft/menu.h"
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

/* Start the hovered-unit stats panel for the unit on the cursor tile, or the tile
 * info panel when the tile is empty. */
void target_store_cursor_unit_name_and_data(void) {
    battle_unit_t* unit;
    s32 mode;
    s32 unit_id;

    unit = battle_unit_find_selectable_at_tile(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z);
    if (unit != 0) {
        mode = UNIT_VIEW_MODE_HOVERED_UNIT;
        unit_id = unit->stats->battle_id;
    } else {
        mode = UNIT_VIEW_MODE_TILE_INFO;
        unit_id = 0xFF;
    }
    menu_start_unit_view_thread(mode, unit_id, 0xFF);
}

/* Start the right-side stats panel for the unit on the cursor tile, or the tile
 * info panel when the tile is empty. */
void target_store_cursor_unit_as_preview_target(void) {
    battle_unit_t* unit = battle_unit_find_selectable_at_tile(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z);
    battle_unit_get_source();
    if (unit != 0) {
        menu_start_unit_view_thread(UNIT_VIEW_MODE_RIGHT_UNIT, 0xFF, unit->stats->battle_id);
    } else {
        menu_start_unit_view_thread(UNIT_VIEW_MODE_TILE_INFO, 0xFF, 0xFF);
    }
}

/* Start the unit panels for a selected target tile: the attack preview for the
 * source unit against the unit on the cursor tile, or the source unit's own stats
 * when the tile is empty. */
void target_store_cursor_casting_unit_name_and_data(void) {
    battle_unit_t* target;
    battle_unit_t* source;

    target = battle_unit_find_selectable_at_tile(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z);
    source = battle_unit_get_source();
    if (target != 0) {
        menu_start_unit_view_thread(UNIT_VIEW_MODE_ATTACK_PREVIEW, source->stats->battle_id, target->stats->battle_id);
    } else {
        menu_start_unit_view_thread(UNIT_VIEW_MODE_HOVERED_UNIT, source->stats->battle_id, 0xFF);
    }
}

/* Move the cursor while a target is being chosen: onto selected_unit when one is
 * given, otherwise to the tile stored in the unit's cursor command state.
 *
 * When the cursor tile changes, the unit panels are refreshed for the current
 * game state: the cursor unit's panel in the free-cursor, close-move-help and
 * target-select states, the preview target's panel in the targeting-range state. */
void target_update_free_cursor_selection(const battle_unit_t* unit, const battle_unit_t* selected_unit) {
    s32 old_x;
    s32 old_z;
    s32 old_y;

    old_x = g_battle_cursor_x;
    old_z = g_battle_cursor_z;
    old_y = g_battle_cursor_y;
    if (selected_unit != 0) {
        target_move_cursor_to_unit(selected_unit);
    } else {
        g_battle_cursor_x = unit->command_state.cursor.target_panel.vx;
        g_battle_cursor_z = unit->command_state.cursor.target_panel.vy;
        g_battle_cursor_y = unit->command_state.cursor.target_panel.vz;
    }
    if (((g_battle_cursor_x != old_x) | (g_battle_cursor_z != old_z) | (g_battle_cursor_y != old_y)) != 0) {
        switch (g_battle_game_state) {
        case BATTLE_GAME_STATE_FREE_CURSOR:
        case BATTLE_GAME_STATE_CLOSE_MOVE_HELP:
        case BATTLE_GAME_STATE_TARGET_SELECT:
            target_store_cursor_unit_name_and_data();
            break;
        case BATTLE_GAME_STATE_TARGETING_RANGE:
            target_store_cursor_unit_as_preview_target();
            break;
        }
    }
}

/* Moves the free cursor one step from the pad direction and reports whether
 * it moved.
 *
 * Before reading the direction, the cursor may hop to the other map level:
 * on START when the multi-height cursor option is 3, otherwise once the
 * repeat counter passes the option's delay. A step onto a blocked tile whose
 * other level is also blocked is undone; otherwise the cursor takes the
 * unblocked level, or the one nearer the old tile's height.
 *
 * `direction` doubles as the level-hop flag, as the target keeps both in one
 * register. The do-while(0) weights the delay's references so that GCC
 * allocates it before `direction`; without it the two swap s0 and s1. The
 * 32-bit option value is copied first so the loop note does not stop the load from
 * being scheduled into the prologue. */
s32 target_move_cursor_by_input(void) {
    game_options_t options;
    u32 delay;
    s32 direction;
    map_tile_t* current;
    map_tile_t* lower;
    map_tile_t* upper;
    s8 lower_rise;
    s8 upper_rise;

    options = g_main_game_options;
    do {
        switch (options.fields.multi_height_cursor_speed) {
        case 0:
            delay = 30;
            break;
        case 1:
            delay = 60;
            break;
        case 2:
        default:
            delay = 90;
            break;
        }
    } while (0);
    direction = 0;
    current = map_get_tile_data_pointer(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z);
    if (g_main_game_options.fields.multi_height_cursor_speed == 3) {
        if (g_main_input_pressed_buttons & PSX_PAD_START) {
            if (!map_get_tile_data_pointer(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z ^ 1)
                    ->flags.blocked) {
                direction = 1;
                g_battle_cursor_z ^= 1;
            }
        }
    } else if (g_battle_cursor_repeat_counter > delay) {
        g_battle_cursor_repeat_counter = 0;
        if (!map_get_tile_data_pointer(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z ^ 1)->flags.blocked) {
            direction = 1;
            g_battle_cursor_z ^= 1;
        }
    }
    if (direction == 0) {
        g_battle_cursor_repeat_counter++;
        g_battle_cursor_previous_x = g_battle_cursor_x;
        g_battle_cursor_previous_z = g_battle_cursor_z;
        g_battle_cursor_previous_y = g_battle_cursor_y;
        direction = camera_get_input_direction(CAMERA_INPUT_PRESSED);
        switch (direction) {
        case BATTLE_FACING_NORTH:
            g_battle_cursor_y++;
            break;
        case BATTLE_FACING_SOUTH:
            g_battle_cursor_y--;
            break;
        case BATTLE_FACING_WEST:
            g_battle_cursor_x--;
            break;
        case BATTLE_FACING_EAST:
            g_battle_cursor_x++;
            break;
        case BATTLE_FACING_NORTH_EAST:
            g_battle_cursor_y++;
            g_battle_cursor_x++;
            break;
        case BATTLE_FACING_NORTH_WEST:
            g_battle_cursor_y++;
            g_battle_cursor_x--;
            break;
        case BATTLE_FACING_SOUTH_EAST:
            g_battle_cursor_x++;
            g_battle_cursor_y--;
            break;
        case BATTLE_FACING_SOUTH_WEST:
            g_battle_cursor_y--;
            g_battle_cursor_x--;
            break;
        }
        if (g_battle_cursor_x == g_map_max_x) {
            g_battle_cursor_x--;
        } else if ((u32)g_battle_cursor_x > g_map_max_x) {
            g_battle_cursor_x = 0;
        }
        if (g_battle_cursor_y == g_map_max_y) {
            g_battle_cursor_y = g_map_max_y - 1;
        }
        if ((u32)g_battle_cursor_y > g_map_max_y) {
            g_battle_cursor_y = 0;
        }
        lower = map_get_tile_data_pointer(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z);
        upper = map_get_tile_data_pointer(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z ^ 1);
        if (lower->flags.blocked) {
            if (upper->flags.blocked) {
                switch (direction) {
                case BATTLE_FACING_NORTH:
                    g_battle_cursor_y--;
                    break;
                case BATTLE_FACING_SOUTH:
                    g_battle_cursor_y++;
                    break;
                case BATTLE_FACING_WEST:
                    g_battle_cursor_x++;
                    break;
                case BATTLE_FACING_EAST:
                    g_battle_cursor_x--;
                    break;
                case BATTLE_FACING_NORTH_EAST:
                    g_battle_cursor_y--;
                    g_battle_cursor_x--;
                    break;
                case BATTLE_FACING_NORTH_WEST:
                    g_battle_cursor_y--;
                    g_battle_cursor_x++;
                    break;
                case BATTLE_FACING_SOUTH_EAST:
                    g_battle_cursor_x--;
                    g_battle_cursor_y++;
                    break;
                case BATTLE_FACING_SOUTH_WEST:
                    g_battle_cursor_y++;
                    g_battle_cursor_x++;
                    break;
                }
            } else {
                g_battle_cursor_z ^= 1;
            }
        } else if (!upper->flags.blocked) {
            lower_rise = lower->height - current->height;
            upper_rise = upper->height - current->height;
            if (lower_rise < 0) {
                lower_rise = -lower_rise;
            }
            if (upper_rise < 0) {
                upper_rise = -upper_rise;
            }
            if (upper_rise < lower_rise) {
                g_battle_cursor_z ^= 1;
            }
        }
    }
    if (((g_battle_cursor_x != g_battle_cursor_previous_x) | (g_battle_cursor_z != g_battle_cursor_previous_z)
            | (g_battle_cursor_y != g_battle_cursor_previous_y))
        != 0) {
        main_sound_play_sfx(MAIN_SFX_CURSOR_MOVE);
        g_battle_cursor_repeat_counter = 0;
        switch (g_battle_game_state) {
        case BATTLE_GAME_STATE_FREE_CURSOR:
        case BATTLE_GAME_STATE_CLOSE_MOVE_HELP:
        case BATTLE_GAME_STATE_TARGET_SELECT:
            target_store_cursor_unit_name_and_data();
            break;
        case BATTLE_GAME_STATE_TARGETING_RANGE:
            target_store_cursor_unit_as_preview_target();
            break;
        }
        return 1;
    }
    return 0;
}
