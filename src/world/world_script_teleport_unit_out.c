#include "fft/battle.h"
#include "fft/battle_gfx.h"
#include "fft/event.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

extern s32 get_unit_id_from_misc_id(s32 misc_id);
extern s32 battle_effect_set_secondary_teleport_by_misc_id(u32 misc_id);

void world_script_teleport_unit_out(s32 unit_id, s32 remove_unit) {
    s32 misc_id;
    s32 unit_index;

    misc_id = world_get_misc_id(unit_id);
    if (misc_id != EVENT_MISC_ID_NONE) {
        g_world_sound_effect_id_to_play = 0x6a;
        g_world_thread_inner_subroutine_callback = battle_effect_set_secondary_teleport_by_misc_id;
        world_thread_call_on_main_stack(misc_id);
        unit_index = get_unit_id_from_misc_id(misc_id);
        world_unit_set_animation(misc_id, 0x1b);
        battle_gfx_init_misc_unit_palette_modulation(misc_id, 2, 0x1f, 0x1f, 0x1f);
        world_thread_wait_frames(0x3c);
        battle_gfx_init_misc_unit_palette_modulation(misc_id, 2, -0x1f, -0x1f, -0x1f);
        world_thread_wait_frames(0x3c);
        if (remove_unit != 0) {
            set_unit_cannot_exist(unit_index);
        }
    }
}
