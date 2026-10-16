/* The selected party unit's name id, combined with its name class. */

#include "fft/main_unit.h"
#include "fft/unit.h"
#include "fft/world_menu.h"
#include "psx/types.h"

/* Return the name id of the party unit in g_formation_selected_unit_identity's
 * roster slot, packed as name_class << 8 | index. */
s32 formation_get_selected_unit_name_id(void) {
    const unit_name_id_t* name_id = &main_party_get_unit(g_formation_selected_unit_identity.roster_slot)->name_id;

    return (name_id->fields.name_class << 8) | name_id->fields.index;
}
