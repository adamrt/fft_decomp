#ifndef BATTLE_MAP_H
#define BATTLE_MAP_H

/* BATTLE map terrain and tile lookups, move-find items and traps, and the map data commands. */

#include "fft/battle_move.h"
#include "fft/map.h"
#include "fft/unit.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* terrain */
extern s32 g_battle_map_id;
extern u8 g_battle_map_max_x;
extern u8 g_battle_map_max_y;
/* Battle terrain: two layers of 256 records, indexed
 * (layer * 256 + y * width + x). Callers that walk it with raw byte offsets
 * cast to u8*; the record shape is map_tile_t. */
extern map_tile_t g_battle_map_tile_data[MAP_TILE_COUNT];
extern s32 g_battle_map_lowest_surface_y;
extern map_tile_t* g_battle_map_tile_data_ptr;
extern u8 g_battle_map_tile_depth;
extern u8 g_battle_map_tile_width;

s32 battle_map_calculate_location(const unit_stats_t* unit);
s32 battle_map_get_tile_data_value(map_tile_data_selector_e field, s32 x, s32 y, s32 layer);
map_tile_t* battle_map_get_tile_data_pointer(s32 x, s32 y, u32 layer);
s32 battle_map_set_tile_data_value(map_tile_data_selector_e field, s32 x, s32 y, s32 layer, s32 value);
map_tile_t* battle_map_get_unit_tile_by_runtime_id(u32 runtime_id);
void battle_map_store_selected_tile_coordinates(SVECTOR* destination);
s32 battle_map_calculate_slope_height(const SVECTOR* coords, s32 map_z);
s32 battle_map_resolve_rotated_offset_layer(unit_stats_t*, s32, s32, s32, s32*, s32*, s32*);
void battle_map_store_max_coordinates(SVECTOR* destination);
/* A typed terrain parameter loses the first retail byte-cursor increment. */
void battle_map_copy_xy_coords_and_tile_data(u8* data);
map_tile_t* battle_map_get_tile_data_ptr_from_battle_id(u32 battle_id);
map_tile_t* battle_map_get_unit_tile_from_world_coords(u32 runtime_id);

/* move-find items and traps */
/* Four serialized move-find item/trap entries per map. */
typedef enum map_move_find_item_entry_flags {
    MAP_MOVE_FIND_ENTRY_TRAP_DEGENERATOR = 0x01,
    MAP_MOVE_FIND_ENTRY_TRAP_DEATHTRAP = 0x02,
    MAP_MOVE_FIND_ENTRY_TRAP_SLEEPING_GAS = 0x04,
    MAP_MOVE_FIND_ENTRY_TRAP_STEEL_NEEDLE = 0x08,
    MAP_MOVE_FIND_ENTRY_NO_TRAP = 0x10,
    MAP_MOVE_FIND_ENTRY_NO_ITEM = 0x20,
} map_move_find_item_entry_flags_e;

typedef struct map_move_find_item_entry {
    u8 y : 4;
    u8 x : 4;
    u8 flags : 7; /* map_move_find_item_entry_flags_e */
    u8 elevation : 1;
    u8 rare_item_id;
    u8 common_item_id;
} map_move_find_item_entry_t;
typedef char map_move_find_item_entry_size_must_be_4[(sizeof(map_move_find_item_entry_t) == 4) ? 1 : -1];

typedef struct map_move_find_item_data {
    map_move_find_item_entry_t entries[4];
} map_move_find_item_data_t;
typedef char map_move_find_item_data_size_must_be_16[(sizeof(map_move_find_item_data_t) == 16) ? 1 : -1];

extern map_move_find_item_data_t g_battle_map_move_find_item_data[MAP_ID_COUNT];
/* The current map's entries, copied from g_battle_map_move_find_item_data when the map loads. */
extern map_move_find_item_data_t g_battle_current_map_move_find_item_data;
extern move_found_item_t g_battle_map_rare_item_result;

void battle_map_load_move_find_item_data(map_move_find_item_data_t* map_data);
move_found_item_t* battle_map_determine_rare_common_item(unit_stats_t* unit);
move_find_result_data_t* battle_map_get_move_find_result(s32 x, u8 y, u8 layer);

/* map data commands */
/* Commands of battle_map_dispatch_map_data_command, which switches on command
 * - 0x6a. This is its own id space: event Use3DObject (0x54) reaches command
 * 0x80. Names come from the case bodies. */
typedef enum map_data_command {
    MAP_DATA_COMMAND_DISABLE_TEXTURE_ANIMATION = 0x6a,
    MAP_DATA_COMMAND_RESTORE_TEXTURE_ANIMATION = 0x6b,
    MAP_DATA_COMMAND_DISABLE_ALL_TEXTURE_ANIMATIONS = 0x71,
    MAP_DATA_COMMAND_RESTORE_ALL_TEXTURE_ANIMATIONS = 0x72,
    MAP_DATA_COMMAND_DISABLE_OVERLAY_DITHER = 0x7a, /* SetDrawMode dtd 0 */
    MAP_DATA_COMMAND_ENABLE_OVERLAY_DITHER = 0x7b,  /* SetDrawMode dtd 1 */
    MAP_DATA_COMMAND_SET_3D_OBJECT_STATE = 0x80,
    MAP_DATA_COMMAND_GET_3D_OBJECT_STATE = 0x81,
    MAP_DATA_COMMAND_GET_TEXTURE_ANIMATION_ACTIVE = 0x82,
    MAP_DATA_COMMAND_START_TEXTURE_ANIMATION = 0x83,
    MAP_DATA_COMMAND_SPLIT_MESH_PARTS = 0x98,
    MAP_DATA_COMMAND_RELEASE_GNS_HOLD = 0x9a,
    MAP_DATA_COMMAND_SET_TEXTURE_ANIMATION_DURATION = 0x9d,
} map_data_command_e;

s32 battle_map_dispatch_map_data_command(map_data_command_e command, u32 index, s32 object_state, s32 flag);
s32 battle_map_command_get_3d_object_state(s32 value);
s32 battle_map_command_set_3d_object_state(s32 value_a, s32 value_b);
s32 battle_map_command_get_texture_animation_active(s32 value);
s32 battle_map_command_start_texture_animation(s32 value_a, s32 value_b);

#endif
