#include "fft/wldcore.h"
#include "psx/types.h"

/* Fills the caller's panel entry buffer with the menu entry ids offered at a
 * world map location and returns how many were written; the two callers store
 * the result as the panel's entry_count and pass the buffer as entries.
 *
 * Location 0x16 offers a run of consecutive entries 0xb8ed + i, one per unit
 * counted by script variable 0x65, plus one. Every other location offers the
 * three fixed entries 0xb85d-0xb85f when its state record's is_town byte is 1, and
 * appends a fourth entry 0xb860 when script variable 0x90 is set and the
 * location is one of 9, 0x0c or 0x0e.
 *
 * The barrier at the head of the append body is a delay-slot lever, not a
 * scheduling one. Without it the block's first instruction is the constant
 * load `li v1,0xb860`, which writes a register the guard `beq s1,v0` does not
 * use, so reorg steals it into that branch's delay slot and retargets the
 * branch one instruction further. The target instead leads the block with
 * `sll v0,s0,2`, which writes the branch's own operand and therefore cannot be
 * stolen, leaving the slot to be filled from the fall-through with the next
 * comparison's `li v0,0xc`. The barrier reproduces that by making the block
 * lead with an unstealable insn; it emits no instruction of its own. */
s32 wldcore_map_build_location_menu_entries(s32 location_id, s32* out) {
    s32 count;
    s32 i;

    if (location_id == 0x16) {
        count = world_script_get_variable(EVENT_SCRIPT_VAR_DEEP_DUNGEON_LIST_LENGTH) + 1;
        for (i = 0; i < count; i++) {
            out[i] = 0xB8ED + i;
        }
        return count;
    }

    count = 0;
    if (g_wldcore_location_records[location_id].is_town == 1) {
        count = 3;
        out[0] = 0xB85D;
        out[1] = 0xB85E;
        out[2] = 0xB85F;
        if (world_script_get_variable(EVENT_SCRIPT_VAR_FUR_SHOP_ENABLED) != 0) {
            if ((location_id == 9) || (location_id == 0xC) || (location_id == 0xE)) {
                __asm__ volatile("");
                out[count] = 0xB860;
                count++;
            }
        }
    }
    return count;
}
