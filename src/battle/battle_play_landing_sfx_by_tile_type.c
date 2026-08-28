#include "fft/battle.h"
#include "psx/types.h"

void battle_play_landing_sfx_by_tile_type(battle_unit_misc_data_t* unit) {
    map_tile_t* tile;
    s32 surface;
    s32 sfx;

    if (unit->movement.word & BATTLE_MOTION_FLAG_SUPPRESS_SFX_AND_LANDING_EFFECTS)
        return;
    tile = battle_map_get_tile_data_pointer(
        unit->movement.bytes.destination_x, unit->movement.bytes.destination_y, unit->movement.bytes.destination_z);
    if (tile == 0)
        return;
    surface = tile->surface.value & MAP_SURFACE_MASK;
    switch (surface) {
    case MAP_SURFACE_GRASSLAND:
    case MAP_SURFACE_THICKET:
    case MAP_SURFACE_RUG:
        sfx = 0x28;
        break;
    case MAP_SURFACE_SWAMP:
    case MAP_SURFACE_MARSH:
    case MAP_SURFACE_POISONED_MARSH:
    case MAP_SURFACE_WATERWAY:
    case MAP_SURFACE_RIVER:
    case MAP_SURFACE_LAKE:
    case MAP_SURFACE_SEA:
    case MAP_SURFACE_WATERFALL:
        sfx = 0x23;
        break;
    case MAP_SURFACE_WOODEN_FLOOR:
    case MAP_SURFACE_TREE:
    case MAP_SURFACE_BOX:
        sfx = 0x3A;
        break;
    default:
        sfx = 0x29;
        break;
    }
    main_sound_play_sfx_find_channel(sfx);
}
