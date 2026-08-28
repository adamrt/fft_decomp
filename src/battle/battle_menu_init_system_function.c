#include "fft/battle.h"

struct world_unit_command_action;
extern void battle_text_determine_spell_quote(struct world_unit_command_action*, s32, s32);

s32 battle_menu_init_system_function(s32 type, s32 option, s32 unit_id, s32 parameter, s32 enabled) {
    s32 ptr;

    if (type != 8 && type != 6) {
        ptr = (s32)battle_unit_get_target_id_ptr_by_battle_id(unit_id);
        if (type == BATTLE_MENU_SYSTEM_COMMAND_SPELL_QUOTE) {
            /* The target returns the void callee's leftover $v0. */
            return ((s32 (*)(s32, s32, s32))battle_text_determine_spell_quote)(ptr, unit_id, enabled);
        }
        if (type == BATTLE_MENU_SYSTEM_COMMAND_ABILITY_ANNOUNCEMENT) {
            return battle_menu_dispatch_system_function(
                BATTLE_MENU_SYSTEM_COMMAND_ABILITY_ANNOUNCEMENT, 0, unit_id, 0, 0, (battle_ai_command_action_t*)ptr);
        }
    }
    return battle_menu_dispatch_system_function(
        type, option, unit_id, parameter, enabled, (battle_ai_command_action_t*)ptr);
}
