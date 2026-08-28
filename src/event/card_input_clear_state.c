#include "fft/card.h"
#include "psx/types.h"

void card_input_clear_state(void) {
    g_card_input_newly_pressed = 0;
    g_card_input_secondary_repeat = 0;
    g_card_input_primary_repeat = 0;
}
