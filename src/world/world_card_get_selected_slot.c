#include "fft/world.h"
#include "psx/types.h"

/* Returns the memory card slot selected for world map save/load. */
u8 world_card_get_selected_slot(void) {
    return g_world_card_selected_slot;
}
