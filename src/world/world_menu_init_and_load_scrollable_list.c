#include "fft/world.h"

/* Initialize the list state, then load its visible entries from the menu
 * definition. */
void world_menu_init_and_load_scrollable_list(const s16* values, s32 selected_index, u32 state, void* menu) {
    world_menu_init_scrollable_list_core(values, selected_index, state);
    world_menu_load_scrollable_list_layout((world_menu_list_record_t*)menu);
}
