/* Interpret directional input for the free cursor and show the hovered unit and action panels. */

#include "fft/battle_map.h"
#include "fft/battle_state.h"
#include "fft/camera.h"
#include "fft/cursor.h"
#include "fft/input.h"
#include "fft/option.h"
#include "fft/sound.h"
#include "fft/unit.h"
#include "fft/unit_runtime.h"
#include "fft/unit_view.h"
#include "psx/pad.h"
#include "psx/types.h"

/* The selected tile before a cursor step, in map column/layer/row order. */
typedef struct cursor_tile_position {
    s32 x;
    s32 layer;
    s32 y;
} cursor_tile_position_t;
typedef char cursor_tile_position_size_must_be_12[sizeof(cursor_tile_position_t) == 12 ? 1 : -1];

/* Free-cursor repeat counter. */
static u32 s_cursor_repeat_counter;

static cursor_tile_position_t s_cursor_previous_tile = { 0 };

/* Map the directional buttons to a map direction (battle facing, diagonals included) for the
 * camera's yaw quadrant and the cursor input mode. source picks held, newly pressed (repeating at
 * the cursor repeat speed) or released buttons; returns -1 for no direction or when a face button,
 * Select or Start was just pressed. */
s32 cursor_input_direction_get(s32 source) {
    s32 direction = -1;
    u32 input;
    u32 previous;
    u32 pressed;
    u32 divisor;
    u32 threshold;

    /* Pick the button word that source names for the current cursor input mode. */
    switch (g_cursor_input_mode) {
    case CURSOR_INPUT_OPTION_A:
    case CURSOR_INPUT_OPTION_B:
        switch (source) {
        case CAMERA_INPUT_HELD:
            input = g_input_previous_buttons;
            break;
        case CAMERA_INPUT_PRESSED:
            input = g_input_pressed_buttons;
            switch (g_game_options.fields.cursor_repeat_speed) {
            case GAME_CURSOR_REPEAT_SPEED_FAST:
                divisor = 3;
                threshold = 15;
                break;
            case GAME_CURSOR_REPEAT_SPEED_REGULAR:
                divisor = 6;
                threshold = 30;
                break;
            case GAME_CURSOR_REPEAT_SPEED_SLOW:
            default:
                divisor = 12;
                threshold = 60;
                break;
            }
            /* Buttons held past threshold frames count as pressed again every divisor frames. */
            if (threshold < g_input_stable_frames && g_input_stable_frames % divisor == 0) {
                input = g_input_previous_buttons;
            }
            break;
        case CAMERA_INPUT_RELEASED:
            input = g_input_released_buttons;
            break;
        }
        break;
    case CURSOR_INPUT_FREE_CURSOR:
        switch (source) {
        case CAMERA_INPUT_HELD:
            input = g_input_previous_buttons | g_input_previous_copy_0;
            break;
        case CAMERA_INPUT_PRESSED:
            previous = g_input_previous_buttons | g_input_previous_copy_0;
            pressed = g_input_pressed_buttons;
            input = 0;

            /* A new press counts only when it completes a diagonal with a held perpendicular button. */
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
            } else if (g_input_pressed_copy_1 & PSX_PAD_UP) {
                /* battle_input_update stores last frame's presses in both copy_0 and
                 * copy_1, so these lone-direction tests never pass: a lone direction registers only
                 * through the auto-repeat below. */
                if (!((pressed | g_input_pressed_copy_0) & PSX_PAD_UP) && (previous & PSX_PAD_UP)) {
                    input = PSX_PAD_UP;
                }
            } else if (g_input_pressed_copy_1 & PSX_PAD_DOWN) {
                if (!((pressed | g_input_pressed_copy_0) & PSX_PAD_DOWN) && (previous & PSX_PAD_DOWN)) {
                    input = PSX_PAD_DOWN;
                }
            } else if (g_input_pressed_copy_1 & PSX_PAD_LEFT) {
                if (!((pressed | g_input_pressed_copy_0) & PSX_PAD_LEFT) && (previous & PSX_PAD_LEFT)) {
                    input = PSX_PAD_LEFT;
                }
            } else if (g_input_pressed_copy_1 & PSX_PAD_RIGHT) {
                if (!((pressed | g_input_pressed_copy_0) & PSX_PAD_RIGHT) && (previous & PSX_PAD_RIGHT)) {
                    input = PSX_PAD_RIGHT;
                }
            }

            switch (g_game_options.fields.cursor_repeat_speed) {
            case GAME_CURSOR_REPEAT_SPEED_FAST:
                divisor = 6;
                threshold = 6;
                break;
            case GAME_CURSOR_REPEAT_SPEED_REGULAR:
                divisor = 10;
                threshold = 15;
                break;
            case GAME_CURSOR_REPEAT_SPEED_SLOW:
            default:
                divisor = 16;
                threshold = 30;
                break;
            }
            if (threshold < g_input_stable_frames && g_input_stable_frames % divisor == 0) {
                input = g_input_previous_buttons | g_input_previous_copy_0;
            }
            break;
        case CAMERA_INPUT_RELEASED:
            input = g_input_released_buttons;
            break;
        }
        break;
    }

    /* Rotate the buttons by the camera's yaw quadrant into a battle facing for the mode's layout. */
    switch (g_cursor_input_mode) {
    case CURSOR_INPUT_OPTION_A:
        switch (g_camera_render_state.vy & 0xC00) {
        case UNIT_FACING_SOUTH:
            if ((input & (PSX_PAD_UP | PSX_PAD_RIGHT)) == (PSX_PAD_UP | PSX_PAD_RIGHT)) {
                direction = UNIT_FACING_NORTH_EAST;
            } else if ((input & (PSX_PAD_UP | PSX_PAD_LEFT)) == (PSX_PAD_UP | PSX_PAD_LEFT)) {
                direction = UNIT_FACING_NORTH_WEST;
            } else if ((input & (PSX_PAD_RIGHT | PSX_PAD_DOWN)) == (PSX_PAD_RIGHT | PSX_PAD_DOWN)) {
                direction = UNIT_FACING_SOUTH_EAST;
            } else if ((input & (PSX_PAD_DOWN | PSX_PAD_LEFT)) == (PSX_PAD_DOWN | PSX_PAD_LEFT)) {
                direction = UNIT_FACING_SOUTH_WEST;
            } else if (input & PSX_PAD_UP) {
                direction = UNIT_FACING_NORTH;
            } else if (input & PSX_PAD_DOWN) {
                direction = UNIT_FACING_SOUTH;
            } else if (input & PSX_PAD_LEFT) {
                direction = UNIT_FACING_WEST;
            } else if (input & PSX_PAD_RIGHT) {
                direction = UNIT_FACING_EAST;
            }
            break;
        case UNIT_FACING_WEST:
            if ((input & (PSX_PAD_UP | PSX_PAD_RIGHT)) == (PSX_PAD_UP | PSX_PAD_RIGHT)) {
                direction = UNIT_FACING_NORTH_WEST;
            } else if ((input & (PSX_PAD_UP | PSX_PAD_LEFT)) == (PSX_PAD_UP | PSX_PAD_LEFT)) {
                direction = UNIT_FACING_SOUTH_WEST;
            } else if ((input & (PSX_PAD_RIGHT | PSX_PAD_DOWN)) == (PSX_PAD_RIGHT | PSX_PAD_DOWN)) {
                direction = UNIT_FACING_NORTH_EAST;
            } else if ((input & (PSX_PAD_DOWN | PSX_PAD_LEFT)) == (PSX_PAD_DOWN | PSX_PAD_LEFT)) {
                direction = UNIT_FACING_SOUTH_EAST;
            } else if (input & PSX_PAD_UP) {
                direction = UNIT_FACING_WEST;
            } else if (input & PSX_PAD_DOWN) {
                direction = UNIT_FACING_EAST;
            } else if (input & PSX_PAD_LEFT) {
                direction = UNIT_FACING_SOUTH;
            } else if (input & PSX_PAD_RIGHT) {
                direction = UNIT_FACING_NORTH;
            }
            break;
        case UNIT_FACING_NORTH:
            if ((input & (PSX_PAD_UP | PSX_PAD_RIGHT)) == (PSX_PAD_UP | PSX_PAD_RIGHT)) {
                direction = UNIT_FACING_SOUTH_WEST;
            } else if ((input & (PSX_PAD_UP | PSX_PAD_LEFT)) == (PSX_PAD_UP | PSX_PAD_LEFT)) {
                direction = UNIT_FACING_SOUTH_EAST;
            } else if ((input & (PSX_PAD_RIGHT | PSX_PAD_DOWN)) == (PSX_PAD_RIGHT | PSX_PAD_DOWN)) {
                direction = UNIT_FACING_NORTH_WEST;
            } else if ((input & (PSX_PAD_DOWN | PSX_PAD_LEFT)) == (PSX_PAD_DOWN | PSX_PAD_LEFT)) {
                direction = UNIT_FACING_NORTH_EAST;
            } else if (input & PSX_PAD_UP) {
                direction = UNIT_FACING_SOUTH;
            } else if (input & PSX_PAD_DOWN) {
                direction = UNIT_FACING_NORTH;
            } else if (input & PSX_PAD_LEFT) {
                direction = UNIT_FACING_EAST;
            } else if (input & PSX_PAD_RIGHT) {
                direction = UNIT_FACING_WEST;
            }
            break;
        case UNIT_FACING_EAST:
            if ((input & (PSX_PAD_UP | PSX_PAD_RIGHT)) == (PSX_PAD_UP | PSX_PAD_RIGHT)) {
                direction = UNIT_FACING_SOUTH_EAST;
            } else if ((input & (PSX_PAD_UP | PSX_PAD_LEFT)) == (PSX_PAD_UP | PSX_PAD_LEFT)) {
                direction = UNIT_FACING_NORTH_EAST;
            } else if ((input & (PSX_PAD_RIGHT | PSX_PAD_DOWN)) == (PSX_PAD_RIGHT | PSX_PAD_DOWN)) {
                direction = UNIT_FACING_SOUTH_WEST;
            } else if ((input & (PSX_PAD_DOWN | PSX_PAD_LEFT)) == (PSX_PAD_DOWN | PSX_PAD_LEFT)) {
                direction = UNIT_FACING_NORTH_WEST;
            } else if (input & PSX_PAD_UP) {
                direction = UNIT_FACING_EAST;
            } else if (input & PSX_PAD_DOWN) {
                direction = UNIT_FACING_WEST;
            } else if (input & PSX_PAD_LEFT) {
                direction = UNIT_FACING_NORTH;
            } else if (input & PSX_PAD_RIGHT) {
                direction = UNIT_FACING_SOUTH;
            }
            break;
        }
        break;
    case CURSOR_INPUT_OPTION_B:
        switch (g_camera_render_state.vy & 0xC00) {
        case UNIT_FACING_SOUTH:
            if ((input & (PSX_PAD_UP | PSX_PAD_RIGHT)) == (PSX_PAD_UP | PSX_PAD_RIGHT)) {
                direction = UNIT_FACING_NORTH_WEST;
            } else if ((input & (PSX_PAD_UP | PSX_PAD_LEFT)) == (PSX_PAD_UP | PSX_PAD_LEFT)) {
                direction = UNIT_FACING_SOUTH_WEST;
            } else if ((input & (PSX_PAD_RIGHT | PSX_PAD_DOWN)) == (PSX_PAD_RIGHT | PSX_PAD_DOWN)) {
                direction = UNIT_FACING_NORTH_EAST;
            } else if ((input & (PSX_PAD_DOWN | PSX_PAD_LEFT)) == (PSX_PAD_DOWN | PSX_PAD_LEFT)) {
                direction = UNIT_FACING_SOUTH_EAST;
            } else if (input & PSX_PAD_UP) {
                direction = UNIT_FACING_WEST;
            } else if (input & PSX_PAD_DOWN) {
                direction = UNIT_FACING_EAST;
            } else if (input & PSX_PAD_LEFT) {
                direction = UNIT_FACING_SOUTH;
            } else if (input & PSX_PAD_RIGHT) {
                direction = UNIT_FACING_NORTH;
            }
            break;
        case UNIT_FACING_WEST:
            if ((input & (PSX_PAD_UP | PSX_PAD_RIGHT)) == (PSX_PAD_UP | PSX_PAD_RIGHT)) {
                direction = UNIT_FACING_SOUTH_WEST;
            } else if ((input & (PSX_PAD_UP | PSX_PAD_LEFT)) == (PSX_PAD_UP | PSX_PAD_LEFT)) {
                direction = UNIT_FACING_SOUTH_EAST;
            } else if ((input & (PSX_PAD_RIGHT | PSX_PAD_DOWN)) == (PSX_PAD_RIGHT | PSX_PAD_DOWN)) {
                direction = UNIT_FACING_NORTH_WEST;
            } else if ((input & (PSX_PAD_DOWN | PSX_PAD_LEFT)) == (PSX_PAD_DOWN | PSX_PAD_LEFT)) {
                direction = UNIT_FACING_NORTH_EAST;
            } else if (input & PSX_PAD_UP) {
                direction = UNIT_FACING_SOUTH;
            } else if (input & PSX_PAD_DOWN) {
                direction = UNIT_FACING_NORTH;
            } else if (input & PSX_PAD_LEFT) {
                direction = UNIT_FACING_EAST;
            } else if (input & PSX_PAD_RIGHT) {
                direction = UNIT_FACING_WEST;
            }
            break;
        case UNIT_FACING_NORTH:
            if ((input & (PSX_PAD_UP | PSX_PAD_RIGHT)) == (PSX_PAD_UP | PSX_PAD_RIGHT)) {
                direction = UNIT_FACING_SOUTH_EAST;
            } else if ((input & (PSX_PAD_UP | PSX_PAD_LEFT)) == (PSX_PAD_UP | PSX_PAD_LEFT)) {
                direction = UNIT_FACING_NORTH_EAST;
            } else if ((input & (PSX_PAD_RIGHT | PSX_PAD_DOWN)) == (PSX_PAD_RIGHT | PSX_PAD_DOWN)) {
                direction = UNIT_FACING_SOUTH_WEST;
            } else if ((input & (PSX_PAD_DOWN | PSX_PAD_LEFT)) == (PSX_PAD_DOWN | PSX_PAD_LEFT)) {
                direction = UNIT_FACING_NORTH_WEST;
            } else if (input & PSX_PAD_UP) {
                direction = UNIT_FACING_EAST;
            } else if (input & PSX_PAD_DOWN) {
                direction = UNIT_FACING_WEST;
            } else if (input & PSX_PAD_LEFT) {
                direction = UNIT_FACING_NORTH;
            } else if (input & PSX_PAD_RIGHT) {
                direction = UNIT_FACING_SOUTH;
            }
            break;
        case UNIT_FACING_EAST:
            if ((input & (PSX_PAD_UP | PSX_PAD_RIGHT)) == (PSX_PAD_UP | PSX_PAD_RIGHT)) {
                direction = UNIT_FACING_NORTH_EAST;
            } else if ((input & (PSX_PAD_UP | PSX_PAD_LEFT)) == (PSX_PAD_UP | PSX_PAD_LEFT)) {
                direction = UNIT_FACING_NORTH_WEST;
            } else if ((input & (PSX_PAD_RIGHT | PSX_PAD_DOWN)) == (PSX_PAD_RIGHT | PSX_PAD_DOWN)) {
                direction = UNIT_FACING_SOUTH_EAST;
            } else if ((input & (PSX_PAD_DOWN | PSX_PAD_LEFT)) == (PSX_PAD_DOWN | PSX_PAD_LEFT)) {
                direction = UNIT_FACING_SOUTH_WEST;
            } else if (input & PSX_PAD_UP) {
                direction = UNIT_FACING_NORTH;
            } else if (input & PSX_PAD_DOWN) {
                direction = UNIT_FACING_SOUTH;
            } else if (input & PSX_PAD_LEFT) {
                direction = UNIT_FACING_WEST;
            } else if (input & PSX_PAD_RIGHT) {
                direction = UNIT_FACING_EAST;
            }
            break;
        }
        break;
    case CURSOR_INPUT_FREE_CURSOR:
        switch (g_camera_render_state.vy & 0xC00) {
        case UNIT_FACING_SOUTH:
            if ((input & (PSX_PAD_UP | PSX_PAD_RIGHT)) == (PSX_PAD_UP | PSX_PAD_RIGHT)) {
                direction = UNIT_FACING_NORTH;
            } else if ((input & (PSX_PAD_UP | PSX_PAD_LEFT)) == (PSX_PAD_UP | PSX_PAD_LEFT)) {
                direction = UNIT_FACING_WEST;
            } else if ((input & (PSX_PAD_RIGHT | PSX_PAD_DOWN)) == (PSX_PAD_RIGHT | PSX_PAD_DOWN)) {
                direction = UNIT_FACING_EAST;
            } else if ((input & (PSX_PAD_DOWN | PSX_PAD_LEFT)) == (PSX_PAD_DOWN | PSX_PAD_LEFT)) {
                direction = UNIT_FACING_SOUTH;
            } else if (input & PSX_PAD_UP) {
                direction = UNIT_FACING_NORTH_WEST;
            } else if (input & PSX_PAD_DOWN) {
                direction = UNIT_FACING_SOUTH_EAST;
            } else if (input & PSX_PAD_LEFT) {
                direction = UNIT_FACING_SOUTH_WEST;
            } else if (input & PSX_PAD_RIGHT) {
                direction = UNIT_FACING_NORTH_EAST;
            }
            break;
        case UNIT_FACING_WEST:
            if ((input & (PSX_PAD_UP | PSX_PAD_RIGHT)) == (PSX_PAD_UP | PSX_PAD_RIGHT)) {
                direction = UNIT_FACING_WEST;
            } else if ((input & (PSX_PAD_UP | PSX_PAD_LEFT)) == (PSX_PAD_UP | PSX_PAD_LEFT)) {
                direction = UNIT_FACING_SOUTH;
            } else if ((input & (PSX_PAD_RIGHT | PSX_PAD_DOWN)) == (PSX_PAD_RIGHT | PSX_PAD_DOWN)) {
                direction = UNIT_FACING_NORTH;
            } else if ((input & (PSX_PAD_DOWN | PSX_PAD_LEFT)) == (PSX_PAD_DOWN | PSX_PAD_LEFT)) {
                direction = UNIT_FACING_EAST;
            } else if (input & PSX_PAD_UP) {
                direction = UNIT_FACING_SOUTH_WEST;
            } else if (input & PSX_PAD_DOWN) {
                direction = UNIT_FACING_NORTH_EAST;
            } else if (input & PSX_PAD_LEFT) {
                direction = UNIT_FACING_SOUTH_EAST;
            } else if (input & PSX_PAD_RIGHT) {
                direction = UNIT_FACING_NORTH_WEST;
            }
            break;
        case UNIT_FACING_NORTH:
            if ((input & (PSX_PAD_UP | PSX_PAD_RIGHT)) == (PSX_PAD_UP | PSX_PAD_RIGHT)) {
                direction = UNIT_FACING_SOUTH;
            } else if ((input & (PSX_PAD_UP | PSX_PAD_LEFT)) == (PSX_PAD_UP | PSX_PAD_LEFT)) {
                direction = UNIT_FACING_EAST;
            } else if ((input & (PSX_PAD_RIGHT | PSX_PAD_DOWN)) == (PSX_PAD_RIGHT | PSX_PAD_DOWN)) {
                direction = UNIT_FACING_WEST;
            } else if ((input & (PSX_PAD_DOWN | PSX_PAD_LEFT)) == (PSX_PAD_DOWN | PSX_PAD_LEFT)) {
                direction = UNIT_FACING_NORTH;
            } else if (input & PSX_PAD_UP) {
                direction = UNIT_FACING_SOUTH_EAST;
            } else if (input & PSX_PAD_DOWN) {
                direction = UNIT_FACING_NORTH_WEST;
            } else if (input & PSX_PAD_LEFT) {
                direction = UNIT_FACING_NORTH_EAST;
            } else if (input & PSX_PAD_RIGHT) {
                direction = UNIT_FACING_SOUTH_WEST;
            }
            break;
        case UNIT_FACING_EAST:
            if ((input & (PSX_PAD_UP | PSX_PAD_RIGHT)) == (PSX_PAD_UP | PSX_PAD_RIGHT)) {
                direction = UNIT_FACING_EAST;
            } else if ((input & (PSX_PAD_UP | PSX_PAD_LEFT)) == (PSX_PAD_UP | PSX_PAD_LEFT)) {
                direction = UNIT_FACING_NORTH;
            } else if ((input & (PSX_PAD_RIGHT | PSX_PAD_DOWN)) == (PSX_PAD_RIGHT | PSX_PAD_DOWN)) {
                direction = UNIT_FACING_SOUTH;
            } else if ((input & (PSX_PAD_DOWN | PSX_PAD_LEFT)) == (PSX_PAD_DOWN | PSX_PAD_LEFT)) {
                direction = UNIT_FACING_WEST;
            } else if (input & PSX_PAD_UP) {
                direction = UNIT_FACING_NORTH_EAST;
            } else if (input & PSX_PAD_DOWN) {
                direction = UNIT_FACING_SOUTH_WEST;
            } else if (input & PSX_PAD_LEFT) {
                direction = UNIT_FACING_NORTH_WEST;
            } else if (input & PSX_PAD_RIGHT) {
                direction = UNIT_FACING_SOUTH_EAST;
            }
            break;
        }
        break;
    }

    if (g_input_pressed_buttons
        & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE | PSX_PAD_SELECT | PSX_PAD_START)) {
        direction = -1;
    }
    return direction;
}

