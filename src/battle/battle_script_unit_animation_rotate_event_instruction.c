#include "fft/battle.h"
#include "psx/types.h"

/*
 * Parameters: 0x00 unit id (halfword), 0x02 rotation, 0x03 animation id
 * (halfword), 0x04 move-flag mode (0 unset, else flip).
 */
void battle_script_unit_animation_rotate_event_instruction(const u8* parameters) {
    const u8* data = parameters;
    u8 rotation;
    s16 animation;
    s16 call_misc_id;
    s16 misc_id;
    s32 index;
    world_unit_animation_state_t* rotation_state;

    misc_id = battle_script_load_halfword(data);
    rotation = data[2];
    animation = battle_script_load_halfword(data + 3);
    call_misc_id = battle_get_misc_id(misc_id);
    misc_id = call_misc_id;
    if (data[4] == 0) {
        battle_unit_clear_facing_update_suppression(call_misc_id);
    } else {
        battle_unit_set_facing_update_suppression(call_misc_id);
    }
    index = misc_id;
    rotation_state = &g_battle_unit_misc_rotation_data[index];
    rotation_state->target_facing = rotation;
    rotation_state->delay = 0;
    rotation_state->rotating = 0;
    battle_unit_init_for_store_anim_facing_move(index, rotation);
    battle_unit_set_specific_animation_value_on_battle_init(index, animation);
}
