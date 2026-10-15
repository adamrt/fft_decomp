#include "fft/wldcore.h"
#include "psx/libgpu.h"
#include "psx/types.h"

party_unit_t* wldcore_get_party_unit(s32 party_index) {
    return main_party_get_unit(party_index);
}
