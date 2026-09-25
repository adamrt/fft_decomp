#include "fft/main.h"

enum {
    ENTD_SET_BYTES = 0x14000,
    ENTD2_FIRST_EVENT = 0x80,
    ENTD3_FIRST_EVENT = 0x100,
    ENTD4_FIRST_EVENT = 0x180,
    ENTD1_LBA = 0xebc1,
    ENTD2_LBA = 0xebe9,
    ENTD3_LBA = 0xec11,
    ENTD4_LBA = 0xec39,
};

int main_entd_open_file(void) {
    int lba;
    entd_file_t* entd_set;

    if (g_main_entd_current_event_id != 0) {
        if (g_main_entd_current_event_id < ENTD2_FIRST_EVENT) {
            entd_set = main_heap_alloc(ENTD_SET_BYTES);
            g_main_entd_set = entd_set;
            lba = ENTD1_LBA;
        } else if (g_main_entd_current_event_id < ENTD3_FIRST_EVENT) {
            entd_set = main_heap_alloc(ENTD_SET_BYTES);
            g_main_entd_set = entd_set;
            lba = ENTD2_LBA;
        } else if (g_main_entd_current_event_id < ENTD4_FIRST_EVENT) {
            entd_set = main_heap_alloc(ENTD_SET_BYTES);
            g_main_entd_set = entd_set;
            lba = ENTD3_LBA;
        } else {
            entd_set = main_heap_alloc(ENTD_SET_BYTES);
            g_main_entd_set = entd_set;
            lba = ENTD4_LBA;
        }

        if (main_file_call_build_header(lba, ENTD_SET_BYTES, entd_set) != 0) {
            main_heap_call_free(g_main_entd_set);
            return 0;
        }
    }
    return 1;
}
