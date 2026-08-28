#include "fft/battle.h"
#include "psx/types.h"

void battle_gfx_load_casting_unit_graphics(void) {
    battle_unit_misc_data_t* unit;

    unit = battle_unit_get_casting_misc_data();
    if (unit != 0) {
        /* The sprite data block starts at the graphic trigger; 0x1e0 is the
         * current frame. */
        battle_gfx_load_trap_and_unit_frame_parts(unit, (battle_unit_anim_state_t*)&unit->sprite_graphic_trigger,
            unit->animation_frame, unit->encoded_animation);
    }
}
