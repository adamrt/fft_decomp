#include "fft/battle.h"
#include "psx/types.h"

/* Dispatches the target unit's first reaction ability whose bit is set in
 * reaction_abilities[0..2]. Formula 7 abilities never provoke a reaction.
 *
 * Counter Magic (byte 1) is tested last, after the byte-2 reactions, so
 * Absorb Used MP or Counter takes precedence over it. */
void battle_action_dispatch_target_reaction_ability(void) {
    battle_stats_t* unit;

    if (g_current_ability.formula == 7)
        return;
    if (battle_action_can_unit_react_1(g_battle_action_target) != 0)
        return;
    unit = g_battle_action_target;
    if (unit->reaction_abilities[0] & BATTLE_REACTION_SET_1_SUNKEN_STATE) {
        battle_action_check_counter_reaction_usability(
            ABILITY_ID_REACTION_SUNKEN_STATE, ABILITY_SECONDARY_FLAG_4_BLADE_GRASP_ELIGIBLE);
    } else if (unit->reaction_abilities[0] & BATTLE_REACTION_SET_1_CAUTION) {
        battle_action_check_counter_reaction_usability(
            ABILITY_ID_REACTION_CAUTION, ABILITY_SECONDARY_FLAG_4_BLADE_GRASP_ELIGIBLE);
    } else if (unit->reaction_abilities[0] & BATTLE_REACTION_SET_1_DRAGON_SPIRIT) {
        battle_action_check_counter_reaction_usability(
            ABILITY_ID_REACTION_DRAGON_SPIRIT, ABILITY_SECONDARY_FLAG_4_BLADE_GRASP_ELIGIBLE);
    } else if (unit->reaction_abilities[0] & BATTLE_REACTION_SET_1_BRAVE_UP) {
        battle_action_check_counter_reaction_usability(
            ABILITY_ID_REACTION_BRAVE_UP, ABILITY_SECONDARY_FLAG_4_BLADE_GRASP_ELIGIBLE);
    } else if (unit->reaction_abilities[1] & BATTLE_REACTION_SET_2_FAITH_UP) {
        battle_action_check_face_up_and_absorb_used_mp_usability(ABILITY_ID_REACTION_FAITH_UP);
    } else if (unit->reaction_abilities[1] & BATTLE_REACTION_SET_2_COUNTER_TACKLE) {
        battle_action_check_counter_reaction_usability(
            ABILITY_ID_REACTION_COUNTER_TACKLE, ABILITY_SECONDARY_FLAG_4_BLADE_GRASP_ELIGIBLE);
    } else if (unit->reaction_abilities[1] & BATTLE_REACTION_SET_2_COUNTER_FLOOD) {
        battle_action_check_counter_reaction_usability(
            ABILITY_ID_REACTION_COUNTER_FLOOD, ABILITY_SECONDARY_FLAG_4_COUNTER_FLOOD);
    } else if (unit->reaction_abilities[2] & BATTLE_REACTION_SET_3_ABSORB_USED_MP) {
        battle_action_check_face_up_and_absorb_used_mp_usability(ABILITY_ID_REACTION_ABSORB_USED_MP);
    } else if (unit->reaction_abilities[2] & BATTLE_REACTION_SET_3_COUNTER) {
        battle_action_check_counter_reaction_usability(
            ABILITY_ID_REACTION_COUNTER, ABILITY_SECONDARY_FLAG_4_BLADE_GRASP_ELIGIBLE);
    } else if (unit->reaction_abilities[1] & BATTLE_REACTION_SET_2_COUNTER_MAGIC) {
        battle_action_check_counter_reaction_usability(
            ABILITY_ID_REACTION_COUNTER_MAGIC, ABILITY_SECONDARY_FLAG_4_COUNTER_MAGIC);
    }
}
