#include "fft/world.h"
#include "psx/types.h"

/* Returns the memory card slot selected for world map save/load. */
s32 world_card_get_selected_slot(void) {
    return g_card_slot;
}
