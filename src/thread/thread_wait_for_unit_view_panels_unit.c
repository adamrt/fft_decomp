#include "fft/script.h"
#include "fft/thread.h"
#include "psx/types.h"

void thread_wait_for_unit_view_panels(void) {
    thread_wait_until_inactive(THREAD_SLOT_HOVERED_UNIT_STATS);
    thread_wait_until_inactive(THREAD_SLOT_RIGHT_UNIT_STATS);
    thread_wait_until_inactive(THREAD_SLOT_UNIT_SUMMARY_PANEL);
    thread_wait_until_inactive(THREAD_SLOT_TILE_INFO_PANEL);
}

/* Empty in BATTLE and WORLD; in BATTLE it sits between
 * thread_wait_for_unit_view_panels and script_noop_5. Nothing on the disc
 * calls it or points at it; the name is a neutral placeholder until a
 * reference turns up. */
void script_noop_4(void) {
}

/* Empty in BATTLE and WORLD; in BATTLE it sits between script_noop_4 and
 * script_frame_after_threads_stub. Nothing on the disc calls it or points at
 * it; the name is a neutral placeholder until a reference turns up. */
void script_noop_5(void) {
}

/* Empty. The event frame runner calls it each frame once the threads have run. */
void script_frame_after_threads_stub(void) {
}

void script_noop_6(void) {
}

/* Empty in BATTLE and WORLD; in BATTLE it sits between script_noop_6 and
 * script_assign_bitset_flag. Nothing on the disc calls it or points at it; the
 * name is a neutral placeholder until a reference turns up. */
void script_noop_7(void) {
}

/* Set or clear one flag in a 32-bit-word bitmap. */
void script_assign_bitset_flag(u32* bits, s32 index, s32 set) {
    s32 word = index / 32;
    u32 mask = 1U << (index & 0x1F);

    if (set != 0) {
        bits[word] |= mask;
    } else {
        bits[word] &= ~mask;
    }
}

/* Store five day bits and four month bits in the indexed nine-bit date record. */
void script_unlockdate_event_instruction(u32* bits, s32 record, s32 month, s32 day) {
    s32 base;
    s32 i;
    u8 mask;

    base = record * 9;
    mask = 1;
    for (i = 0; i < 5; i++) {
        script_assign_bitset_flag(bits, base + i, day & mask);
        mask <<= 1;
    }
    mask = 1;
    for (i = 0; i < 4; i++) {
        s32 bit = i + 5;
        script_assign_bitset_flag(bits, base + bit, month & mask);
        mask <<= 1;
    }
}
