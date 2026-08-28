#include "fft/battle.h"
#include "fft/main_runtime.h"

void battle_gfx_init_earned_exp_jp_display(battle_unit_misc_data_t* unit) {
    battle_gfx_sprite_display_data_t* display_0;
    battle_gfx_sprite_display_data_t* display_1;
    battle_gfx_sprite_display_data_t* display_2;

    if (g_main_game_options.fields.display_gained_exp_jp == GAME_OPTION_ON) {
        display_0 = unit->numeric_displays[0];
        display_1 = unit->numeric_displays[1];
        display_2 = unit->numeric_displays[2];
        display_2->spritesheet_id = BATTLE_SPRITESHEET_ID_FRAME;
        display_1->spritesheet_id = BATTLE_SPRITESHEET_ID_FRAME;
        display_0->spritesheet_id = BATTLE_SPRITESHEET_ID_FRAME;
        if (unit->action_rewards.earned_experience != 0) {
            unit->action_display_flags.word |= BATTLE_ACTION_DISPLAY_FLAG_GAINED_EXP;
        }
        if (unit->action_rewards.earned_jp != 0) {
            unit->action_display_flags.word |= BATTLE_ACTION_DISPLAY_FLAG_GAINED_JP;
        }
        if (unit->numeric_display_active == 0) {
            unit->numeric_display_progress = 0;
            /* The target passes two extra arguments the one-parameter callee ignores. */
            ((s32 (*)(battle_unit_misc_data_t*, battle_gfx_sprite_display_data_t*,
                battle_unit_misc_data_t*))battle_gfx_build_next_special_action_result_display)(unit, display_0, unit);
        }
    }
}
