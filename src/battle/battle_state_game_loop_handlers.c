#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_camera.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_script.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "fft/script.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/*
 * Per-frame handler for the BattleTargetSelectMode game state.
 *
 * Circle accepts the unit under the cursor. Selected ability 0xc requires a
 * unit outside the caster's non-player team and 0xe one inside it; any other
 * value leaves the selection pending. X returns to the caster and the script
 * action menus; triangle only recenters the cursor on the caster.
 */
void battle_state_handle_target_select_state(void) {
    battle_unit_t* caster;
    battle_unit_t* target;

    camera_handle_rotation_input();
    camera_update_zoom_input();
    camera_update_tilt_input();
    target_move_cursor_by_input();
    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    if (g_main_input_pressed_buttons & PSX_PAD_CIRCLE) {
        caster = battle_unit_get_casting();
        target = battle_unit_find_selectable_at_tile(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z);
        /* The targeted choice requires an enemy, the recovery choice an ally. */
        if (target == 0) {
            battle_state_enter_target_select_denied();
        } else if (caster->target_select_command == AI_COMMAND_AUTOBATTLE_TARGETED_SELECT) {
            if ((caster->stats->team_flags & BATTLE_TEAM_MASK) == (target->stats->team_flags & BATTLE_TEAM_MASK)) {
                battle_state_enter_target_select_denied();
            } else {
                battle_state_enter_target_select_confirm();
            }
        } else if (caster->target_select_command == AI_COMMAND_AUTOBATTLE_RECOVERY_SELECT) {
            if ((caster->stats->team_flags & BATTLE_TEAM_MASK) == (target->stats->team_flags & BATTLE_TEAM_MASK)) {
                battle_state_enter_target_select_confirm();
            } else {
                battle_state_enter_target_select_denied();
            }
        }
        battle_gfx_update_unit_palettes();
    } else if (g_main_input_pressed_buttons & PSX_PAD_CROSS) {
        target_move_cursor_to_unit(battle_unit_get_casting());
        battle_gfx_update_unit_palettes();
        battle_menu_set_next_script_action_menus();
    } else if (g_main_input_pressed_buttons & PSX_PAD_TRIANGLE) {
        target_move_cursor_to_unit(battle_unit_get_casting());
    }
}

/* TargetSelectDenied game-state handler: when the message closes (command 7, 8 or
 * 0xff), return to target selection. */
void battle_state_handle_target_select_denied_state(void) {
    s32 selected_command;

    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    selected_command = menu_get_selected_command_address()->kind;
    if (selected_command >= 7 && (selected_command < 9 || selected_command == 0xff)) {
        battle_state_enter_target_select();
    }
}

/* TargetSelectConfirm game-state handler: command 7 stores the unit under the
 * cursor as the casting unit's auto-battle target and opens the next action
 * menus; 8 or 0xff returns to target selection. */
void battle_state_handle_target_select_confirm_state(void) {
    battle_unit_t* caster;
    battle_unit_t* target;

    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    switch ((s32)menu_get_selected_command_address()->kind) {
    case 7:
        caster = battle_unit_get_casting();
        target = battle_unit_find_selectable_at_tile(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z);
        menu_store_auto_battle_main_target_id(target->stats->battle_id, caster->stats->battle_id);
        target_move_cursor_to_unit(caster);
        battle_menu_set_next_script_action_menus();
        break;
    case 8:
    case 0xFF:
        battle_state_enter_target_select();
        break;
    }
}

/* TargetDisplayStart game-state handler: once the prompt has closed (command 7, 8
 * or 0xff) and the camera has settled, enter the target display. */
void battle_state_handle_target_display_start_state(void) {
    s32 id;

    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    id = menu_get_selected_command_address()->kind;
    if ((id >= 7) && ((id < 9) || (id == 0xFF))) {
        g_action_post_action = 1;
    }
    if ((g_camera_step_vector.vx | g_camera_step_vector.vy | g_camera_step_vector.vz) == 0) {
        if (g_camera_rotation_action == CAMERA_ROTATION_NONE) {
            if (g_action_post_action != 0) {
                battle_state_enter_target_display();
            }
        }
    }
}

/* TargetDisplay game-state handler: circle or cross puts the cursor back on the
 * casting unit and opens the next action menus. */
void battle_state_handle_target_display_state(void) {
    s32 flags;
    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    flags = g_main_input_pressed_buttons;
    if ((flags & PSX_PAD_CIRCLE) || (flags & PSX_PAD_CROSS)) {
        target_move_cursor_to_unit(battle_unit_get_casting());
        battle_menu_set_next_script_action_menus();
    }
}

/* AfterCommand game-state handler: once the window has closed (command 7, 8 or
 * 0xff) and the source unit's numeric display is idle, enter the JP/EXP gain
 * state. */
void battle_state_handle_after_command_state(void) {
    ai_command_t* command_address;
    s32 command;
    battle_unit_t* unit;

    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    command_address = menu_get_selected_command_address();
    unit = battle_unit_get_source();
    command = command_address->kind;
    if ((command >= 7) && ((command < 9) || (command == 0xFF))) {
        g_action_post_action = 1;
    }
    if ((g_action_post_action != 0) && (unit->numeric_display.active == 0)) {
        battle_state_enter_jp_xp_gain();
    }
}

/* JpExpGain game-state handler: move on once the camera has settled (no cursor
 * vector, no rotation).
 *
 * After a charged ability (turn event ABILITY_READY) a queued scenario event with
 * a nonzero finish operation starts; otherwise the between-turn events run. On a
 * unit's own turn, a unit whose turn is over (action_check_change_of_turn
 * returns 1) gets its last command and ability copied back into its runtime record;
 * a Jump then resets its preparation and goes to the between-turn events, and
 * anything else opens the Wait menu. A unit whose turn goes on gets its action
 * menus back. */
void battle_state_handle_jp_exp_gain_state(void) {
    battle_unit_t* unit;

    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    unit = battle_unit_get_source();
    if ((g_camera_step_vector.vx | g_camera_step_vector.vy | g_camera_step_vector.vz) != 0) {
        return;
    }
    if (g_camera_rotation_action != CAMERA_ROTATION_NONE) {
        return;
    }
    if (g_action_turn_event_type == BATTLE_TURN_EVENT_ABILITY_READY) {
        if (menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_SCENARIO_CHECK, 2, unit->stats->battle_id, 0, 1) == 2
            && script_get_event_finish_operation() != 0) {
            g_battle_previous_game_state = g_battle_game_state;
            menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_SCENARIO_CHECK, 0, unit->stats->battle_id, 0, 1);
            action_set_casting_unit_id_ff();
            return;
        }
        action_check_between_turn_events();
        return;
    }
    if (action_check_change_of_turn(unit->stats->battle_id) == 1) {
        /* unit_stats_t +0x16e holds the same 20-byte command payload. */
        unit->command_state.ai.data.action = unit->stats->action_command;
        unit->used_ability_id = unit->stats->action_command.ability_id;
        if (unit->attack_phase_state == BATTLE_ATTACK_PREPARATION_JUMP) {
            unit->attack_phase_state = BATTLE_ATTACK_PREPARATION_CHARGE;
            battle_gfx_reset_unit_graphic_trigger(unit->runtime_id);
            action_check_between_turn_events();
            return;
        }
        action_choose_wait();
        return;
    }
    battle_menu_set_next_script_action_menus();
}

