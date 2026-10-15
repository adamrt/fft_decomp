#include "fft/battle_unit.h"
#include "psx/types.h"

s32 battle_unit_has_runtime_id(u32 runtime_id) {
    return battle_unit_get_by_runtime_id(runtime_id & 0xffff) != 0;
}
