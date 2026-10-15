#include "fft/menu.h"
#include "fft/world.h"

/* Store the selected ability-name text ID, then build the prompt. */
void world_menu_build_selected_ability_prompt(void) {
    g_text_substitution_values[0] = g_menu_ability_entries[g_menu_list_selected_index] + TEXT_ID_ABILITY_NAME_BASE;
    menu_icon_linked_entry_thread();
}
