#include "fft/text.h"
#include "fft/world.h"
#include "psx/types.h"

/* Store the selected ability-name text ID, then build the prompt. */
void world_menu_build_selected_ability_prompt(void) {
    g_world_text_substitution_values[0]
        = g_world_ability_entries[g_world_menu_cursor_position] + TEXT_ID_ABILITY_NAME_BASE;
    world_menu_icon_linked_entry_thread();
}