/* ChangeTurn game-state handler. Once the menu layer reports command 7, 8 or 0xff
 * (the message closed) and the camera has settled (no cursor vector, no
 * rotation), the turn event type picks the next state: a ready ability enters the
 * action cast, an action result or a Mime action enters status execution, and
 * anything else opens the next action menus. */
void battle_state_handle_change_turn_state(void) {
    s32 command;
    s32 type;

    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    command = menu_get_selected_command_address()->kind;
    if (command >= 7) {
        if (command < 9) {
            g_action_post_action = 1;
        } else if (command == 0xFF) {
            g_action_post_action = 1;
        }
    }
    if (g_action_post_action != 0) {
        if ((g_camera_step_vector.vx | g_camera_step_vector.vy | g_camera_step_vector.vz) == 0) {
            if (g_camera_rotation_action == CAMERA_ROTATION_NONE) {
                battle_unit_get_source();
                type = g_action_turn_event_type;
                if (type == BATTLE_TURN_EVENT_ABILITY_READY) {
                    battle_state_enter_action_cast();
                    return;
                }
                if ((type == BATTLE_TURN_EVENT_ACTION_RESULT) || (type == BATTLE_TURN_EVENT_MIME)) {
                    battle_state_enter_status_execute();
                    return;
                }
                battle_menu_set_next_script_action_menus();
            }
        }
    }
}

/* StatusExecute game-state handler: once the message has closed (command 7, 8 or
 * 0xff), no animation is pending and the source unit's numeric display is idle,
 * refresh the unit's display and run the between-turn events. */
void battle_state_handle_status_execute_state(void) {
    s32 id;
    battle_unit_t* unit;

    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    id = menu_get_selected_command_address()->kind;
    if ((id >= 7) && ((id < 9) || (id == 0xFF))) {
        g_action_post_action = 1;
    }
    if (g_action_post_action != 0) {
        if (g_battle_state_animation_continue_check == 0) {
            unit = battle_unit_get_source();
            if (unit->numeric_display.active == 0) {
                battle_unit_update_display_by_runtime_id(unit->runtime_id);
                action_check_between_turn_events();
            }
        }
    }
}

/* Effect game-state handler: animate the units, the screen color modulation and
 * the camera while an effect plays. When the effect cycle has ended, the game
 * state and animation speed saved by battle_state_enter_effect_playback are
 * restored. Neither function that stores this state has a caller, so the
 * handler does not run. */
void battle_state_handle_effect_state(void) {
    battle_gfx_update_all_unit_rotation_and_vectors();
    if (g_battle_menu_status_screen_selected != 1) {
        battle_unit_update_and_animate_units();
        battle_gfx_update_screen_color_modulation_fade();
        camera_update_real_coord_animation();
        camera_update_screen_offset_animation();
        camera_update_zoom_animation();
        camera_update_rotation_animation();
    }
    battle_gfx_update_status_bubbles_and_graphics();
    battle_gfx_draw_screen_color_modulation_overlay();
    if (g_battle_state_animation_continue_check == 0) {
        g_battle_game_state = g_battle_previous_game_state;
        g_main_system_animation_speed = g_main_saved_animation_speed;
    }
}

/* Advances the live-map event state and submits its active screen fade.
 *
 * Keeping the loaded intensity and the clamped display level as separate
 * values retains the target's two moves before its unsigned 0x100 comparison.
 * The otherwise-unused 24-byte local preserves the original 64-byte frame. */
void battle_state_update_event_mode(void) {
    s16 intensity;
    u16 level;
    char unused[24];

    battle_gfx_update_all_unit_rotation_and_vectors();
    if (g_battle_menu_status_screen_selected != 1) {
        battle_unit_update_and_animate_units();
        battle_gfx_update_screen_color_modulation_fade();
        camera_update_real_coord_animation();
        camera_update_screen_offset_animation();
        camera_update_zoom_animation();
        camera_update_rotation_animation();
        move_update_all_unit_paths();
    }
    battle_gfx_update_status_bubbles_and_graphics();
    battle_gfx_draw_screen_color_modulation_overlay();
    switch (script_run_event_frame_variant(main_gfx_get_otag(), g_input_current_buttons)) {
    case 0:
        battle_gfx_init_evtchr_vram_slots();
        g_battle_menu_status_enabled = 1;
        ai_init_workspace();
        action_check_between_turn_events();
        break;
    case 9:
        battle_gfx_init_evtchr_vram_slots();
        battle_state_halve_animation_speed_and_queue_close(8, MAIN_GAME_FLOW_WORLD);
        break;
    case 10:
        battle_gfx_init_evtchr_vram_slots();
        battle_state_halve_animation_speed_and_queue_close(8, MAIN_GAME_FLOW_BATTLE);
        break;
    case 11:
        battle_gfx_init_evtchr_vram_slots();
        battle_state_halve_animation_speed_and_queue_close(8, MAIN_GAME_FLOW_RESET);
        break;
    case 19:
        battle_gfx_init_evtchr_vram_slots();
        battle_state_halve_animation_speed_and_queue_close(8, MAIN_GAME_FLOW_ENDING);
        break;
    }

    if (g_battle_screen_fade_intensity != 0) {
        intensity = g_battle_screen_fade_intensity;
        level = intensity;
        if (level >= 0x100) {
            level = 0xff;
        }
        g_battle_screen_fade_polygons[g_main_gfx_screen_polarity].r0 = level;
        g_battle_screen_fade_polygons[g_main_gfx_screen_polarity].g0 = level;
        g_battle_screen_fade_polygons[g_main_gfx_screen_polarity].b0 = level;

        AddPrim(main_gfx_get_otag(), &g_battle_screen_fade_polygons[g_main_gfx_screen_polarity]);
        AddPrim(main_gfx_get_otag(), &g_battle_screen_fade_draw_modes[g_main_gfx_screen_polarity]);
    }
}

void battle_state_stop_map_animations(void) {
    map_stop_clut_cycles_and_weather_sfx();
}

/* MapJumpingOut game-state handler: darken the screen by
 * g_battle_state_screen_fade_step per frame. Past full black it stops the map
 * animations, clears the units and starts the map init sequence (MapInitialize),
 * which fades back in by itself. */
