#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_camera.h"
#include "fft/battle_map.h"
#include "fft/battle_menu.h"
#include "fft/battle_move.h"
#include "fft/battle_script.h"
#include "psx/types.h"

/* Advance the movement of every runtime unit (ids 0-15) that has a path by one
 * frame. */
void move_update_all_unit_paths(void) {
    s32 i;
    battle_unit_t* unit;

    for (i = 0; i < BATTLE_UNIT_RUNTIME_SLOT_COUNT; i++) {
        unit = battle_unit_get_by_runtime_id(i);
        if (unit != 0 && unit->path.count != 0) {
            move_update_path_step(unit);
        }
    }
}

/* Turns the camera when the unit's tile is blocked from the current view.
 *
 * The view quadrant is (-yaw & 0xfff) / 0x400; quadrants 0..3 test the tile's
 * north-west, south-west, south-east and north-east camera-block bits. The
 * steep-pitch nibble of camera_block_masks is used at a pitch of
 * CAMERA_PITCH_STEEP_MASK_THRESHOLD or more, the flat-pitch nibble below
 * it. When the current quadrant's bit is set, the first unblocked of a quarter
 * turn (0x400), the opposite quarter turn (-0x400) and a half turn (0x800) is
 * requested; with all four bits set the camera stays.
 *
 * Called when the acting unit starts a cast, when the target-display state shows
 * the casting unit's auto-battle target and when the action menus open for a
 * scripted turn. The `direction < 0` adjustment cannot run: the value is already
 * masked to 0..0xfff. */
void camera_rotate_when_unit_tile_not_visible(const battle_unit_t* unit) {
    s32 direction;
    u8 flags;
    map_tile_t* tile;

    tile = map_get_tile_data_pointer(unit->map_x, unit->map_y, unit->map_z);
    direction = (0 - g_camera_render_state.vy) & 0xFFF;
    if (direction < 0) {
        direction += 0x3FF;
    }
    direction >>= 0xA;
    if (g_camera_render_state.vx >= CAMERA_PITCH_STEEP_MASK_THRESHOLD) {
        flags = tile->camera_block_masks >> MAP_TILE_CAMERA_BLOCK_STEEP_SHIFT;
    } else {
        flags = tile->camera_block_masks & MAP_TILE_CAMERA_BLOCK_DIRECTION_MASK;
    }
    switch (direction) {
    case 0:
        if (flags & MAP_TILE_CAMERA_BLOCK_NORTHWEST) {
            if (!(flags & MAP_TILE_CAMERA_BLOCK_NORTHEAST)) {
                camera_request_rotation(0x400);
            } else {
                if (!(flags & MAP_TILE_CAMERA_BLOCK_SOUTHWEST)) {
                    camera_request_rotation(-0x400);
                } else if (!(flags & MAP_TILE_CAMERA_BLOCK_SOUTHEAST)) {
                    camera_request_rotation(0x800);
                }
            }
        }
        break;
    case 1:
        if (flags & MAP_TILE_CAMERA_BLOCK_SOUTHWEST) {
            if (!(flags & MAP_TILE_CAMERA_BLOCK_NORTHWEST)) {
                camera_request_rotation(0x400);
            } else {
                if (!(flags & MAP_TILE_CAMERA_BLOCK_SOUTHEAST)) {
                    camera_request_rotation(-0x400);
                } else if (!(flags & MAP_TILE_CAMERA_BLOCK_NORTHEAST)) {
                    camera_request_rotation(0x800);
                }
            }
        }
        break;
    case 2:
        if (flags & MAP_TILE_CAMERA_BLOCK_SOUTHEAST) {
            if (!(flags & MAP_TILE_CAMERA_BLOCK_SOUTHWEST)) {
                camera_request_rotation(0x400);
            } else {
                if (!(flags & MAP_TILE_CAMERA_BLOCK_NORTHEAST)) {
                    camera_request_rotation(-0x400);
                } else if (!(flags & MAP_TILE_CAMERA_BLOCK_NORTHWEST)) {
                    camera_request_rotation(0x800);
                }
            }
        }
        break;
    case 3:
        if (flags & MAP_TILE_CAMERA_BLOCK_NORTHEAST) {
            if (!(flags & MAP_TILE_CAMERA_BLOCK_SOUTHEAST)) {
                camera_request_rotation(0x400);
            } else {
                if (!(flags & MAP_TILE_CAMERA_BLOCK_NORTHWEST)) {
                    camera_request_rotation(-0x400);
                } else if (!(flags & MAP_TILE_CAMERA_BLOCK_SOUTHWEST)) {
                    camera_request_rotation(0x800);
                }
            }
        }
        break;
    }
}

/* Hand the battle over to an event script: enter the event state at animation
 * speed 2, reset the jump graphics and the event state, disable the status menu
 * and clear the casting and source unit ids (0xff). */
void action_set_casting_unit_id_ff(void) {
    g_main_system_animation_speed = 2;
    g_battle_game_state = BATTLE_GAME_STATE_EVENT;
    battle_gfx_reset_jumping_unit_graphic_triggers();
    script_reset_event_state();
    action_clear_at_list_active();
    g_battle_menu_status_enabled = 0;
    g_battle_casting_runtime_id = 0xff;
    g_battle_unit_source_runtime_id = 0xff;
}

/* Enter the event state and run the scenario check with no unit (0xff), as the
 * deployment screen does when it finishes. */
void action_set_casting_unit_id_ff_and_init(void) {
    action_set_casting_unit_id_ff();
    menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_SCENARIO_CHECK, 0, 0xff, 0, 1);
}
