#include "fft/world.h"

/* Two empty tied asms, both breaking a constant equivalence, are load-bearing:
 *
 * 1. The list base is a link-time constant, so its pseudo picks up a REG_EQUIV
 *    note and reload rematerializes the address at each use rather than keeping
 *    it live; the function then loses $s0 entirely and shrinks to 104 bytes.
 *    The tied asm breaks that equivalence, giving the target's `lui/addiu $s0`
 *    after the loop, the `sh $v0,0x12($s0)` terminator store, and $s0 as the
 *    first argument of the call that follows.
 *
 * 2. Without the asm on `script`, the constant 7 is propagated into the inner
 *    call's argument, and the argument setup is emitted after the $s0 def:
 *    the scheduler hoists `ori $4,0x7` over `addiu $2,-1` but cannot move it
 *    across the asm that defines $s0, leaving the two in the wrong order.
 *    Forcing 7 into a register before that asm puts `ori $4,0x7` ahead of
 *    `lui/addiu $s0` as in the target; the pseudo is allocated $a0 directly
 *    by the argument copy suggestion, so the lever costs no instruction.
 */

/* Open the name-entry key list: identity entry ids 0..8 plus a -1 terminator. */
void world_menu_open_name_entry_key_list(void) {
    s16* list;
    s32 script;
    s32 i;

    for (i = 8; i >= 0; i--) {
        g_world_name_entry_key_ids[i] = i;
    }
    script = 7;
    /* Tied asms 2 and 1 described above. */
    __asm__("" : "=r"(script) : "0"(script));
    list = g_world_name_entry_key_ids;
    __asm__("" : "=r"(list) : "0"(list));
    list[9] = -1;
    world_menu_init_and_load_scrollable_list(
        list, g_world_menu_cursor_position, (u32)world_menu_get_script(script), g_world_name_entry_keyboard_script);
}