void battle_state_handle_map_jumping_out_state(void) {
    u16 intensity;
    char unused[24];

    battle_state_handle_free_cursor_input();
    intensity = g_battle_screen_fade_intensity;
    if (intensity >= 0x100) {
        intensity = 0xff;
    }

    g_battle_screen_fade_polygons[g_main_gfx_screen_polarity].r0 = intensity;
    g_battle_screen_fade_polygons[g_main_gfx_screen_polarity].g0 = intensity;
    g_battle_screen_fade_polygons[g_main_gfx_screen_polarity].b0 = intensity;

    AddPrim(main_gfx_get_otag(), &g_battle_screen_fade_polygons[g_main_gfx_screen_polarity]);
    AddPrim(main_gfx_get_otag(), &g_battle_screen_fade_draw_modes[g_main_gfx_screen_polarity]);

    g_battle_screen_fade_intensity += g_battle_state_screen_fade_step;
    if (g_battle_screen_fade_intensity >= 0x102) {
        g_battle_screen_fade_intensity = 0xff;
        g_battle_game_state = BATTLE_GAME_STATE_MAP_INITIALIZE;
        g_battle_state_map_init_step = 0;
        battle_state_stop_map_animations();
        battle_unit_clear_all();
    }
}

/* MapJumpingOut2 game-state handler: darken the screen by
 * g_battle_state_screen_fade_step per frame. Past full black it stops the map
 * animations, clears the units and starts the map init sequence (MapInitialize2). */
void battle_state_handle_change_map_jumping_out_state(void) {
    u16 intensity;
    char unused[24];

    battle_state_handle_free_cursor_input();
    intensity = g_battle_screen_fade_intensity;
    if (intensity >= 0x100) {
        intensity = 0xff;
    }

    g_battle_screen_fade_polygons[g_main_gfx_screen_polarity].r0 = intensity;
    g_battle_screen_fade_polygons[g_main_gfx_screen_polarity].g0 = intensity;
    g_battle_screen_fade_polygons[g_main_gfx_screen_polarity].b0 = intensity;

    AddPrim(main_gfx_get_otag(), &g_battle_screen_fade_polygons[g_main_gfx_screen_polarity]);
    AddPrim(main_gfx_get_otag(), &g_battle_screen_fade_draw_modes[g_main_gfx_screen_polarity]);

    g_battle_screen_fade_intensity += g_battle_state_screen_fade_step;
    if (g_battle_screen_fade_intensity >= 0x102) {
        g_battle_screen_fade_intensity = 0xff;
        g_battle_game_state = BATTLE_GAME_STATE_MAP_INITIALIZE_2;
        g_battle_state_map_init_step = 0;
        battle_state_stop_map_animations();
        battle_unit_clear_all();
    }
}

/* CloseBattle game-state handler: draw the fade overlay and raise its intensity
 * by g_battle_state_screen_fade_step. The game loop leaves in the frame it finds
 * this state, so the handler runs for at most one frame. */
void battle_state_handle_close_battle_state(void) {
    u16 intensity;
    /* The original frame reserves this otherwise-unused stack area. */
    u8 stack_padding[0x18];

    battle_state_handle_free_cursor_input();
    intensity = g_battle_screen_fade_intensity;
    if (intensity >= 0x100) {
        intensity = 0xff;
    }
    g_battle_screen_fade_polygons[g_main_gfx_screen_polarity].r0 = intensity;
    g_battle_screen_fade_polygons[g_main_gfx_screen_polarity].g0 = intensity;
    g_battle_screen_fade_polygons[g_main_gfx_screen_polarity].b0 = intensity;
    AddPrim(main_gfx_get_otag(), &g_battle_screen_fade_polygons[g_main_gfx_screen_polarity]);
    AddPrim(main_gfx_get_otag(), &g_battle_screen_fade_draw_modes[g_main_gfx_screen_polarity]);
    g_battle_screen_fade_intensity += g_battle_state_screen_fade_step;
}

/* MapInitialize game-state handler: run one step of the map init sequence behind
 * a black screen; at step 0xd start the fade-in (MapJumpingIn). */
void battle_state_handle_map_init_state(void) {
    char unused[24]; /* unreferenced; sizes the target's 0x30-byte frame */

    g_battle_state_map_init_step = map_step_init_sequence(g_map_id, g_battle_state_map_init_step);
    g_battle_screen_fade_polygons[g_main_gfx_screen_polarity].r0 = 0xff;
    g_battle_screen_fade_polygons[g_main_gfx_screen_polarity].g0 = 0xff;
    g_battle_screen_fade_polygons[g_main_gfx_screen_polarity].b0 = 0xff;
    AddPrim(main_gfx_get_otag(), &g_battle_screen_fade_polygons[g_main_gfx_screen_polarity]);
    AddPrim(main_gfx_get_otag(), &g_battle_screen_fade_draw_modes[g_main_gfx_screen_polarity]);
    if (g_battle_state_map_init_step == 0xd) {
        g_battle_game_state = BATTLE_GAME_STATE_MAP_JUMPING_IN;
        g_battle_screen_fade_intensity = 0xff;
    }
}

/* MapInitialize2 game-state handler, the map change an event asks for: run one
 * step of the map init sequence behind a black screen. At step 0xd the saved game
 * state resumes with the screen still black; the event fades it in with
 * battle_state_start_change_map_jump_in. */
void battle_state_handle_change_map_init_state(void) {
    s32 old_state;
    char unused[24]; /* unreferenced; sizes the target's 0x30-byte frame */

    g_battle_state_map_init_step = map_step_init_sequence(g_map_id, g_battle_state_map_init_step);
    g_battle_screen_fade_polygons[g_main_gfx_screen_polarity].r0 = 0xff;
    g_battle_screen_fade_polygons[g_main_gfx_screen_polarity].g0 = 0xff;
    g_battle_screen_fade_polygons[g_main_gfx_screen_polarity].b0 = 0xff;
    AddPrim(main_gfx_get_otag(), &g_battle_screen_fade_polygons[g_main_gfx_screen_polarity]);
    AddPrim(main_gfx_get_otag(), &g_battle_screen_fade_draw_modes[g_main_gfx_screen_polarity]);
    if (g_battle_state_map_init_step == 0xd) {
        old_state = g_battle_previous_game_state;
        g_battle_previous_game_state = BATTLE_GAME_STATE_FREE_CURSOR;
        g_battle_screen_fade_intensity = 0xff;
        g_battle_game_state = old_state;
    }
}

/* MapJumpingIn game-state handler: lighten the screen by
 * g_battle_state_screen_fade_step per frame. When it is clear, the saved game
 * state resumes at animation speed 1. */
