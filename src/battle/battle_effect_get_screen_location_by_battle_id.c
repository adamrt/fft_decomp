#include "fft/battle.h"
#include "fft/battle_effect.h"

typedef struct battle_effect_coordinate_data battle_effect_coordinate_data_t;
typedef struct battle_effect_screen_location battle_effect_screen_location_t;

/* Resolve a battle ID before calculating its effect-space screen location. */
void battle_effect_get_screen_location_by_battle_id(s32 adjust_for_tile_height, u32 battle_id,
    const battle_effect_coordinate_data_t* effect_coordinates, battle_effect_screen_location_t* destination) {
    battle_effect_resolve_target_render_coords(adjust_for_tile_height, battle_unit_get_misc_id_by_battle_id(battle_id),
        (battle_effect_coord_data_t*)effect_coordinates, (SVECTOR*)destination);
}
