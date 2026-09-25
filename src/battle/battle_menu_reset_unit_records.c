#include "fft/battle.h"

/* Reset the per-unit menu records, clearing each and marking byte 1 as
 * unset (0xFF).
 *
 * Target 0x8013f8b4. */
void battle_menu_reset_unit_records(void) {
    s32 i;
    s32 j;

    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        for (j = 16; j >= 0; j--) {
            g_battle_menu_unit_selection_records[i].bytes[j] = 0;
        }
        g_battle_menu_unit_selection_records[i].bytes[1] = 0xFF;
    }
}