void battle_state_handle_map_jumping_in_state(void) {
    u16 intensity;
    s32 previous_state;
    char unused[24];

    battle_state_handle_free_cursor_input();
    intensity = g_battle_screen_fade_intensity;
    if (intensity >= 0x100) {
        intensity = 0xff;
    }
    g_battle_screen_fade_polygons[g_main_gfx_screen_polarity].r0 = intensity;
    g_battle_screen_fade_polygons[g_main_gfx_screen_polarity].g0 = intensity;
    g_battle_screen_fade_polygons[g_main_gfx_screen_polarity].b0 = intensity;
    AddPrim(main_gfx_get_otag(), &g_battle_screen_fade_polygons[g_main_gfx_screen_polarity]);
    AddPrim(main_gfx_get_otag(), &g_battle_screen_fade_draw_modes[g_main_gfx_screen_polarity]);
    g_battle_screen_fade_intensity -= g_battle_state_screen_fade_step;
    if (g_battle_screen_fade_intensity <= 0) {
        previous_state = g_battle_previous_game_state;
        g_battle_screen_fade_intensity = 0;
        g_battle_previous_game_state = BATTLE_GAME_STATE_FREE_CURSOR;
        g_main_system_animation_speed = 1;
        g_battle_game_state = previous_state;
    }
}

/* MapJumpingIn2 game-state handler, the fade-in that ends an event's map change:
 * it advances the simulation unless the status screen owns the frame, runs the
 * event script and then draws the subtractive fade overlay one step lighter. When
 * the fade reaches zero the saved game state is restored. */
void battle_state_handle_change_map_jumping_in_state(void) {
    u16 intensity;
    char unused[24];

    battle_gfx_update_all_unit_rotation_and_vectors();
    if (g_battle_menu_status_screen_selected != 1) {
        battle_unit_update_and_animate_units();
        battle_gfx_update_screen_color_modulation_fade();
        camera_update_real_coord_animation();
        camera_update_screen_offset_animation();
        camera_update_zoom_animation();
        camera_update_rotation_animation();
        move_update_all_unit_paths();
    }
    battle_gfx_update_status_bubbles_and_graphics();
    battle_gfx_draw_screen_color_modulation_overlay();
    switch (script_run_event_frame_variant(main_gfx_get_otag(), g_input_current_buttons)) {
    case 1:
        break;
    case 0:
        action_check_between_turn_events();
        break;
    case 9:
        battle_state_halve_animation_speed_and_queue_close(8, MAIN_GAME_FLOW_BATTLE);
        break;
    case 10:
        battle_state_halve_animation_speed_and_queue_close(8, MAIN_GAME_FLOW_WORLD);
        break;
    }

    intensity = g_battle_screen_fade_intensity;
    if (intensity >= 0x100) {
        intensity = 0xff;
    }
    g_battle_screen_fade_polygons[g_main_gfx_screen_polarity].r0 = intensity;
    g_battle_screen_fade_polygons[g_main_gfx_screen_polarity].g0 = intensity;
    g_battle_screen_fade_polygons[g_main_gfx_screen_polarity].b0 = intensity;

    AddPrim(main_gfx_get_otag(), &g_battle_screen_fade_polygons[g_main_gfx_screen_polarity]);
    AddPrim(main_gfx_get_otag(), &g_battle_screen_fade_draw_modes[g_main_gfx_screen_polarity]);

    g_battle_screen_fade_intensity -= g_battle_state_screen_fade_step;
    if (g_battle_screen_fade_intensity <= 0) {
        g_battle_screen_fade_intensity = 0;
        g_battle_game_state = g_battle_previous_game_state;
        g_battle_previous_game_state = BATTLE_GAME_STATE_FREE_CURSOR;
    }
}

/* Uploads the fixed CLUTs of the sprite row: 1 and 2 the deployed-unit palettes,
 * 3 the wait-direction arrow, 4 the map selection cursor, 5 the shadow and 7 the
 * status bubble. */
void battle_gfx_extract_deployed_unit_palettes(void) {
    RECT rect; /* never used; retail reserves its 8 bytes */

    map_load_palette_data(&g_battle_deployed_unit_palettes[0], MAP_CLUT_ROW_SPRITES, 1, 0);
    map_load_palette_data(&g_battle_deployed_unit_palettes[1], MAP_CLUT_ROW_SPRITES, 2, 0);
    map_load_palette_data(&g_battle_gfx_wait_direction_arrow_palette, MAP_CLUT_ROW_SPRITES, 3, 0);
    map_load_palette_data(&g_battle_gfx_map_selection_cursor_palette, MAP_CLUT_ROW_SPRITES, 4, 0);
    map_load_palette_data(&g_battle_gfx_shadow_palette, MAP_CLUT_ROW_SPRITES, 5, 0);
    map_load_palette_data(&g_battle_gfx_status_bubble_palette, MAP_CLUT_ROW_SPRITES, 7, 0);
}

/* Empty. battle_state_run_deployment calls it each frame while the map is still being set up. */
void battle_state_deployment_frame_stub(void) {
}

/* Fade in the deployment banner, run map initialization until it reaches step
 * 0xd, then fade the screen back in.
 *
 * banner_p and the loop's fade = 0xff reproduce the target's preheader order and
 * register choice for the hoisted color. The dead fade = 0 before done keeps the
 * step-0xd exit inline ahead of the per-frame path, as in the target; with a
 * plain break GCC moves that block to the loop tail. */
