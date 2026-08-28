#include "fft/attack.h"
#include "psx/types.h"

void attack_deploy_remove_roster_unit_from_tiles(s32 roster_id) {
    s32 row;
    s32 empty;
    u8* row_entries;
    u8* entry;
    u8* end;

    row = 0;
    empty = 0xff;
    row_entries = g_attack_deploy_roster_id_by_tile[0];
    do {
        entry = row_entries;
        end = row_entries + 5;
        do {
            if (*entry == roster_id) {
                *entry = empty;
                return;
            }
            entry++;
        } while ((s32)entry < (s32)end);
        row++;
        row_entries += 5;
    } while (row < 5);
}
