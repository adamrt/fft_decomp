#include "fft/battle.h"
#include "psx/types.h"

/* MirrorSprite (event instruction 0x68): mirrors the unit's sprite when the
 * operand byte is 1 and restores it otherwise. */
void battle_script_mirrorsprite_event_instruction(u8* parameters) {
    s16 unit_id;
    s32 mirror;
    s32 misc_id;

    unit_id = battle_script_load_halfword(parameters);
    mirror = parameters[2];
    misc_id = battle_get_misc_id(unit_id);
    if (misc_id != EVENT_MISC_ID_NONE) {
        if (mirror == 1) {
            battle_unit_set_horizontal_flip_flag(misc_id);
        } else {
            battle_unit_clear_horizontal_flip_flag(misc_id);
        }
    }
}
