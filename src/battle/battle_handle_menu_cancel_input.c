#include "fft/battle.h"
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

typedef struct battle_menu_cancel_context {
    u8 _pad0[0x20];
    s16 count; /* 0x20 */
    u8 _pad1[0x12];
    s16* cursor; /* 0x34 */
} battle_menu_cancel_context_t;

void battle_handle_menu_cancel_input(void* menu) {
    battle_menu_cancel_context_t* ctx = (battle_menu_cancel_context_t*)menu;
    s32 count;
    s32 i;
    s32 idx;
    s32 j;
    s16* pending;
    s32* unused_ptr;
    s16* cur;
    s32 thread;
    char unused[8];

    if ((g_battle_script_event_input & PSX_PAD_CROSS) && battle_menu_can_accept_input() != 0) {
        count = ctx->count;
        if (count != -1) {
            /* The counter is zeroed ahead of the guard, not inside the arm.
               That is what puts `move a0,zero` immediately before the `blez`,
               so reorg fills the branch's delay slot from *before* the branch
               and never considers the join block's own `i = 0` -- leaving the
               branch pointing at it (`blez a1,0xa4`).  Written inside the arm,
               the slot is instead filled from the fallthrough thread, the
               identical insn at the branch target is recognised as redundant,
               and the branch is retargeted one instruction later. */
            i = 0;
            if (count > 0) {
                thread = g_battle_current_thread_id;
                do {
                    idx = i + thread;
                    g_battle_thread_contexts[idx].function_parameter_3 = 1;
                    if (idx == 8) {
                        j = 3;
                        pending = &g_battle_menu_pending_selection[j];
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
                if (g_battle_thread_contexts[j].task_id == NATIVE_THREAD_TASK_RESUME) {
                    g_battle_thread_contexts[j].task_id = NATIVE_THREAD_TASK_STOP_REQUEST;
                }
                j++;
            } while (i < 0x10);
        }
        cur = ctx->cursor;
        g_battle_script_event_input = 0;
        *cur = -1;
        battle_sound_set_effect_to_cancel();
        g_event_input_suppression_frames = 5;
    }
}
