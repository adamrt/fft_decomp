#include "fft/battle_text.h"
#include "fft/main.h"
#include "fft/memory.h"
#include "fft/unit.h"
#include "fft/world.h"
#include "psx/libc.h"
#include "psx/types.h"

void main_party_remove_unit(u32 index) {
    g_main_party_units[index].party_id = PARTY_ID_NONE;
}
