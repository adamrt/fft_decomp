#include "fft/world.h"
#include "psx/etc.h"
#include "psx/libc.h"
#include "psx/types.h"

extern DIRENTRY* firstfile(char* name, DIRENTRY* dir);
extern DIRENTRY* nextfile(DIRENTRY* dir);

/* Counts the save files on the selected memory card into `dir`: every
 * FFT save when `all` is nonzero, otherwise only the BISLPS-00700FF7*
 * entries.  strcpy is left to the compiler builtin so the prefix copy is
 * expanded inline as the target shows. */
s32 world_card_count_save_files(DIRENTRY* dir, s32 all) {
    char path[128];
    s32 count;

    if (g_world_card_selected_slot == 0) {
        strcpy(path, "bu00:");
    } else {
        strcpy(path, "bu10:");
    }
    if (all == 0) {
        strcat(path, "BISLPS-00700FF7*");
    } else {
        strcat(path, "*");
    }
    count = 0;
    if (firstfile(path, dir) == dir) {
        do {
            count++;
            dir++;
        } while (nextfile(dir) == dir);
    }
    return count;
}