/* Start the hovered-unit stats panel for the unit on the cursor tile, or the tile
 * info panel when the tile is empty. */
void cursor_show_hovered_unit_panel(void) {
    unit_runtime_t* unit = unit_find_selectable_at_tile(g_cursor_x, g_cursor_y, g_cursor_layer);
    s32 mode;
    s32 unit_id;

    if (unit != 0) {
        mode = UNIT_VIEW_MODE_HOVERED_UNIT;
        unit_id = unit->stats->battle_id;
    } else {
        mode = UNIT_VIEW_MODE_TILE_INFO;
        unit_id = 0xFF;
    }
    unit_view_start(mode, unit_id, 0xFF);
}

/* Start the right-side stats panel for the unit on the cursor tile, or the tile
 * info panel when the tile is empty. */
void cursor_show_right_unit_panel(void) {
    unit_runtime_t* unit = unit_find_selectable_at_tile(g_cursor_x, g_cursor_y, g_cursor_layer);

    /* Retail looks up the source unit here and ignores it. */
    unit_source_get();
    if (unit != 0) {
        unit_view_start(UNIT_VIEW_MODE_RIGHT_UNIT, 0xFF, unit->stats->battle_id);
    } else {
        unit_view_start(UNIT_VIEW_MODE_TILE_INFO, 0xFF, 0xFF);
    }
}

