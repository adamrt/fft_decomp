#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Initialize the list state, then load its visible entries from the menu
 * definition. */
void world_menu_init_and_load_scrollable_list(
    const s16* values, s32 selected_index, u8* text_table, menu_instruction_t* script) {
    menu_init_scrollable_list_core_variant(values, selected_index, text_table);
    world_menu_load_scrollable_list_layout(script);
}