void battle_state_run_deployment(void) {
    RECT window = { 0, 0, 0, 0 };
    POLY_FT4 banner[2];
    u8 fade_enabled;
    s32 map_id;
    s16 fade;
    POLY_FT4* banner_p;

    battle_state_init_deployment_display(0x100, 0xf0, 0x200, 0, 0, 0);
    fade_enabled = g_main_system_go_straight_to_battle;
    if (script_get_variable(EVENT_SCRIPT_VAR_SAVE_IN_PROGRESS) != 0) {
        fade_enabled = 0;
    }
    g_battle_deployment_skipped = menu_init_attack_resources_and_threads();
    main_gfx_build_now_loading_center(g_battle_deployment_skipped);
    map_id = g_map_id = script_get_variable(EVENT_SCRIPT_VAR_CURRENT_MAP);
    g_battle_state_map_init_step = 0;
    g_map_data_load_complete = 0;

    SetPolyF4(&g_battle_screen_fade_polygons[0]);
    setRGB0(&g_battle_screen_fade_polygons[0], 0, 0, 0);
    setXY4(&g_battle_screen_fade_polygons[0], 0x80, 0, 0x180, 0, 0, 0x100, 0x180, 0x100);
    SetSemiTrans(&g_battle_screen_fade_polygons[0], 1);
    g_battle_screen_fade_polygons[1] = g_battle_screen_fade_polygons[0];
    g_battle_gfx_screen_modulation_polygons[0] = g_battle_screen_fade_polygons[0];
    g_battle_gfx_screen_modulation_polygons[1] = g_battle_screen_fade_polygons[0];
    SetDrawMode(&g_battle_screen_fade_draw_modes[0], 0, 0, 0x40, &window);
    g_battle_screen_fade_draw_modes[1] = g_battle_screen_fade_draw_modes[0];
    g_battle_gfx_screen_modulation_draw_modes[0] = g_battle_screen_fade_draw_modes[0];
    g_battle_gfx_screen_modulation_draw_modes[1] = g_battle_screen_fade_draw_modes[0];

    ((P_TAG*)&banner[0])->len = 9;
    banner[0].code = 0x2c;
    setRGB0(&banner[0], 0x80, 0x80, 0x80);
    setXY4(&banner[0], 0x80, 0x74, 0x17f, 0x74, 0x80, 0x8c, 0x17f, 0x8c);
    setUV4(&banner[0], 0, 0, 0xff, 0, 0, 0x18, 0xff, 0x18);
    SetSemiTrans(&banner[0], 0);
    banner[0].tpage = GetTPage(0, 0, 0x380, 0x100);
    banner[0].clut = GetClut(0x380, 0x11f);
    banner[1] = banner[0];
    SetDispMask(1);

    fade = 0xf8;
    if (fade_enabled != 0) {
        do {
            main_gfx_swap_and_clear_otag();
            (g_battle_screen_fade_polygons + g_main_gfx_screen_polarity)->r0 = fade;
            (g_battle_screen_fade_polygons + g_main_gfx_screen_polarity)->g0 = fade;
            (g_battle_screen_fade_polygons + g_main_gfx_screen_polarity)->b0 = fade;
            AddPrim(main_gfx_get_otag(), &g_battle_screen_fade_polygons[g_main_gfx_screen_polarity]);
            AddPrim(main_gfx_get_otag(), &g_battle_screen_fade_draw_modes[g_main_gfx_screen_polarity]);
            AddPrim(main_gfx_get_otag(), &banner[g_main_gfx_screen_polarity]);
            g_main_gfx_screen_polarity = g_main_gfx_screen_polarity == 0;
            g_main_system_battle_frame_hsync_count
                = battle_state_sync_and_submit_deployment_frame(main_gfx_get_otag() + 0x17f);
            fade -= 8;
        } while (fade >= 0);
    }

    banner_p = banner;
    fade = 0xff;
    while (battle_state_update_deployment_controller_input() != -1) {
        main_gfx_swap_and_clear_otag();
        if (fade_enabled != 0) {
            AddPrim(main_gfx_get_otag(), &banner_p[g_main_gfx_screen_polarity]);
        } else {
            main_gfx_add_now_loading_to_otag(main_gfx_get_otag());
        }
        g_battle_state_map_init_step = map_init_units_sprites_event_and_music(map_id, g_battle_state_map_init_step,
            script_is_deployment_running(main_gfx_get_otag(), g_input_current_buttons));
        if (g_battle_state_map_init_step == 0xd) {
            battle_gfx_extract_deployed_unit_palettes();
            action_set_casting_unit_id_ff_and_init();
            g_battle_screen_fade_intensity = fade;
            /* Not `break`: jump optimization would move this exit block
             * past the loop end, which the target keeps inline. */
            goto done;
        }
        battle_state_deployment_frame_stub();
        (g_battle_screen_fade_polygons + g_main_gfx_screen_polarity)->r0 = fade;
        (g_battle_screen_fade_polygons + g_main_gfx_screen_polarity)->g0 = fade;
        (g_battle_screen_fade_polygons + g_main_gfx_screen_polarity)->b0 = fade;
        AddPrim(main_gfx_get_otag(), &g_battle_screen_fade_polygons[g_main_gfx_screen_polarity]);
        AddPrim(main_gfx_get_otag(), &g_battle_screen_fade_draw_modes[g_main_gfx_screen_polarity]);
        g_main_gfx_screen_polarity = g_main_gfx_screen_polarity == 0;
        g_main_system_battle_frame_hsync_count
            = battle_state_sync_and_submit_deployment_frame(main_gfx_get_otag() + 0x17f);
        main_file_poll_load(&g_main_file_cd_state);
        main_system_frame_stub();
    }
    fade = 0;
done:
    if (fade_enabled != 0) {
        fade = 0;
        do {
            if (fade >= 0xf8) {
                fade = 0xff;
            }
            main_gfx_swap_and_clear_otag();
            (g_battle_screen_fade_polygons + g_main_gfx_screen_polarity)->r0 = fade;
            (g_battle_screen_fade_polygons + g_main_gfx_screen_polarity)->g0 = fade;
            (g_battle_screen_fade_polygons + g_main_gfx_screen_polarity)->b0 = fade;
            AddPrim(main_gfx_get_otag(), &g_battle_screen_fade_polygons[g_main_gfx_screen_polarity]);
            AddPrim(main_gfx_get_otag(), &g_battle_screen_fade_draw_modes[g_main_gfx_screen_polarity]);
            AddPrim(main_gfx_get_otag(), &banner[g_main_gfx_screen_polarity]);
            g_main_gfx_screen_polarity = g_main_gfx_screen_polarity == 0;
            g_main_system_battle_frame_hsync_count
                = battle_state_sync_and_submit_deployment_frame(main_gfx_get_otag() + 0x17f);
            fade += 8;
        } while (fade < 0x100);
    }
}

