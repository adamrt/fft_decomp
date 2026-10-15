#include "fft/battle.h"
#include "fft/battle_event.h"
#include "fft/battle_menu.h"
#include "fft/battle_script.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/thread.h"
/* The `blez` target depends on which reorg pass fills its delay slot, not on
 * how the two `i = 0` insns are spelled (they are identical RTL by then).
 * Zeroing the counter ahead of the guard (see below) puts `move a0,zero`
 * immediately before the branch, so fill_simple_delay_slots takes it from
 * there and the redundancy test that would retarget the branch never runs.
 *
 * A barrier at the head of the join block does not work: it stops gcc filling
 * the slot at all, and the -gcoff line note then keeps the assembler from
 * filling it, leaving a nop.
 */
#include "psx/pad.h"
#include "psx/types.h"

/* On the cancel button, while the menu may accept input, close a menu entry and
 * its thread group.
 *
 * When the entry has a thread group (thread_count is not -1), each of its
 * threads, counted from the current thread, gets thread parameter 3 set to 1,
 * after which battle_menu_can_accept_input refuses it input; thread 8 also clears
 * the four pending selections. Every thread whose task is 1 (a running text
 * thread) is asked to stop (task 3). The entry's result cell receives -1, the
 * event input is consumed, the cancel sound is queued and event input stays
 * suppressed for 5 frames. */
void menu_handle_cancel_input(menu_entry_t* menu) {
    s32 count;
    s32 i;
    s32 idx;
    s32 j;
    s16* pending;
    s16* cur;
    s32 thread;
    char unused[8];

    if ((g_script_event_input & PSX_PAD_CROSS) && menu_can_accept_input() != 0) {
        count = menu->thread_count;
        if (count != -1) {
            /* The counter is zeroed ahead of the guard, not inside the arm.
               That is what puts `move a0,zero` immediately before the `blez`,
               so reorg fills the branch's delay slot from *before* the branch
               and never considers the join block's own `i = 0` -- leaving the
               branch pointing at it (`blez a1,0xa4`).  Written inside the arm,
               the slot is instead filled from the fallthrough thread, the
               identical insn at the branch target is recognized as redundant,
               and the branch is retargeted one instruction later. */
            i = 0;
            if (count > 0) {
                thread = g_thread_current_id;
                do {
                    idx = i + thread;
                    g_thread_contexts[idx].function_parameter_3 = 1;
                    if (idx == 8) {
                        j = 3;
                        pending = &g_menu_pending_selection[j];
                        do {
                            *pending = -1;
                            pending--;
                            j--;
                        } while (j >= 0);
                    }
                    i++;
                } while (i < count);
            }
            i = 0;
            j = 0;
            do {
                i++;
                if (g_thread_contexts[j].task_id == THREAD_TASK_RESUME) {
                    g_thread_contexts[j].task_id = THREAD_TASK_STOP_REQUEST;
                }
                j++;
            } while (i < 0x10);
        }
        cur = menu->value;
        g_script_event_input = 0;
        *cur = -1;
        sound_queue_sfx_cancel();
        g_script_input_suppression_frames = 5;
    }
}
