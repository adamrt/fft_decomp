#include "fft/world.h"
#include "psx/types.h"

/* Selects the memory card slot the world map save/load routines talk to. */
void world_card_set_selected_slot(u8 slot) {
    g_world_card_selected_slot = slot;
}
