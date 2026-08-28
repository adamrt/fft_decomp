#include "fft/main_file.h"
#include "fft/main_runtime.h"
#include "fft/main_unit.h"

void main_entd_init_event_unit_data(int event_id) {
    entd_encounter_t* entd;
    int unit_id;

    g_main_entd_current_event_id = event_id;
    while (main_entd_open_file() == 0) {
        VSync(0);
        main_file_poll_load(&g_main_file_cd_state);
    }

    for (;;) {
        entd = main_entd_get_encounter();
        if (entd == (entd_encounter_t*)-1) {
            break;
        }
        if (entd != 0) {
            main_party_clear_all();
            for (unit_id = 0; unit_id < 16; unit_id++) {
                main_unit_init(entd, unit_id, 0, 1);
            }
            main_heap_call_free(g_main_entd_set);
            break;
        }
        VSync(0);
        main_file_poll_load(&g_main_file_cd_state);
    }
}
