#include "fft/battle.h"
#include "fft/world.h"
#include "psx/types.h"

/* 21 rows of 17 bytes; the same table as world_menu_reset_unit_records. */
enum { WORLD_UNKNOWN_ROW_BYTES = 17 };

/* Clear one unit's row while its auto-battle setting is unset.
 *
 * The predicate is reloaded on every iteration because the byte stores may
 * alias it, so the loop always runs all 17 times. The separate `table` local
 * is load-bearing: the target holds the table base in a pseudo established
 * before the call, which is what puts the row pointer and the row offset in
 * their target registers. */
void world_menu_reset_unit_record(s32 id) {
    u8* table;
    battle_stats_t* stats;
    u8* row;
    s32 index;
    s32 i;
    u8 fill;

    table = (u8*)g_world_menu_unit_selection_rows;
    stats = battle_unit_get_stats_from_battle_id(id);
    fill = 0xFF;
    row = table;
    index = id * WORLD_UNKNOWN_ROW_BYTES;
    row += index;
    for (i = 0; i < WORLD_UNKNOWN_ROW_BYTES; i++) {
        if (stats->auto_battle_setting == 0) {
            row[i] = 0;
            ((u8*)g_world_menu_unit_selection_rows)[index + 1] = fill;
        }
    }
}
