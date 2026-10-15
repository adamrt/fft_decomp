#include "fft/battle_menu.h"
#include "psx/types.h"

/* The number of katanas battle_menu_collect_draw_out_katanas lists in
 * out_item_ids: its result without the 0x100 marker. */
s32 battle_menu_load_draw_out_abilities(s32 unit_id, s32 skillset, u8* out_item_ids) {
    return battle_menu_collect_draw_out_katanas(unit_id, skillset & 0xFF, out_item_ids) & 0xFF;
}