/* Run deployment and advance the battle state one rendered frame at a time. */
void battle_state_run_game_loop(void) {
    battle_gfx_vram_slot_t* timer_field;
    SVECTOR* camera_integer;
    VECTOR* camera_fixed;
    s32 frame_measurement;
    s32 empty_timer;
    s32 timer;
    s32 camera_component;
    s32 init_offset;
    s32 i;
    s32 timer_offset;

    battle_gfx_init_render_buffers();
    battle_gfx_init_render_state();
    effect_init_system();

    /* A reverse slot-index loop changes the reset's original bytes. */
    init_offset = 0x196b0;
    do {
        g_battle_gfx_spritesheet_ids_by_vram_slot[init_offset] = 0;
        init_offset -= 0x32d6;
    } while (init_offset >= 0);

    camera_integer = &g_camera_integer_coords;
    camera_fixed = &g_camera_current_real_coords;

    do {
        battle_state_run_deployment();
        g_battle_menu_status_enabled = 0;
        g_battle_cursor_input_mode = g_main_game_options.fields.cursor_movement;

        for (;;) {
            battle_state_update_controller_input();
            battle_gfx_init_render_frame();
            SetGeomScreen(0x200);
            main_gfx_swap_and_clear_otag();
            camera_update_vector_from_game_state();

            camera_component = -camera_fixed->vx;
            if (camera_component < 0) {
                camera_component += 0xfff;
            }
            camera_integer->vx = camera_component >> 12;
            camera_component = -camera_fixed->vy;
            camera_integer->vy = camera_component / ONE;
            camera_integer->vz = -camera_fixed->vz / ONE;

            camera_update_matrices(
                &g_camera_matrix, &g_camera_render_state, &g_camera_current_screen_offset, &g_camera_zoom);
            map_draw_mesh_and_weather(&g_camera_matrix);
            if (effect_update_cycle() != 0) {
                g_battle_state_animation_continue_check = 2;
            } else {
                g_battle_state_animation_continue_check = 0;
            }

            switch (g_battle_game_state) {
            default:
            case BATTLE_GAME_STATE_FREE_CURSOR:
                battle_state_handle_default_state();
                break;
            case BATTLE_GAME_STATE_FREE_CURSOR_HELP:
                battle_state_handle_free_cursor_help_state();
                break;
            case BATTLE_GAME_STATE_HIGHLIGHT_UNITS:
                battle_state_handle_highlight_units_state();
                break;
            case BATTLE_GAME_STATE_OPEN_ACTION_MENUS:
                battle_state_handle_open_action_menus_state();
                break;
            case BATTLE_GAME_STATE_IDLING_ACTION_MENUS:
                battle_state_handle_idling_action_menus_state();
                break;
            case BATTLE_GAME_STATE_MENU_TO_TARGETING:
                battle_state_handle_menu_to_targeting_state();
                break;
            case BATTLE_GAME_STATE_ACTION_HELP_MENU:
                battle_state_handle_action_help_menu_state();
                break;
            case BATTLE_GAME_STATE_DISPLAY_MOVE_AREA:
                battle_state_handle_display_move_area_state();
                break;
            case BATTLE_GAME_STATE_MINI_MENU:
                battle_state_handle_mini_menu_state();
                break;
            case BATTLE_GAME_STATE_MINI_MENU_HELP:
                battle_state_handle_mini_menu_help_state();
                break;
            case BATTLE_GAME_STATE_ACTIVE_TURN:
                battle_state_handle_active_turn_state();
                break;
            case BATTLE_GAME_STATE_AFTER_COMMAND:
                battle_state_handle_after_command_state();
                break;
            case BATTLE_GAME_STATE_JP_EXP_GAIN:
                battle_state_handle_jp_exp_gain_state();
                break;
            case BATTLE_GAME_STATE_CHANGE_TURN:
                battle_state_handle_change_turn_state();
                break;
            case BATTLE_GAME_STATE_STATUS_EXECUTE:
                battle_state_handle_status_execute_state();
                break;
            case BATTLE_GAME_STATE_UNIT_MOVE:
                battle_state_handle_unit_move_state();
                break;
            case BATTLE_GAME_STATE_CLOSE_MOVE_HELP:
                battle_state_handle_close_move_help_state();
                break;
            case BATTLE_GAME_STATE_MOVE_RANGE_EXCEPTION:
                battle_state_handle_move_range_exception_state();
                break;
            case BATTLE_GAME_STATE_ILLEGAL_MOVE_MENU:
                battle_state_handle_illegal_move_menu_state();
                break;
            case BATTLE_GAME_STATE_UNIT_MOVING_SETUP:
                battle_state_handle_unit_moving_setup_state();
                break;
            case BATTLE_GAME_STATE_UNIT_MOVING:
                battle_state_handle_unit_moving_state();
                break;
            case BATTLE_GAME_STATE_MOVE_CONFIRM_MENU:
                battle_state_handle_move_confirm_menu_state();
                break;
            case BATTLE_GAME_STATE_WAIT_DIRECTION:
                battle_state_handle_wait_direction_state();
                break;
            case BATTLE_GAME_STATE_WAIT_MENU:
                battle_state_handle_wait_menu_state();
                break;
            case BATTLE_GAME_STATE_CRYSTAL_LEARN:
                battle_state_handle_crystal_learn_state();
                break;
            case BATTLE_GAME_STATE_ACTION_EXECUTE_SETUP:
                battle_state_handle_action_execute_setup_state();
                break;
            case BATTLE_GAME_STATE_TARGETING_RANGE:
                battle_state_handle_targeting_range_state();
                break;
            case BATTLE_GAME_STATE_ILLEGAL_RANGE:
                battle_state_handle_illegal_range_state();
                break;
            case BATTLE_GAME_STATE_ABILITY_PREVIEW_HANDLING:
                battle_state_handle_ability_preview_state();
                break;
            case BATTLE_GAME_STATE_ABILITY_PREVIEW_HELP:
                battle_state_handle_ability_preview_help_state();
                break;
            case BATTLE_GAME_STATE_CONFIRM_ACTION:
                battle_state_handle_confirm_action_state();
                break;
            case BATTLE_GAME_STATE_PRE_ATTACK_ANIMATION:
                battle_state_handle_pre_attack_animation_state();
                break;
            case BATTLE_GAME_STATE_ACTION_CAST:
                battle_state_handle_action_cast_state();
                break;
            case BATTLE_GAME_STATE_COMMENCE_ATTACK_PHASE:
                battle_state_handle_commence_attack_phase_state();
                break;
            case BATTLE_GAME_STATE_ACTION_ANNOUNCEMENT:
                battle_state_handle_action_announcement_state();
                break;
            case BATTLE_GAME_STATE_OPEN_SP2_FILES:
                battle_state_handle_open_sp2_files_state();
                break;
            case BATTLE_GAME_STATE_START_EFFECT_FILE_OPEN:
                battle_state_handle_start_effect_file_open_state();
                break;
            case BATTLE_GAME_STATE_SECONDARY_EFFECT:
                battle_state_handle_secondary_effect_state();
                break;
            case BATTLE_GAME_STATE_ACTION_EXECUTE:
                battle_state_update_action_execute_mode();
                break;
            case BATTLE_GAME_STATE_BATTLE_MESSAGE_DISPLAY:
                battle_state_handle_battle_message_display_state();
                break;
            case BATTLE_GAME_STATE_RESUME_ATTACK_PHASE:
                battle_state_handle_resume_attack_phase_state();
                break;
            case BATTLE_GAME_STATE_LEARN_ABILITY_ON_HIT:
                battle_state_handle_learn_ability_on_hit_state();
                break;
            case BATTLE_GAME_STATE_DEEP_DUNGEON_MESH_LOAD:
                battle_state_handle_deep_dungeon_mesh_load_state();
                break;
            case BATTLE_GAME_STATE_DEEP_DUNGEON_MESH_FINISH:
                battle_state_handle_deep_dungeon_mesh_finish_state();
                break;
            case BATTLE_GAME_STATE_TARGET_SELECT_START:
                battle_state_handle_target_select_start_state();
                break;
            case BATTLE_GAME_STATE_TARGET_SELECT:
                battle_state_handle_target_select_state();
                break;
            case BATTLE_GAME_STATE_TARGET_SELECT_DENIED:
                battle_state_handle_target_select_denied_state();
                break;
            case BATTLE_GAME_STATE_TARGET_SELECT_CONFIRM:
                battle_state_handle_target_select_confirm_state();
                break;
            case BATTLE_GAME_STATE_TARGET_DISPLAY_START:
                battle_state_handle_target_display_start_state();
                break;
            case BATTLE_GAME_STATE_TARGET_DISPLAY:
                battle_state_handle_target_display_state();
                break;
            case BATTLE_GAME_STATE_EFFECT:
                battle_state_handle_effect_state();
                break;
            case BATTLE_GAME_STATE_EVENT:
                battle_state_update_event_mode();
                break;
            case BATTLE_GAME_STATE_MAP_JUMPING_OUT:
                battle_state_handle_map_jumping_out_state();
                break;
            case BATTLE_GAME_STATE_MAP_INITIALIZE:
                battle_state_handle_map_init_state();
                break;
            case BATTLE_GAME_STATE_MAP_JUMPING_IN:
                battle_state_handle_map_jumping_in_state();
                break;
            case BATTLE_GAME_STATE_MAP_JUMPING_OUT_2:
                battle_state_handle_change_map_jumping_out_state();
                break;
            case BATTLE_GAME_STATE_MAP_INITIALIZE_2:
                battle_state_handle_change_map_init_state();
                break;
            case BATTLE_GAME_STATE_MAP_JUMPING_IN_2:
                battle_state_handle_change_map_jumping_in_state();
                break;
            case BATTLE_GAME_STATE_CLOSE_BATTLE:
                battle_state_handle_close_battle_state();
                break;
            }

            camera_update_map_pan();
            camera_update_rotation();
            camera_update_zoom();
            camera_update_tilt();
            map_update_animations();
            main_sound_update_tunes();

            if (g_battle_game_state == BATTLE_GAME_STATE_CLOSE_BATTLE) {
                break;
            }
            main_file_poll_load(&g_main_file_cd_state);
            g_main_gfx_screen_polarity = !g_main_gfx_screen_polarity;
            g_frame_counter++;
            map_upload_clut_rows();
            frame_measurement = battle_state_sync_frame(main_gfx_get_otag() + 0x17f);
            g_main_system_battle_frame_hsync_count = frame_measurement;
            i = 0;
            if (g_main_system_battle_frame_hsync_count_max < frame_measurement) {
                g_main_system_battle_frame_hsync_count_max = frame_measurement;
            }
            empty_timer = BATTLE_GFX_VRAM_SLOT_BUFFER_EVTCHR;
            timer_field = g_battle_gfx_vram_slots;
            timer_offset = 0;
            do {
                /* The byte offset preserves the target's separate indexed load. */
                timer = ((battle_gfx_vram_slot_t*)((u8*)g_battle_gfx_vram_slots + timer_offset))->image_buffer_state;
                timer_offset += sizeof(*timer_field);
                if (timer != 0 && timer != empty_timer) {
                    timer_field->image_buffer_state = timer - 1;
                }
                i++;
                timer_field++;
            } while (i < 2);

            if (g_battle_game_state == BATTLE_GAME_STATE_EFFECT
                || g_battle_game_state == BATTLE_GAME_STATE_ACTION_EXECUTE) {
                if (g_main_system_animation_speed == 1) {
                    g_battle_frame_pacing_timer = 0;
                } else if (g_battle_frame_pacing_suppressed == 0) {
                    if (g_main_system_battle_frame_hsync_count >= 0x301) {
                        g_battle_frame_pacing_timer = 0x1e;
                    } else if (g_main_system_battle_frame_hsync_count >= 0x201 && g_battle_frame_pacing_timer < 0x0f) {
                        g_battle_frame_pacing_timer = 0x0f;
                    }
                }
            }
            g_battle_frame_pacing_suppressed = 0;
            main_system_frame_stub();
        }

        battle_state_stop_map_animations();
        battle_unit_clear_all();
    } while (g_main_system_game_flow_state == MAIN_GAME_FLOW_BATTLE);

    DrawSync(0);
    VSync(0);
    DrawSync(0);
    VSync(0);
}

