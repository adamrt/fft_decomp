#include "fft/event_card.h"
#include "psx/types.h"

u8 card_io_get_selected_slot(void) {
    return g_card_io_selected_slot;
}
