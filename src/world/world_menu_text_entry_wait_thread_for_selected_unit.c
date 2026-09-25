#include "fft/world.h"

void world_menu_text_entry_wait_thread_for_selected_unit(void) {
    /* The retail code reads roster_slot with lhu; keep the byte-exact width. */
    g_world_selected_unit_text_binding.speaker_id
        = *(u16*)&g_world_formation_unit_pointers[g_world_formation_selected_unit_index]->roster_slot;
    world_menu_text_entry_wait_thread();
}
