#include "fft/battle.h"
#include "psx/types.h"

/*
 * UnitAnim event instruction.  Parameters: 0x00 unit id
 * (halfword), 0x02 animation id (halfword), 0x04 move-flag mode (0 unset,
 * 1 flip).  A unit id above 0xff selects a group, so the misc-state loop
 * runs over all 21 unit indices.
 */
void battle_script_unit_anim(u8* parameters) {
    u16 misc_id;
    s32 state;
    s32 unit_index;
    s32 animation;
    s32 move_flag_mode;

    misc_id = battle_script_load_halfword(parameters);
    if (battle_unit_try_get_misc_data_by_unit_id(&misc_id, &state) == 0) {
        return;
    }
    unit_index = 0;
    do {
        if (battle_script_filter_unit_id_by_mode(&misc_id, (u16*)&unit_index, &state) != 0) {
            move_flag_mode = parameters[4];
            if (move_flag_mode == 0) {
                battle_unit_clear_facing_update_suppression(misc_id);
            } else if (move_flag_mode == 1) {
                battle_unit_set_facing_update_suppression(misc_id);
            }
            animation = (s16)battle_script_load_halfword(parameters + 2);
            battle_unit_set_specific_animation_value_on_battle_init(misc_id, animation);
            if ((u32)(animation - 0x53) < 3) {
                battle_effect_play_and_wait_for_animation();
            }
            if (state == 0) {
                return;
            }
        }
        unit_index = unit_index + 1;
    } while (unit_index < EVENT_UNIT_SLOT_COUNT);
}
