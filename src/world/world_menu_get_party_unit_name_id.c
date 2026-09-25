#include "fft/world.h"

/* Assembles party_data_t.name_id from its two little-endian bytes. */
s32 world_menu_get_party_unit_name_id(void) {
    u8* name_id = main_party_get_data_pointer(g_world_selected_unit_identity.roster_slot)->name_id;

    return (name_id[1] << 8) | name_id[0];
}
