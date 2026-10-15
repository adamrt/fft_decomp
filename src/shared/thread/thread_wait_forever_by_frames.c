#include "fft/event_equip.h"
#include "fft/thread.h"

/* Park this event thread while allowing the scheduler to continue. */
void thread_wait_forever_by_frames(void) {
    for (;;) {
        thread_wait_frames(1);
    }
}
