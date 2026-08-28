#include "fft/battle_ai.h"

/* Position the temporary simulation unit at the supplied AI coordinates. */
void battle_ai_move_temp_unit_to_coords(battle_ai_coords_t* coord) {
    u32 higher_elevation;
    g_battle_ai_temp_unit_data->x = coord->bytes.x;
    g_battle_ai_temp_unit_data->position.bits.y = coord->bytes.y;
    higher_elevation = coord->bytes.elevation << 15;
    g_battle_ai_temp_unit_data->position.raw = (g_battle_ai_temp_unit_data->position.raw & 0x7FFF) | higher_elevation;
}