battle_unit_t* battle_unit_get_casting(void) {
    battle_unit_t* unit;

    for (unit = g_battle_unit_list_head; unit != 0; unit = unit->previous) {
        if (unit->runtime_id == g_battle_casting_runtime_id) {
            return unit;
        }
    }
    return 0;
}

battle_unit_t* battle_unit_get_source(void) {
    battle_unit_t* unit;
    s32 source_id;

    unit = g_battle_unit_list_head;
    if (unit != 0) {
        source_id = g_battle_unit_source_runtime_id;
        for (; unit != 0; unit = unit->previous) {
            if (unit->runtime_id == source_id) {
                return unit;
            }
        }
    }
    return 0;
}

/* The first record in the runtime unit list standing on a tile, whatever its state
 * (crystals, treasures and records without battle stats included), or 0. */
battle_unit_t* battle_unit_find_runtime_at_tile(s32 map_x, s32 map_y, s32 map_z) {
    battle_unit_t* unit;

    unit = g_battle_unit_list_head;
    while (unit != 0) {
        if (unit->map_x == map_x && unit->map_y == map_y && unit->map_z == map_z) {
            return unit;
        }
        unit = unit->previous;
    }
    return 0;
}

/* Returns the unit the cursor selects on a tile: one with battle stats in an
 * occupied slot that is not a crystal, a treasure or in the air from Jump.
 *
 * Among several, the source unit (g_battle_unit_source_runtime_id) is preferred,
 * then a rider over its mount, then the first in list order. Returns 0 for none.
 * The candidate array holds 16 entries and is filled without a bound check. */
battle_unit_t* battle_unit_find_selectable_at_tile(s32 map_x, s32 map_y, s32 map_z) {
    battle_unit_t* matches[BATTLE_UNIT_RUNTIME_SLOT_COUNT];
    battle_unit_t* unit;
    s32 count;
    s32 index;

    count = 0;
    unit = g_battle_unit_list_head;
    while (unit != 0) {
        if ((unit->map_x == map_x) && (unit->map_y == map_y) && (unit->map_z == map_z) && (unit->stats != 0)
            && (unit->stats->entd_slot != BATTLE_ENTD_SLOT_NONE)
            && ((unit->status_flags_5_6
                    & (BATTLE_UNIT_STATUS_CRYSTAL | BATTLE_UNIT_STATUS_TREASURE | BATTLE_UNIT_STATUS_JUMP))
                == 0)) {
            matches[count++] = unit;
        }
        unit = unit->previous;
    }
    if (count != 0) {
        if (count != 1) {
            for (index = 0; index < count; index++) {
                if (matches[index]->runtime_id == g_battle_unit_source_runtime_id) {
                    return matches[index];
                }
            }
            for (index = 0; index < count; index++) {
                if (matches[index]->mount_state == BATTLE_UNIT_MOUNT_STATE_RIDER) {
                    return matches[index];
                }
            }
        }
        return matches[0];
    }
    return 0;
}

