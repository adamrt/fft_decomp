#include "fft/battle_action.h"
#include "fft/battle_gfx.h"

/* Queues the gained Exp and JP displays after an action, when the Display Gained
 * Exp/JP option is on and the amount is not 0. The first display is built at once
 * unless one is already running. */
void battle_gfx_init_earned_exp_jp_display(battle_unit_t* unit) {
    battle_gfx_sprite_display_data_t* display_0;
    battle_gfx_sprite_display_data_t* display_1;
    battle_gfx_sprite_display_data_t* display_2;

    if (g_main_game_options.fields.display_gained_exp_jp == GAME_OPTION_ON) {
        display_0 = unit->numeric_display.displays[0];
        display_1 = unit->numeric_display.displays[1];
        display_2 = unit->numeric_display.displays[2];
        display_2->tpage = BATTLE_GFX_TPAGE_FRAME;
        display_1->tpage = BATTLE_GFX_TPAGE_FRAME;
        display_0->tpage = BATTLE_GFX_TPAGE_FRAME;
        if (unit->strike.action_rewards.earned_experience != 0) {
            unit->action_display_flags |= ACTION_DISPLAY_FLAG_GAINED_EXP;
        }
        if (unit->strike.action_rewards.earned_jp != 0) {
            unit->action_display_flags |= ACTION_DISPLAY_FLAG_GAINED_JP;
        }
        if (unit->numeric_display.active == 0) {
            unit->numeric_display.progress = 0;
            battle_gfx_build_next_special_action_result_display(unit);
        }
    }
}