/* Start the unit panels for a selected target tile: the attack preview for the
 * source unit against the unit on the cursor tile, or the source unit's own stats
 * when the tile is empty. */
void cursor_show_attack_preview_panel(void) {
    unit_runtime_t* target = unit_find_selectable_at_tile(g_cursor_x, g_cursor_y, g_cursor_layer);
    unit_runtime_t* source = unit_source_get();

    if (target != 0) {
        unit_view_start(UNIT_VIEW_MODE_ATTACK_PREVIEW, source->stats->battle_id, target->stats->battle_id);
    } else {
        unit_view_start(UNIT_VIEW_MODE_HOVERED_UNIT, source->stats->battle_id, 0xFF);
    }
}

/* Move the cursor while a target is being chosen: onto selected_unit when one is
 * given, otherwise to the tile stored in the unit's cursor command state.
 *
 * When the cursor tile changes, the unit panels are refreshed for the current
 * game state: the cursor unit's panel in the free-cursor, select-move-tile and
 * target-select states, the preview target's panel in the targeting-range state. */
void cursor_move_to_selection(const unit_runtime_t* unit, const unit_runtime_t* selected_unit) {
    s32 old_x = g_cursor_x;
    s32 old_layer = g_cursor_layer;
    s32 old_y = g_cursor_y;

    if (selected_unit != 0) {
        cursor_move_to_unit(selected_unit);
    } else {
        g_cursor_x = unit->command_state.cursor.target_panel.vx;
        g_cursor_layer = unit->command_state.cursor.target_panel.vy;
        g_cursor_y = unit->command_state.cursor.target_panel.vz;
    }
    if (((g_cursor_x != old_x) | (g_cursor_layer != old_layer) | (g_cursor_y != old_y)) != 0) {
        switch (g_battle_state) {
        case BATTLE_STATE_FREE_CURSOR:
        case BATTLE_STATE_SELECT_MOVE_TILE:
        case BATTLE_STATE_TARGET_SELECT:
            cursor_show_hovered_unit_panel();
            break;
        case BATTLE_STATE_TARGETING_RANGE:
            cursor_show_right_unit_panel();
            break;
        }
    }
}

