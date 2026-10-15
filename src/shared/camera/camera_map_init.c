#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_camera.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "psx/types.h"

/* Places the camera for a map's GNS camera-position record.
 *
 * The position is the negated coords in 20.12 and the angles are masked to
 * 0x1fff. The cursor goes to the tile under the position (0x1c000 per tile) on
 * layer 0, and the camera height to that tile's height, 0xc000 (12 world units)
 * per level, negative upwards. */
void camera_set_focus_and_cursor_tile(const SVECTOR* coords, const SVECTOR* rotation) {
    s32 tile_x;
    s32 tile_y;

    main_util_set_vector(&g_camera_current_real_coords, -coords->vx * ONE, -coords->vy * ONE, -coords->vz * ONE);
    main_util_set_svector(&g_camera_render_state, rotation->vx & 0x1fff, rotation->vy & 0x1fff, rotation->vz & 0x1fff);
    g_battle_cursor_z = 0;
    tile_x = g_camera_current_real_coords.vx / (MAP_TILE_SIZE * ONE);
    g_battle_cursor_x = tile_x;
    tile_y = g_camera_current_real_coords.vz / (MAP_TILE_SIZE * ONE);
    g_battle_cursor_y = tile_y;
    g_camera_current_real_coords.vy = -map_get_tile_data_pointer(tile_x, tile_y, 0)->height * (MAP_HEIGHT_STEP * ONE);
}

/* Resets the camera for a new map: screen offset (0x100, 0xa0, 0x280), zoom ONE,
 * pitch 0x12e (the low tilt), yaw 0xe00, position at the origin, cursor on tile
 * (0, 0), no acting or casting unit, no rotation, tilt or zoom in progress and
 * directional panning off. */
void camera_init_defaults(void) {
    main_util_set_vector(&g_camera_current_screen_offset, 0x100, 0xA0, 0x280);
    main_util_set_vector(&g_camera_zoom, ONE, ONE, ONE);
    main_util_set_svector(&g_camera_render_state, 0x12E, 0xE00, 0);
    main_util_set_vector(&g_camera_current_real_coords, 0, 0, 0);

    g_battle_casting_runtime_id = 0xFF;
    g_battle_unit_source_runtime_id = 0xFF;
    g_battle_cursor_y = 0;
    g_battle_cursor_z = 0;
    g_battle_cursor_x = 0;
    g_camera_rotation_action = CAMERA_ROTATION_NONE;
    g_camera_zoom_action = CAMERA_STEP_NONE;
    g_map_zoom_target = MAP_ZOOM_IN;
    g_camera_tilt_action = CAMERA_STEP_NONE;
    g_map_tilt_target = MAP_TILT_LOW;
    g_camera_map_pan_enabled = 0;
}

/* Appends one unit to the graphics load queue that
 * battle_gfx_step_queued_unit_graphics_load works through.
 *
 * A spritesheet id of BATTLE_SPRITESHEET_ID_COUNT or more is replaced by
 * Ramza's chapter 1 sheet. The count stops at 16, the size of the
 * queue, so a seventeenth call writes the record one past its end. */
void battle_gfx_append_unit_graphics_load_descriptor(s32 map_x, s32 map_y, s32 map_level, s16 facing,
    s16 spritesheet_id, s16 palette_id, s16 runtime_id, unit_stats_t* unit, u32 flags) {
    g_main_gfx_unit_graphics_load_queue.descriptors[g_main_gfx_unit_graphics_load_queue.count].map_x = map_x;
    g_main_gfx_unit_graphics_load_queue.descriptors[g_main_gfx_unit_graphics_load_queue.count].map_y = map_y;
    g_main_gfx_unit_graphics_load_queue.descriptors[g_main_gfx_unit_graphics_load_queue.count].map_level = map_level;
    g_main_gfx_unit_graphics_load_queue.descriptors[g_main_gfx_unit_graphics_load_queue.count].facing = facing;
    g_main_gfx_unit_graphics_load_queue.descriptors[g_main_gfx_unit_graphics_load_queue.count].spritesheet_id
        = spritesheet_id;
    if (spritesheet_id >= BATTLE_SPRITESHEET_ID_COUNT) {
        g_main_gfx_unit_graphics_load_queue.descriptors[g_main_gfx_unit_graphics_load_queue.count].spritesheet_id
            = BATTLE_SPRITESHEET_ID_RAMZA_CHAPTER_1;
    }
    g_main_gfx_unit_graphics_load_queue.descriptors[g_main_gfx_unit_graphics_load_queue.count].palette_id = palette_id;
    g_main_gfx_unit_graphics_load_queue.descriptors[g_main_gfx_unit_graphics_load_queue.count].runtime_id = runtime_id;
    g_main_gfx_unit_graphics_load_queue.descriptors[g_main_gfx_unit_graphics_load_queue.count].stats = unit;
    g_main_gfx_unit_graphics_load_queue.descriptors[g_main_gfx_unit_graphics_load_queue.count].flags = flags;
    if (g_main_gfx_unit_graphics_load_queue.count < BATTLE_UNIT_RUNTIME_SLOT_COUNT) {
        g_main_gfx_unit_graphics_load_queue.count++;
    }
}

/* Always 0. No code calls it. */
s32 battle_return_zero_8008e6d0(void) {
    return 0;
}
