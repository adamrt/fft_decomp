/* An empty menu input event state update. */

#include "fft/menu/menu_input.h"
#include "psx/types.h"

/* Empty in ATTACK, DEBUGCHR, EQUIP and REQUIRE; BUNIT, CARD and WORLD carry a body under this name. */
void menu_input_update_event_state(u32* otag, u32 input, s32 framebuffer_y) {
}
