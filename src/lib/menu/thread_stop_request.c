/* Requesting a menu thread to stop. */

#include "fft/menu_list.h"
#include "fft/thread.h"
#include "psx/types.h"

/* Clear the first two menu parameters and request stop in the third. */
void menu_thread_request_stop(s32 thread_id) {
    thread_set_parameters(thread_id, 0, 0, 1);
}