/* Moves the free cursor one step from the pad direction and reports whether
 * it moved.
 *
 * Before reading the direction, the cursor may hop to the other map level:
 * on START when the multi-height cursor option is STOP, otherwise once the
 * cursor has rested longer than the option's delay (30, 60 or 90 frames). A
 * step onto a blocked tile whose other level is also blocked is undone;
 * otherwise the cursor takes the unblocked level, or the one nearer the old
 * tile's height.
 *
 * `direction` doubles as the level-hop flag, as the target keeps both in one
 * register. The do-while(0) weights the delay's references so that GCC
 * allocates it before `direction`; without it the two swap s0 and s1. The
 * 32-bit option value is copied first so the loop note does not stop the load from
 * being scheduled into the prologue. */
s32 cursor_move_by_input(void) {
    game_options_t options = g_game_options;
    u32 delay;
    s32 direction;
    battle_map_tile_t* current;
    battle_map_tile_t* stepped;
    battle_map_tile_t* opposite;
    battle_map_tile_t* other_level;
    s8 stepped_rise;
    s8 opposite_rise;

    do {
        switch (options.fields.multi_height_cursor_speed) {
        case GAME_MULTI_HEIGHT_CURSOR_SPEED_FAST:
            delay = 30;
            break;
        case GAME_MULTI_HEIGHT_CURSOR_SPEED_REGULAR:
            delay = 60;
            break;
        case GAME_MULTI_HEIGHT_CURSOR_SPEED_SLOW:
        default:
            delay = 90;
            break;
        }
    } while (0);

    direction = 0;
    current = battle_map_tile_get_pointer(g_cursor_x, g_cursor_y, g_cursor_layer);
    if (g_game_options.fields.multi_height_cursor_speed == GAME_MULTI_HEIGHT_CURSOR_SPEED_STOP) {
        if (g_input_pressed_buttons & PSX_PAD_START) {
            other_level = battle_map_tile_get_pointer(g_cursor_x, g_cursor_y, g_cursor_layer ^ 1);
            if (!other_level->flags.blocked) {
                direction = 1;
                g_cursor_layer ^= 1;
            }
        }
    } else if (s_cursor_repeat_counter > delay) {
        s_cursor_repeat_counter = 0;
        other_level = battle_map_tile_get_pointer(g_cursor_x, g_cursor_y, g_cursor_layer ^ 1);
        if (!other_level->flags.blocked) {
            direction = 1;
            g_cursor_layer ^= 1;
        }
    }

    if (direction == 0) {
        s_cursor_repeat_counter++;
        s_cursor_previous_tile.x = g_cursor_x;
        s_cursor_previous_tile.layer = g_cursor_layer;
        s_cursor_previous_tile.y = g_cursor_y;

        direction = cursor_input_direction_get(CAMERA_INPUT_PRESSED);
        switch (direction) {
        case UNIT_FACING_NORTH:
            g_cursor_y++;
            break;
        case UNIT_FACING_SOUTH:
            g_cursor_y--;
            break;
        case UNIT_FACING_WEST:
            g_cursor_x--;
            break;
        case UNIT_FACING_EAST:
            g_cursor_x++;
            break;
        case UNIT_FACING_NORTH_EAST:
            g_cursor_y++;
            g_cursor_x++;
            break;
        case UNIT_FACING_NORTH_WEST:
            g_cursor_y++;
            g_cursor_x--;
            break;
        case UNIT_FACING_SOUTH_EAST:
            g_cursor_x++;
            g_cursor_y--;
            break;
        case UNIT_FACING_SOUTH_WEST:
            g_cursor_y--;
            g_cursor_x--;
            break;
        }

        /* Keep the cursor on the map: a step past the edge goes back, and a step below 0
         * compares as a large unsigned value and resets to 0. */
        if (g_cursor_x == g_battle_map_terrain_width) {
            g_cursor_x--;
        } else if ((u32)g_cursor_x > g_battle_map_terrain_width) {
            g_cursor_x = 0;
        }
        if (g_cursor_y == g_battle_map_terrain_depth) {
            g_cursor_y = g_battle_map_terrain_depth - 1;
        }
        if ((u32)g_cursor_y > g_battle_map_terrain_depth) {
            g_cursor_y = 0;
        }

        /* The tile stepped onto at the cursor's level and the tile on the other level. */
        stepped = battle_map_tile_get_pointer(g_cursor_x, g_cursor_y, g_cursor_layer);
        opposite = battle_map_tile_get_pointer(g_cursor_x, g_cursor_y, g_cursor_layer ^ 1);
        if (stepped->flags.blocked) {
            if (opposite->flags.blocked) {
                switch (direction) {
                case UNIT_FACING_NORTH:
                    g_cursor_y--;
                    break;
                case UNIT_FACING_SOUTH:
                    g_cursor_y++;
                    break;
                case UNIT_FACING_WEST:
                    g_cursor_x++;
                    break;
                case UNIT_FACING_EAST:
                    g_cursor_x--;
                    break;
                case UNIT_FACING_NORTH_EAST:
                    g_cursor_y--;
                    g_cursor_x--;
                    break;
                case UNIT_FACING_NORTH_WEST:
                    g_cursor_y--;
                    g_cursor_x++;
                    break;
                case UNIT_FACING_SOUTH_EAST:
                    g_cursor_x--;
                    g_cursor_y++;
                    break;
                case UNIT_FACING_SOUTH_WEST:
                    g_cursor_y++;
                    g_cursor_x++;
                    break;
                }
            } else {
                g_cursor_layer ^= 1;
            }
        } else if (!opposite->flags.blocked) {
            stepped_rise = stepped->height - current->height;
            opposite_rise = opposite->height - current->height;
            if (stepped_rise < 0) {
                stepped_rise = -stepped_rise;
            }
            if (opposite_rise < 0) {
                opposite_rise = -opposite_rise;
            }
            if (opposite_rise < stepped_rise) {
                g_cursor_layer ^= 1;
            }
        }
    }

    if (((g_cursor_x != s_cursor_previous_tile.x) | (g_cursor_layer != s_cursor_previous_tile.layer)
            | (g_cursor_y != s_cursor_previous_tile.y))
        != 0) {
        sound_sfx_play(SOUND_SFX_CURSOR_MOVE);
        s_cursor_repeat_counter = 0;
        switch (g_battle_state) {
        case BATTLE_STATE_FREE_CURSOR:
        case BATTLE_STATE_SELECT_MOVE_TILE:
        case BATTLE_STATE_TARGET_SELECT:
            cursor_show_hovered_unit_panel();
            break;
        case BATTLE_STATE_TARGETING_RANGE:
            cursor_show_right_unit_panel();
            break;
        }
        return 1;
    }
    return 0;
}
