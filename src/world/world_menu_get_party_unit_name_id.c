/* The selected party unit's name id, combined with its name class. */

#include "fft/main_unit.h"
#include "fft/unit.h"
#include "fft/world_menu.h"
#include "psx/types.h"

/* Returns the name index combined with its name class. */
s32 world_menu_get_party_unit_name_id(void) {
    const unit_name_id_t* name_id = &main_party_get_unit(g_world_selected_unit_identity.roster_slot)->name_id;

    return (name_id->fields.name_class << 8) | name_id->fields.index;
}
