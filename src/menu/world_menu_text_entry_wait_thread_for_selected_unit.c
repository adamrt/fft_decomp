#include "fft/world.h"

/* Message entry thread whose speaker is the selected formation unit's
 * roster slot. */
void world_menu_text_entry_wait_thread_for_selected_unit(void) {
    /* The retail code reads roster_slot with lhu; keep the byte-exact width. */
    g_world_selected_unit_text_binding.speaker_id = g_world_unit_pointers[g_unit_selected_index]->identity.roster_slot;
    menu_text_entry_wait_thread();
}
