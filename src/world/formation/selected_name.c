/* The selected party unit's name id, combined with its name class. */

#include "fft/formation.h"
#include "fft/menu.h"
#include "fft/party.h"
#include "fft/unit.h"
#include "psx/types.h"

/* Return the name id of the party unit in g_formation_selected_unit_identity's
 * roster slot, packed as name_class << 8 | index. */
s32 formation_selected_unit_name_id_get(void) {
    const unit_name_id_fields_t* name_id = &party_unit_get(g_formation_selected_unit_identity.roster_slot)->name_id;

    return (name_id->name_class << 8) | name_id->index;
}
