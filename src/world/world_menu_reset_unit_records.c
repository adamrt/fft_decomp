#include "fft/unit_slots.h"
#include "fft/world.h"
#include "psx/types.h"

/* One row per battle unit, each 17 bytes. A typed row-array version changes the target's
 * induction variables (84 bytes rather than 76), so retain the byte view.
 * Use an integer countdown: decrementing an element pointer after row[0]
 * would form a pointer before the first row. */
enum { WORLD_UNKNOWN_ROW_BYTES = 17 };

void world_menu_reset_unit_records(void) {
    s32 index;
    u8* row;
    s32 count;
    u8 fill;

    fill = 0xFF;
    index = 0;
    row = (u8*)g_world_menu_unit_selection_rows;
    do {
        count = WORLD_UNKNOWN_ROW_BYTES - 1;
        do {
            row[count] = 0;
            count -= 1;
        } while (count >= 0);
        ((u8*)g_world_menu_unit_selection_rows)[index + 1] = fill;
        index += WORLD_UNKNOWN_ROW_BYTES;
        row += WORLD_UNKNOWN_ROW_BYTES;
    } while (index < BATTLE_UNIT_SLOT_COUNT * WORLD_UNKNOWN_ROW_BYTES);
}
