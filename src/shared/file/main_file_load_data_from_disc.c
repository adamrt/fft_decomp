#include "fft/main.h"

/* Blocking read of whole sectors to destination through the shared loader. The
 * last argument is main_file_build_header's loading display mode: 0 blanks the
 * screen and sets up the "Now Loading" message, 1 leaves the display state alone,
 * and -1 turns the message off. */
void main_file_load_data_from_disc(int sector, int sectors, void* destination, int loading_display_mode) {
    main_file_load_descriptor_t* descriptor = &g_main_file_cd_state;

    main_file_build_header(descriptor, sector, sectors, destination, loading_display_mode);
    while (g_main_file_still_loading != 0) {
        main_file_poll_load(descriptor);
        VSync(0);
    }
}
