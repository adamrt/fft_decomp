/* Io. A small object between boundaries the binary proves. */

#include "fft/event_card.h"
#include "psx/types.h"

void card_io_set_selected_slot(s32 value) {
    g_card_io_selected_slot = value;
}

s32 card_io_get_selected_slot(void) {
    return g_card_io_selected_slot;
}
