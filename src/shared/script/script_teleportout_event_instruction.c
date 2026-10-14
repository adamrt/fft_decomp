#include "fft/battle_effect.h"
#include "fft/battle_gfx.h"
#include "fft/battle_script.h"
#include "fft/battle_unit.h"
#include "fft/main.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "psx/types.h"

/* TeleportOut: play the teleport sound and secondary effect on the unit, raise
 * its hands, modulate its palette by +0x1f per channel, wait 60 frames, modulate
 * it by -0x1f and wait 60 frames more. Then, when `remove` is set, mark its
 * battle unit as not existing. A unit id that does not resolve does nothing. The
 * interpreter always passes remove = 1. */
void script_teleportout_event_instruction(s32 unit_id, s32 remove) {
    s32 unit;
    s32 idx;

    unit = script_resolve_runtime_id(unit_id);
    if (unit == EVENT_RUNTIME_ID_NONE) {
        return;
    }
    g_sound_pending_sfx_id = MAIN_SFX_TELEPORT_OUT;
    g_thread_call_target = (void (*)(void))effect_set_secondary_teleport_by_runtime_id;
    thread_call_on_main_stack(unit);
    idx = battle_unit_get_battle_id_by_runtime_id(unit);
    unit_set_animation(unit, BATTLE_UNIT_ANIMATION_RAISE_HANDS);
    battle_gfx_start_event_unit_fade_out(unit, 2, 0x1F, 0x1F, 0x1F);
    thread_wait_frames(0x3C);
    battle_gfx_start_event_unit_fade_out(unit, 2, -0x1F, -0x1F, -0x1F);
    thread_wait_frames(0x3C);
    if (remove != 0) {
        battle_unit_set_cannot_exist(idx);
    }
}
