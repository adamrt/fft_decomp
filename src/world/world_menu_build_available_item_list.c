/* Three load-bearing shapes:
 *
 * 1. The tied launder on `off` (not on `base`). The 8/0 store is a byte
 *    offset added to a loop-invariant g_world_menu_location_list_row_colors
 *    base with the `addu` recomputed in each arm; without a launder loop.c
 *    folds the two into a single address giv and emits a pointer walk.
 *    Laundering `base` instead rotates the callee-saved allocation.
 *
 * 2. `i = 0;` hoisted above the other initialisers, with `for (; ...)`.
 *    The register saves are emitted in initialiser order, and the target
 *    saves s0 first; initialising `i` in the for-header emits it last.
 *
 * 3. `base` is an s32 address, not a pointer, so the store address is the
 *    integer sum `off + base`. gcc canonicalises the pointer operand of
 *    pointer arithmetic into `rs` regardless of source order, so every
 *    pointer-typed spelling yields `addu v0,base,off`; the target wants
 *    `addu v0,off,base`. Integer addition preserves source order.
 *    Minus-over-negate does not work: it hoists a `negu` into the prologue
 *    and emits `subu`.
 */
#include "fft/world.h"
#include "psx/types.h"

/* Build the list of available items (script variables 0x200..0x22a) and
 * position the menu around the currently selected one. */
void world_menu_build_available_item_list(void) {
    s32 i;
    s32 count;
    s32 off;
    s16* flags;
    u16* items;
    s32 selected;
    s32 base;
    world_menu_entry_t* menu;

    i = 0;
    count = 0;
    flags = g_world_menu_location_list_row_colors;
    base = (s32)flags;
    off = 0;
    items = g_world_menu_location_list_text_ids;
    for (; i < 0x2B; i++) {
        if (world_script_get_variable(i + 0x200) != 0) {
            *items = i - 0x7000;
            if (world_script_get_variable(i + 0x267) == 0) {
                *(s16*)(off + base) = 8;
            } else {
                *(s16*)(off + base) = 0;
            }
            if (i == world_script_get_variable(EVENT_SCRIPT_VAR_LOCATION)) {
                *flags = 4;
                selected = count;
            }
            off += 2;
            flags++;
            items++;
            count++;
            /* Launder described in note 1 above. */
            __asm__("" : "=r"(off) : "0"(off));
        }
    }
    if (count >= 8) {
        g_world_menu_location_list_layout.columns.row_count = 7;
        g_world_menu_location_list_layout.columns.hidden_rows = count - 7;
    } else {
        g_world_menu_location_list_layout.columns.row_count = count;
        g_world_menu_location_list_layout.columns.hidden_rows = 0;
    }
    if (g_world_menu_location_list_layout.columns.hidden_rows < selected) {
        g_world_menu_location_list_layout.row_offset = g_world_menu_location_list_layout.columns.hidden_rows;
    } else {
        g_world_menu_location_list_layout.row_offset = selected;
    }
    menu = g_world_menu_thread_menu_data;
    menu[6].selected_index = selected;
    menu[6].window_y = (7 - g_world_menu_location_list_layout.columns.row_count) * 8 - 0x48;
    world_menu_scrolling_list_thread();
}