/* The crystal or treasure lying on a tile, or 0. Among several, the source unit
 * (g_battle_unit_source_runtime_id) is preferred, then the first in list order. The
 * candidate array holds 16 entries and is filled without a bound check. */
battle_unit_t* battle_unit_get_crystal_or_treasure_at_map_coords(s32 map_x, s32 map_y, s32 map_z) {
    battle_unit_t* matches[BATTLE_UNIT_RUNTIME_SLOT_COUNT];
    battle_unit_t* unit;
    s32 count;
    s32 index;

    count = 0;
    unit = g_battle_unit_list_head;
    while (unit != 0) {
        if ((unit->map_x == map_x) && (unit->map_y == map_y) && (unit->map_z == map_z) && (unit->stats != 0)
            && (unit->stats->entd_slot != BATTLE_ENTD_SLOT_NONE)
            && ((unit->status_flags_5_6 & (BATTLE_UNIT_STATUS_CRYSTAL | BATTLE_UNIT_STATUS_TREASURE)) != 0)) {
            matches[count++] = unit;
        }
        unit = unit->previous;
    }
    if (count != 0) {
        for (index = 0; index < count; index++) {
            if (matches[index]->runtime_id == g_battle_unit_source_runtime_id) {
                return matches[index];
            }
        }
        return matches[0];
    }
    return 0;
}

/* Find the other Chocobo renderer record occupying a map tile.
 *
 * Mounting logic excludes the acting and casting records when multiple
 * Chocobo records share the coordinates.
 */
battle_unit_t* battle_unit_find_other_chocobo_at_map_coords(s32 x, s32 y, s32 z) {
    battle_unit_t* unit;
    battle_unit_t** cursor;
    s32 count;
    s32 id;
    s32 spritesheet_id;
    battle_unit_t* list[BATTLE_UNIT_RUNTIME_SLOT_COUNT];
    battle_unit_t* result;
    s32 index;

    unit = g_battle_unit_list_head;
    count = 0;
    if (unit != 0) {
        spritesheet_id = BATTLE_SPRITESHEET_ID_CHOCOBO;
        cursor = list;
        for (; unit != 0; unit = unit->previous) {
            if (unit->map_x == x && unit->map_y == y && unit->map_z == z && unit->spritesheet_id == spritesheet_id) {
                *cursor = unit;
                cursor += 1;
                count += 1;
            }
        }
    }
    if (count != 0) {
        for (index = 0; index < count; index++) {
            result = list[index];
            id = result->runtime_id;
            if (id != g_battle_unit_source_runtime_id && id != g_battle_casting_runtime_id) {
                return result;
            }
        }
    }
    return 0;
}

/* Finds another unit standing on tile (x, y, z): one with battle stats in an
 * occupied slot, other than self and, when self is mounted or ridden, other than
 * its mount partner.
 *
 * Among several, the source unit (g_battle_unit_source_runtime_id) is preferred,
 * then the first in list order. Returns 0 for none. The candidate array holds 16
 * entries and is filled without a bound check. */
battle_unit_t* battle_unit_find_overlapping_at_tile(battle_unit_t* self, s32 x, s32 y, s32 z) {
    battle_unit_t* list[BATTLE_UNIT_RUNTIME_SLOT_COUNT];
    battle_unit_t* unit;
    unit_stats_t* stats;
    s32 count;
    s32 i;

    count = 0;
    for (unit = g_battle_unit_list_head; unit != 0; unit = unit->previous) {
        if (unit != self && unit->map_x == x && unit->map_y == y && unit->map_z == z) {
            stats = unit->stats;
            if (stats != 0 && stats->entd_slot != BATTLE_ENTD_SLOT_NONE) {
                if (self->mount_state == BATTLE_UNIT_MOUNT_STATE_NONE
                    || unit->runtime_id != self->mount_partner_runtime_id) {
                    list[count] = unit;
                    count += 1;
                }
            }
        }
    }
    if (count != 0) {
        for (i = 0; i < count; i++) {
            if (list[i]->runtime_id == g_battle_unit_source_runtime_id) {
                return list[i];
            }
        }
        return list[0];
    }
    return 0;
}

/* The runtime (display) record whose runtime_id matches the requested ID, or 0. */
battle_unit_t* battle_unit_get_by_runtime_id(u16 runtime_id) {
    battle_unit_t* unit;

    unit = g_battle_unit_list_head;
    while (unit != 0) {
        if (unit->runtime_id == runtime_id) {
            return unit;
        }
        unit = unit->previous;
    }
    return 0;
}

/* The runtime (display) record whose battle stats occupy unit slot battle_id, or 0. */
battle_unit_t* battle_unit_get_by_battle_id(u16 battle_id) {
    battle_unit_t* unit;

    unit = g_battle_unit_list_head;
    while (unit != 0) {
        if ((unit->stats != 0) && (unit->stats->battle_id == battle_id)) {
            return unit;
        }
        unit = unit->previous;
    }
    return 0;
}

s32 battle_unit_count_crystals(void) {
    battle_unit_t* unit;
    s32 count;

    count = 0;
    unit = g_battle_unit_list_head;
    while (unit != 0) {
        if (unit->spritesheet_id == BATTLE_SPRITESHEET_ID_CRYSTAL) {
            count++;
        }
        unit = unit->previous;
    }
    return count;
}

/* Moves a unit's record to the head of the runtime unit list, so list walks reach it
 * first. Returns 1 when moved, 0 when no record has the runtime id. */
s32 battle_unit_move_to_head(u32 runtime_id) {
    battle_unit_t* unit;
    battle_unit_t* iterator;
    battle_unit_t* head;

    unit = battle_unit_get_by_runtime_id(runtime_id & 0xffff);
    if (unit != 0) {
        /* The 32-bit list-head pointer serves as a sentinel record: its previous field
         * (0x000) is the head link. */
        for (iterator = (battle_unit_t*)&g_battle_unit_list_head; iterator != 0; iterator = iterator->previous) {
            if (iterator->previous == unit) {
                iterator->previous = unit->previous;
                head = (battle_unit_t*)&g_battle_unit_list_head;
                unit->previous = head->previous;
                head->previous = unit;
                return 1;
            }
        }
        return 0;
    }
    return 0;
}
