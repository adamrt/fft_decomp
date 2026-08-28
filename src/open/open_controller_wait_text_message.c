#include "fft/open.h"
#include "fft/thread.h"
#include "fft/world.h"

/* The thread-2 controller's view of its open_controller_record_t. */
typedef struct open_thread_completion_t {
    s32 run_followup;
    s32 result;
} open_thread_completion_t;

void open_controller_wait_text_message(open_thread_completion_t* completion) {
    if (world_thread_is_running(2) == 0) {
        g_open_menu_formation_entry_mask = completion->result;
        g_open_current_controller_index--;
        if (completion->run_followup != 0) {
            world_text_restore_section_pointers();
        }
    }
}
