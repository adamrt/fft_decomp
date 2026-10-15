#include "fft/battle.h"
#include "fft/battle_gfx.h"
#include "psx/types.h"

s32 battle_gfx_set_shadow_graphic_trigger_by_runtime_id(u32 runtime_id) {
    battle_unit_t* unit = battle_unit_get_by_runtime_id(runtime_id);

    if (unit != 0) {
        unit->shadow_graphic_trigger = 1;
        return 1;
    }
    return 0;
}

/* Hides a unit's shadow (UnitShadow event instruction). Returns 0 when no unit
 * has the runtime id. */
s32 battle_gfx_clear_shadow_graphic_trigger_by_runtime_id(u32 runtime_id) {
    battle_unit_t* unit = battle_unit_get_by_runtime_id(runtime_id);

    if (unit != 0) {
        unit->shadow_graphic_trigger = 0;
        return 1;
    }
    return 0;
}

/* Start a change to map_id (MapJumpingOut), saving the current game state: the
 * screen darkens by 0x100 / duration per frame (a duration of 0 takes one frame),
 * the map loads, the screen fades back in and the saved state resumes. */
void battle_state_start_map_jump_out(s32 map_id, s32 duration) {
    g_map_id = map_id;
    g_battle_previous_game_state = g_battle_game_state;
    g_battle_game_state = BATTLE_GAME_STATE_MAP_JUMPING_OUT;
    g_battle_state_screen_fade_step = duration != 0 ? 0x100 / duration : 0x100;
}

/* Start an event's change to map_id (MapJumpingOut2), saving the current game
 * state: the screen darkens by 0x100 / duration per frame (a duration of 0 takes
 * one frame), the map loads, and the saved state resumes behind a black screen
 * until battle_state_start_change_map_jump_in fades it in. */
void battle_state_start_change_map_jump_out(s32 map_id, s32 duration) {
    g_map_id = map_id;
    g_battle_previous_game_state = g_battle_game_state;
    g_battle_game_state = BATTLE_GAME_STATE_MAP_JUMPING_OUT_2;
    g_battle_state_screen_fade_step = duration != 0 ? 0x100 / duration : 0x100;
}

/* Start the fade-in that ends an event's map change (MapJumpingIn2), saving the
 * current game state to return to. The screen lightens by 0x100 / duration per
 * frame; a duration of 0 takes one frame. */
void battle_state_start_change_map_jump_in(s32 duration) {
    g_battle_previous_game_state = g_battle_game_state;
    g_battle_game_state = BATTLE_GAME_STATE_MAP_JUMPING_IN_2;
    g_battle_state_screen_fade_step = duration != 0 ? 0x100 / duration : 0x100;
}

/* Leave the battle for the world map: enter CloseBattle at two vertical blanks
 * per frame with a fade step of 0x100 / duration (0x100 for a duration of 0). */
void battle_state_start_close_battle(s32 duration) {
    g_main_system_animation_speed = 2;
    g_battle_game_state = BATTLE_GAME_STATE_CLOSE_BATTLE;
    g_battle_state_screen_fade_step = duration != 0 ? 0x100 / duration : 0x100;
    g_main_system_game_flow_state = MAIN_GAME_FLOW_WORLD;
}
