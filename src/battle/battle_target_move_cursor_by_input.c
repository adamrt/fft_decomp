#include "fft/battle.h"
#include "fft/battle_state.h"
#include "fft/main_runtime.h"
#include "fft/main_sound.h"
#include "fft/map.h"
#include "psx/pad.h"

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
 * option word is copied first so the loop note does not stop the load from
 * being scheduled into the prologue. */
s32 battle_target_move_cursor_by_input(void) {
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
        switch ((s32)options.fields.multi_height_cursor_speed) {
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
    current = battle_map_get_tile_data_pointer(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z);
    if (g_main_game_options.fields.multi_height_cursor_speed == 3) {
        if (g_controller_input_pressed & PSX_PAD_START) {
            if (!battle_map_get_tile_data_pointer(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z ^ 1)
                    ->flags_06.bits.blocked) {
                direction = 1;
                g_battle_cursor_z ^= 1;
            }
        }
    } else if (g_battle_cursor_repeat_counter > delay) {
        g_battle_cursor_repeat_counter = 0;
        if (!battle_map_get_tile_data_pointer(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z ^ 1)
                ->flags_06.bits.blocked) {
            direction = 1;
            g_battle_cursor_z ^= 1;
        }
    }
    if (direction == 0) {
        g_battle_cursor_repeat_counter++;
        g_battle_cursor_previous_x = g_battle_cursor_x;
        g_battle_cursor_previous_z = g_battle_cursor_z;
        g_battle_cursor_previous_y = g_battle_cursor_y;
        /* The target passes only the mode; the definition's second parameter is a matching device. */
        direction = ((s32 (*)(s32))battle_camera_get_input_direction)(1);
        switch (direction) {
        case 0x800:
            g_battle_cursor_y++;
            break;
        case 0:
            g_battle_cursor_y--;
            break;
        case 0x400:
            g_battle_cursor_x--;
            break;
        case 0xc00:
            g_battle_cursor_x++;
            break;
        case 0xa00:
            g_battle_cursor_y++;
            g_battle_cursor_x++;
            break;
        case 0x600:
            g_battle_cursor_y++;
            g_battle_cursor_x--;
            break;
        case 0xe00:
            g_battle_cursor_x++;
            g_battle_cursor_y--;
            break;
        case 0x200:
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
        lower = battle_map_get_tile_data_pointer(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z);
        upper = battle_map_get_tile_data_pointer(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z ^ 1);
        if (lower->flags_06.bits.blocked) {
            if (upper->flags_06.bits.blocked) {
                switch (direction) {
                case 0x800:
                    g_battle_cursor_y--;
                    break;
                case 0:
                    g_battle_cursor_y++;
                    break;
                case 0x400:
                    g_battle_cursor_x++;
                    break;
                case 0xc00:
                    g_battle_cursor_x--;
                    break;
                case 0xa00:
                    g_battle_cursor_y--;
                    g_battle_cursor_x--;
                    break;
                case 0x600:
                    g_battle_cursor_y--;
                    g_battle_cursor_x++;
                    break;
                case 0xe00:
                    g_battle_cursor_x--;
                    g_battle_cursor_y++;
                    break;
                case 0x200:
                    g_battle_cursor_y++;
                    g_battle_cursor_x++;
                    break;
                }
            } else {
                g_battle_cursor_z ^= 1;
            }
        } else if (!upper->flags_06.bits.blocked) {
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
            battle_target_store_cursor_unit_name_and_data();
            break;
        case BATTLE_GAME_STATE_TARGETING_RANGE:
            battle_target_store_cursor_unit_as_preview_target();
            break;
        }
        return 1;
    }
    return 0;
}
