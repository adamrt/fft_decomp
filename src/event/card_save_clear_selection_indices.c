#include "fft/event_card.h"
#include "psx/types.h"

void card_save_clear_selection_indices(void) {
    g_card_menu_selection_index = -1;
    g_card_save_slot_index = -1;
}
