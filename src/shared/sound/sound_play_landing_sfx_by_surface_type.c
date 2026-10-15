#include "fft/battle_map.h"
#include "fft/battle_unit.h"
#include "fft/main.h"
#include "fft/sound.h"
#include "psx/types.h"

/* Play the landing sound for the surface of the tile a unit's step ends on:
 * MAIN_SFX_LAND_SOFT for grassland, thicket and rug, MAIN_SFX_LAND_WATER for the
 * water and marsh surfaces, MAIN_SFX_LAND_WOOD for wooden floor, tree and box,
 * and MAIN_SFX_LAND_DEFAULT otherwise. Nothing plays for a unit whose motion
 * flags suppress landing effects, or when the destination tile does not exist. */
void sound_play_landing_sfx_by_surface_type(battle_unit_t* unit) {
    map_tile_t* tile;
    s32 surface;
    s32 sfx;

    if (unit->movement.suppress_sfx_and_landing_effects)
        return;
    tile = map_get_tile_data_pointer(
        unit->movement.destination_x, unit->movement.destination_y, unit->movement.destination_z);
    if (tile == 0)
        return;
    surface = tile->surface.type;
    switch (surface) {
    case MAP_SURFACE_GRASSLAND:
    case MAP_SURFACE_THICKET:
    case MAP_SURFACE_RUG:
        sfx = MAIN_SFX_LAND_SOFT;
        break;
    case MAP_SURFACE_SWAMP:
    case MAP_SURFACE_MARSH:
    case MAP_SURFACE_POISONED_MARSH:
    case MAP_SURFACE_WATERWAY:
    case MAP_SURFACE_RIVER:
    case MAP_SURFACE_LAKE:
    case MAP_SURFACE_SEA:
    case MAP_SURFACE_WATERFALL:
        sfx = MAIN_SFX_LAND_WATER;
        break;
    case MAP_SURFACE_WOODEN_FLOOR:
    case MAP_SURFACE_TREE:
    case MAP_SURFACE_BOX:
        sfx = MAIN_SFX_LAND_WOOD;
        break;
    default:
        sfx = MAIN_SFX_LAND_DEFAULT;
        break;
    }
    main_sound_play_sfx_find_channel(sfx);
}
