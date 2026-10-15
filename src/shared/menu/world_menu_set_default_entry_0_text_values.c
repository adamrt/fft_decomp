#include "fft/world.h"
#include "psx/types.h"

/* Set the two substitution values of default menu entry 0: text id
 * 0xA000 + text_index and `value`. */
void world_menu_set_default_entry_0_text_values(u32 text_index, u32 value) {
    g_world_menu_default_entry_0_text_values[0] = text_index + 0xA000;
    g_world_menu_default_entry_0_text_values[1] = value;
}
