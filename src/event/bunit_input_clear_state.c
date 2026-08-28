#include "fft/bunit.h"
#include "psx/types.h"

/* Reset the three masks consumed by the BUNIT menu-input processor. */
void bunit_input_clear_state(void) {
    g_bunit_menu_input_mask = 0;
    g_bunit_menu_input_repeat_mask = 0;
    g_bunit_menu_input_active_mask = 0;
}
