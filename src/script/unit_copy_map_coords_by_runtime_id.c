#include "fft/battle_unit.h"
#include "fft/script.h"
#include "fft/unit.h"
#include "psx/types.h"

void unit_copy_map_coords_by_runtime_id(u32 runtime_id, SVECTOR* coords) {
    battle_unit_get_map_coords_by_runtime_id(runtime_id, coords);
}
