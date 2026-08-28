#include "fft/equip.h"
#include "psx/types.h"

/* EQUIP.OUT 001bf754 - Fill the 4 s16 selection-index slots ending at
 * g_equip_menu_result_rows with -1 (writing backwards). */
void equip_menu_reset_selection_indices(void) {
    s16 sentinel = -1;
    s32 remaining = 3;
    s16* slot = &g_equip_menu_result_rows[3];
    do {
        *slot = sentinel;
        remaining -= 1;
        slot -= 1;
    } while (remaining >= 0);
}
