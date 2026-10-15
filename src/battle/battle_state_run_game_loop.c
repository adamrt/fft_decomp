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
