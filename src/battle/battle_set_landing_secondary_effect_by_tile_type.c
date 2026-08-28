#include "fft/battle.h"
#include "psx/types.h"

void battle_set_landing_secondary_effect_by_tile_type(battle_unit_misc_data_t* unit) {
    map_tile_t* tile;
    s32 surface;

    if (unit->movement.word & BATTLE_MOTION_FLAG_SUPPRESS_SFX_AND_LANDING_EFFECTS)
        return;
    tile = battle_map_get_tile_data_pointer(
        unit->movement.bytes.destination_x, unit->movement.bytes.destination_y, unit->movement.bytes.destination_z);
    if (tile == 0)
        return;
    surface = tile->surface.value & MAP_SURFACE_MASK;
    switch (surface) {
    case MAP_SURFACE_SAND:
    case MAP_SURFACE_STALACTITE:
    case MAP_SURFACE_ROCKY_CLIFF:
    case MAP_SURFACE_GRAVEL:
    case MAP_SURFACE_WASTELAND:
    case MAP_SURFACE_LAVA_ROCKS:
    case MAP_SURFACE_ICE:
    case MAP_SURFACE_ROAD:
    case MAP_SURFACE_WOODEN_FLOOR:
    case MAP_SURFACE_STONE_FLOOR:
    case MAP_SURFACE_ROOF:
    case MAP_SURFACE_STONE_WALL:
    case MAP_SURFACE_SALT:
    case MAP_SURFACE_BOOK:
    case MAP_SURFACE_TREE:
    case MAP_SURFACE_BOX:
    case MAP_SURFACE_BRICK:
    case MAP_SURFACE_CHIMNEY:
    case MAP_SURFACE_MUD_WALL:
    case MAP_SURFACE_BRIDGE:
    case MAP_SURFACE_STAIRS:
    case MAP_SURFACE_DECK:
    case MAP_SURFACE_MACHINE:
    case MAP_SURFACE_IRON_PLATE:
    case MAP_SURFACE_TOMBSTONE:
    case MAP_SURFACE_COFFIN:
        battle_effect_set_secondary_fall_dust(unit);
        break;
    case MAP_SURFACE_SWAMP:
    case MAP_SURFACE_MARSH:
    case MAP_SURFACE_POISONED_MARSH:
    case MAP_SURFACE_WATERWAY:
    case MAP_SURFACE_RIVER:
    case MAP_SURFACE_LAKE:
    case MAP_SURFACE_SEA:
    case MAP_SURFACE_WATERFALL:
        battle_effect_set_secondary_splash(unit);
        break;
    }
}
