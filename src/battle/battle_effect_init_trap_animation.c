/* The table is declared `u8[]` and cast only at the halfword read.
 *
 * The target has no stack frame. Casting an array symbol away from its
 * declared element type (e.g. `(u8*)table + offset` off a `u16[]`, or
 * `(u32)`, `&table[0]` or a local `u8* p`) allocates an expand-time stack
 * temp that later optimization deletes, and gcc 2.6.3 never shrinks the frame
 * afterwards: an 8-byte frame appears whose `addiu sp` pair displaces the
 * load-delay nop and the `jr ra` delay-slot store. No zero-instruction
 * constraint reaches that temp; only keeping every symbol at its own type
 * avoids it.
 */
#include "fft/battle.h"
#include "psx/types.h"

/* Start the animation script of trap group on slot: reset the colour and
 * script state and store palette_id (the VRAM palette; callers
 * pass CLUT-shaped values such as 0x7aca) at 0x88. */
void battle_effect_init_trap_animation(s32 group, u16 palette_id, battle_effect_slot_t* slot) {
    u32 offset;

    slot->clut = palette_id;
    offset = *(u16*)(g_battle_effect_trap_animation_data + (g_battle_effect_trap_particle_configs[group * 46] << 1));
    slot->blue = 0x80;
    slot->green = 0x80;
    slot->red = 0x80;
    slot->step_index = 0;
    slot->hold = 0;
    slot->angle = 0;
    slot->script = (battle_effect_anim_script_t*)(g_battle_effect_trap_animation_data + offset);
}
