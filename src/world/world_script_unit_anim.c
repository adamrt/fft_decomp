#include "fft/battle.h"
#include "fft/world.h"
#include "psx/types.h"

/* UnitAnim event instruction: apply the encoded animation to every misc record
 * that resolves from the unit id. */
void world_script_unit_anim(const u8* parameters) {
    u16 unit_id;
    s32 misc_index;
    s32 unit_index;
    s16 animation;

    unit_id = world_script_load_halfword(parameters);
    if (world_unit_try_get_misc_data_by_id(&unit_id, &misc_index) != 0) {
        unit_index = 0;
        do {
            if (world_script_filter_unit_id_by_mode(&unit_id, (u16*)&unit_index, &misc_index) != 0) {
                if (parameters[4] == 0) {
                    battle_unit_clear_facing_update_suppression(unit_id);
                } else if (parameters[4] == 1) {
                    battle_unit_set_facing_update_suppression(unit_id);
                }
                animation = world_script_load_halfword(parameters + 2);
                world_unit_set_animation(unit_id, animation);
                if ((u32)(animation - 0x53) < 3) {
                    world_script_play_effect_and_wait();
                }
                if (misc_index == 0) {
                    break;
                }
            }
            unit_index++;
        } while (unit_index < BATTLE_UNIT_SLOT_COUNT);
    }
}
