#include "fft/battle_text.h"
#include "fft/main.h"
#include "fft/memory.h"
#include "fft/unit.h"
#include "fft/world.h"
#include "psx/libc.h"
#include "psx/types.h"

void main_party_clear_all(void) {
    s32 i;

    for (i = PARTY_ROSTER_SLOT_COUNT - 1; i >= 0; i--) {
        g_main_party_units[i].party_id = PARTY_ID_NONE;
    }
}
