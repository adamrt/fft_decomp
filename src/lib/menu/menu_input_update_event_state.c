/* An empty menu input event state update. */

#include "fft/menu_input.h"
#include "psx/types.h"

/* Empty in ATTACK, DEBUGCHR, EQUIP and REQUIRE; BUNIT, CARD and WORLD carry a body under this name. */
void menu_input_event_state_update(u32* otag, u32 input, s32 framebuffer_y) {
}
