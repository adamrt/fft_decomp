#include "fft/battle_gfx.h"
#include "fft/battle_menu.h"
#include "fft/battle_script.h"
#include "fft/event_helpmenu.h"
#include "fft/event_require.h"
#include "fft/gfx.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Return a roster unit's seven equipped items to the inventory. Always
 * returns 0. */
s32 require_party_restore_item_counts(s32 party_index) {
    party_unit_t* party = main_party_get_unit(party_index);
    s32 index;

    for (index = 0; index < UNIT_EQUIPMENT_SLOT_COUNT; index++) {
        u8 item_id = party->equipment[index];

        if ((item_id != ITEM_ID_NOTHING) && (item_id != ITEM_ID_NONE)) {
            g_main_item_quantities[item_id]++;
        }
    }
    return 0;
}
