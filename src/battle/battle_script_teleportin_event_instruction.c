#include "fft/battle.h"

/* The event dispatcher supplies 1 as its second argument at 0x80145ecc;
 * this entry forwards only unit_id, unlike TeleportOut's removal flag. */
void battle_script_teleportin_event_instruction(s32 unit_id, s32 unused) {
    s32 misc_id = battle_get_misc_id(unit_id);
    if (misc_id != EVENT_MISC_ID_NONE) {
        g_battle_thread_call_target = (void (*)(void))battle_effect_set_secondary_teleport_by_misc_id;
        battle_thread_call_on_main_stack(misc_id);
    }
}
