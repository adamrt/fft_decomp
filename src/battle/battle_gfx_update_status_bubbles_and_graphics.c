#include "fft/battle.h"
#include "psx/types.h"

/*
 * Per-frame status-bubble and unit-graphics update.
 *
 * In the listed game states, bubbles are held off for 0x3c frames whenever the
 * camera zooms, tilts, rotates or moves, and processed for every Misc unit_t
 * Data record once the countdown has run out. Every state then reloads the
 * casting unit's graphics and runs battle_gfx_draw_unit_sprite_layers on every record.
 */
void battle_gfx_update_status_bubbles_and_graphics(void) {
    battle_unit_misc_data_t* unit;

    switch (g_battle_game_state) {
    case BATTLE_GAME_STATE_FREE_CURSOR:
    case BATTLE_GAME_STATE_HIGHLIGHT_UNITS:
    case BATTLE_GAME_STATE_OPEN_ACTION_MENUS:
    case BATTLE_GAME_STATE_IDLING_ACTION_MENUS:
    case BATTLE_GAME_STATE_MENU_TO_TARGETING:
    case BATTLE_GAME_STATE_DISPLAY_MOVE_AREA:
    case BATTLE_GAME_STATE_UNIT_MOVE:
    case BATTLE_GAME_STATE_CLOSE_MOVE_HELP:
    case BATTLE_GAME_STATE_ACTION_EXECUTE_SETUP:
    case BATTLE_GAME_STATE_TARGETING_RANGE:
    case BATTLE_GAME_STATE_ABILITY_PREVIEW_HANDLING:
    case BATTLE_GAME_STATE_CONFIRM_ACTION:
        if (g_battle_camera_zoom_action == 0 && g_battle_camera_tilt_action == 0 && g_battle_camera_rotation_action == 0
            && !(g_battle_current_vector.vx | g_battle_current_vector.vy | g_battle_current_vector.vz)) {
            if (g_battle_gfx_status_bubble_delay != 0) {
                g_battle_gfx_status_bubble_delay -= g_animation_speed;
                if (g_battle_gfx_status_bubble_delay <= 0) {
                    g_battle_gfx_status_bubble_delay = 0;
                }
            } else {
                for (unit = g_battle_unit_last_misc_data; unit != 0; unit = unit->previous) {
                    battle_gfx_update_status_bubble_display(unit);
                }
            }
        } else {
            g_battle_gfx_status_bubble_delay = 0x3c;
        }
        break;
    }
    battle_gfx_load_casting_unit_graphics();
    for (unit = g_battle_unit_last_misc_data; unit != 0; unit = unit->previous) {
        battle_gfx_draw_unit_sprite_layers(unit);
    }
}
