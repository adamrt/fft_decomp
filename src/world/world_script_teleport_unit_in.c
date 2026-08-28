#include "fft/event.h"
#include "fft/world.h"

extern s32 battle_effect_set_secondary_teleport_by_misc_id(u32 misc_id);

/* The interpreter also supplies a1=1; this entry does not consume it. */
void world_script_teleport_unit_in(s32 unit_id, s32 unused) {
    s32 misc_id;

    misc_id = world_get_misc_id(unit_id);
    if (misc_id != EVENT_MISC_ID_NONE) {
        g_world_thread_inner_subroutine_callback = (void (*)(void))battle_effect_set_secondary_teleport_by_misc_id;
        world_thread_call_on_main_stack(misc_id);
    }
}
