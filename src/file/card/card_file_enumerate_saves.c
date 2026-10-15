#include "fft/event_card.h"
#include "psx/types.h"

/* List the selected card slot's directory into `entries` and return the
 * count: the game's save files, or every file when `all_files` is set. */
s32 card_file_enumerate_saves(DIRENTRY* entries, s32 all_files) {
    s32 count;
    char path[128];

    if (g_card_io_selected_slot == 0) {
        strcpy(path, g_card_file_slot_0_path);
    } else {
        strcpy(path, g_card_file_slot_1_path);
    }
    if (all_files == 0) {
        strcat(path, g_card_save_file_pattern);
    } else {
        strcat(path, g_card_file_all_pattern);
    }

    count = 0;
    if (firstfile(path, entries) == entries) {
        do {
            count++;
            entries++;
        } while (nextfile(entries) == entries);
    }
    return count;
}
