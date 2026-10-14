#include "fft/menu.h"
#include "fft/sound.h"
#include "fft/thread.h"

/* Menu thread for a refused command: queue the invalid-choice sound and exit. */
void menu_run_invalid_choice_thread(void) {
    sound_queue_sfx_invalid();
    thread_exit_current();
}
