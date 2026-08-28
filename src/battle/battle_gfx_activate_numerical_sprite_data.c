#include "fft/battle.h"

void battle_gfx_activate_numerical_sprite_data(battle_unit_misc_data_t* unit, s32 mode) {
    battle_gfx_sprite_display_data_t* display_0 = unit->numeric_displays[0];
    battle_gfx_sprite_display_data_t* display_1 = unit->numeric_displays[1];
    battle_gfx_sprite_display_data_t* display_2 = unit->numeric_displays[2];

    display_2->spritesheet_id = BATTLE_SPRITESHEET_ID_FRAME;
    display_1->spritesheet_id = BATTLE_SPRITESHEET_ID_FRAME;
    display_0->spritesheet_id = BATTLE_SPRITESHEET_ID_FRAME;
    switch (mode) {
    case 1:
        unit->action_display_flags.word |= BATTLE_ACTION_DISPLAY_FLAG_NO_TARGET;
        break;
    case 2:
        unit->action_display_flags.word |= BATTLE_ACTION_DISPLAY_FLAG_SILENCED;
        break;
    case 3:
        unit->action_display_flags.word |= BATTLE_ACTION_DISPLAY_FLAG_NO_MP;
        break;
    }
    if (unit->numeric_display_active == 0) {
        unit->numeric_display_progress = 0;
        battle_gfx_build_next_special_action_result_display(unit);
    }
}
