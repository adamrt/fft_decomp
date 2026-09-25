#include "fft/battle.h"
#include "psx/types.h"

/* Resolve one effect target to renderer-space coordinates.
 *
 * A unit target copies the unit's screen coordinate record, and when
 * add_depth is set lowers Y to the tile's own surface height. A tile target
 * projects the tile centre (x * 28 + 14, y * 28 + 14) and takes its height
 * from the tile record, adding the slope depth when add_depth is set. */
void battle_effect_resolve_target_render_coords(
    s32 add_depth, s32 target_index, battle_effect_coord_data_t* coord_data, SVECTOR* out) {
    map_tile_t* tile;

    if (target_index == -1) {
        if (coord_data->targets[16].target_type == 0) {
            *out = *(SVECTOR*)battle_unit_get_screen_data_ptr_by_misc_id(coord_data->targets[16].id.misc_id);
            if (add_depth != 0) {
                s32 height;
                tile = battle_map_get_tile_data_ptr_from_misc_id(coord_data->targets[16].id.misc_id);
                height = -((tile->height + (tile->depth_half_height >> MAP_TILE_DEPTH_SHIFT)) * 12);
                if (height < out->vy) {
                    out->vy = height;
                }
            }
        } else {
            tile = battle_map_get_tile_data_pointer(
                coord_data->targets[16].id.tile_x, coord_data->targets[16].tile_y, coord_data->targets[16].map_z);
            out->vx = coord_data->targets[16].id.tile_x * 28 + 14;
            out->vz = coord_data->targets[16].tile_y * 28 + 14;
            if (add_depth != 0) {
                out->vy = -((tile->height + (tile->depth_half_height >> MAP_TILE_DEPTH_SHIFT)) * 12);
            } else {
                out->vy = -(tile->height * 12);
            }
        }
    } else {
        if (coord_data->targets[target_index].target_type == 0) {
            *out = *(SVECTOR*)battle_unit_get_screen_data_ptr_by_misc_id(coord_data->targets[target_index].id.misc_id);
            if (add_depth != 0) {
                s32 height;
                tile = battle_map_get_tile_data_ptr_from_misc_id(coord_data->targets[target_index].id.misc_id);
                height = -((tile->height + (tile->depth_half_height >> MAP_TILE_DEPTH_SHIFT)) * 12);
                if (height < out->vy) {
                    out->vy = height;
                }
            }
        } else {
            tile = battle_map_get_tile_data_pointer(coord_data->targets[target_index].id.tile_x,
                coord_data->targets[target_index].tile_y, coord_data->targets[target_index].map_z);
            out->vx = coord_data->targets[target_index].id.tile_x * 28 + 14;
            out->vz = coord_data->targets[target_index].tile_y * 28 + 14;
            if (add_depth != 0) {
                out->vy = -((tile->height + (tile->depth_half_height >> MAP_TILE_DEPTH_SHIFT)) * 12);
            } else {
                out->vy = -(tile->height * 12);
            }
        }
    }
}
