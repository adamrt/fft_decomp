/* Gfx unit palettes and shadows. Its start is a cut chosen by topic; its end is a cut chosen by topic. */

#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_camera.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_script.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "fft/script.h"
#include "psx/gte_inline.h"
#include "psx/gte_inline_split.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Uploads the fixed CLUTs of the sprite row: 1 and 2 the deployed-unit palettes,
 * 3 the wait-direction arrow, 4 the map selection cursor, 5 the shadow and 7 the
 * status bubble. */
void battle_gfx_extract_deployed_unit_palettes(void) {
    RECT rect; /* never used; retail reserves its 8 bytes */

    battle_map_load_palette_data(&g_battle_deployed_unit_palettes[0], MAP_CLUT_ROW_SPRITES, 1, 0);
    battle_map_load_palette_data(&g_battle_deployed_unit_palettes[1], MAP_CLUT_ROW_SPRITES, 2, 0);
    battle_map_load_palette_data(&g_battle_gfx_wait_direction_arrow_palette, MAP_CLUT_ROW_SPRITES, 3, 0);
    battle_map_load_palette_data(&g_battle_gfx_map_selection_cursor_palette, MAP_CLUT_ROW_SPRITES, 4, 0);
    battle_map_load_palette_data(&g_battle_gfx_shadow_palette, MAP_CLUT_ROW_SPRITES, 5, 0);
    battle_map_load_palette_data(&g_battle_gfx_status_bubble_palette, MAP_CLUT_ROW_SPRITES, 7, 0);
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
        g_battle_state_map_init_step = battle_map_init_units_sprites_event_and_music(map_id,
            g_battle_state_map_init_step, script_is_deployment_running(main_gfx_get_otag(), g_input_current_buttons));
        if (g_battle_state_map_init_step == 0xd) {
            battle_gfx_extract_deployed_unit_palettes();
            battle_action_set_casting_unit_id_ff_and_init();
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
    battle_effect_init_system();

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
            battle_camera_update_vector_from_game_state();

            camera_component = -camera_fixed->vx;
            if (camera_component < 0) {
                camera_component += 0xfff;
            }
            camera_integer->vx = camera_component >> 12;
            camera_component = -camera_fixed->vy;
            camera_integer->vy = camera_component / ONE;
            camera_integer->vz = -camera_fixed->vz / ONE;

            battle_camera_update_matrices(
                &g_camera_matrix, &g_camera_render_state, &g_camera_current_screen_offset, &g_camera_zoom);
            battle_map_draw_mesh_and_weather(&g_camera_matrix);
            if (battle_effect_update_cycle() != 0) {
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

            battle_camera_update_map_pan();
            battle_camera_update_rotation();
            battle_camera_update_zoom();
            battle_camera_update_tilt();
            battle_map_update_animations();
            main_sound_update_tunes();

            if (g_battle_game_state == BATTLE_GAME_STATE_CLOSE_BATTLE) {
                break;
            }
            main_file_poll_load(&g_main_file_cd_state);
            g_main_gfx_screen_polarity = !g_main_gfx_screen_polarity;
            g_frame_counter++;
            battle_map_upload_clut_rows();
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

s32 battle_gfx_open_sp2_file(battle_unit_t* unit, u32* destination) {
    s32 spritesheet_id;
    u8 animation_id;
    s32 index;
    s32 lba;
    s32 byte_length;

    spritesheet_id = g_battle_gfx_spritesheet_slots[unit->spritesheet_vram_slot].spritesheet_id;
    animation_id = g_battle_ability_animation_data[unit->sp2_ability_id].attack_animation;
    if (spritesheet_id == BATTLE_SPRITESHEET_ID_STEEL_GIANT) {
        index = animation_id - 0x73;
        lba = g_battle_gfx_worker8_sp2_files[index].sector;
        if (lba == 0) {
            return -1;
        }
        if (animation_id >= (u32)unit->animation.seq->first_sp2_sequence >> 1) {
            byte_length = g_battle_gfx_worker8_sp2_files[index].size;
            return main_file_call_build_header(lba, byte_length, destination) != 0;
        }
    } else {
        lba = g_battle_gfx_sp2_files[spritesheet_id].sector;
        if (lba == 0) {
            return -1;
        }
        if (animation_id >= (u32)unit->animation.seq->first_sp2_sequence >> 1) {
            byte_length = g_battle_gfx_sp2_files[spritesheet_id].size;
            return main_file_call_build_header(lba, byte_length, destination) != 0;
        }
    }
    return -1;
}

/* Free the EVTCHR slots whose page holds the unit's SP2 image. */
void battle_gfx_invalidate_sp2_vram_slot(battle_unit_t* unit) {
    u32 invalid;
    battle_gfx_vram_slot_t* slot;
    s32 count;

    count = 0;
    invalid = BATTLE_GFX_VRAM_SLOT_OWNER_NONE;
    slot = g_battle_gfx_vram_slots;
    for (; count < 2; count++, slot++) {
        u32 descriptor = slot->owner;

        if (((descriptor & BATTLE_GFX_VRAM_SLOT_OWNER_ID_MASK) == unit->runtime_id)
            && ((descriptor & BATTLE_GFX_VRAM_SLOT_OWNER_SP2) != 0)) {
            slot->owner = invalid;
        }
    }
}

/* The next nibble of the compressed spritesheet stream, the high nibble of each
 * byte first. */
u32 battle_gfx_read_compressed_nibble(void) {
    u32 nibble;
    u8 value;

    if (g_battle_gfx_compressed_nibble_phase == 0) {
        nibble = g_battle_gfx_compressed_data[g_battle_gfx_compressed_offset] >> 4;
    } else {
        value = g_battle_gfx_compressed_data[g_battle_gfx_compressed_offset];
        g_battle_gfx_compressed_offset++;
        nibble = value & 0xf;
    }
    g_battle_gfx_compressed_nibble_phase ^= 1;
    return nibble;
}

/* Expands a nibble-RLE attack spritesheet into 0x6400 bytes of 4bpp pixels.
 *
 * A zero nibble introduces a run of transparent pixels whose length follows as
 * one nibble, or as two or three nibbles after the markers 7 and 8. */
void battle_gfx_decompress_attack_spritesheet(const u8* source, u8* destination) {
    s32 high;
    s32 count;
    s32 value;
    s32 i;
    s32 size;
    u8* out;

    high = 0;
    count = 0;
    size = 0x6400;
    out = destination;
    g_battle_gfx_compressed_data = source;
    g_battle_gfx_compressed_offset = 0;
    g_battle_gfx_compressed_nibble_phase = 0;
    while (1) {
        value = battle_gfx_read_compressed_nibble();
        if (value == 0) {
            value = battle_gfx_read_compressed_nibble();
            switch (value) {
            case 0:
                value = battle_gfx_read_compressed_nibble();
                break;
            case 7:
                value = battle_gfx_read_compressed_nibble();
                value |= battle_gfx_read_compressed_nibble() << 4;
                break;
            case 8:
                value = battle_gfx_read_compressed_nibble();
                value |= battle_gfx_read_compressed_nibble() << 4;
                value |= battle_gfx_read_compressed_nibble() << 8;
                break;
            }
            for (i = 0; i < value; i++) {
                if (high == 0) {
                    destination[count] = 0;
                } else {
                    out++;
                    destination[count] &= 0xf0;
                    count++;
                    if (count == size) {
                        return;
                    }
                }
                high ^= 1;
            }
        } else {
            if (high == 0) {
                *out = value << 4;
            } else {
                count++;
                *out |= value;
                out++;
                if (count == size) {
                    return;
                }
            }
            high ^= 1;
        }
    }
}

void battle_gfx_init_render_state(void) {
    g_battle_gfx_numeric_display_matrix.m[2][2] = ONE;
    g_battle_gte_rtps_vxy0.vz = 0x200;
    g_battle_gte_rtpt_vxy2.vz = 0x200;
    g_battle_gte_rtpt_vxy1.vz = 0x200;
    g_battle_gte_rtpt_vxy0.vz = 0x200;
    g_battle_gfx_numeric_display_matrix.m[2][1] = 0;
    g_battle_gfx_numeric_display_matrix.m[2][0] = 0;
    g_battle_gfx_numeric_display_matrix.m[1][2] = 0;
    g_battle_gfx_numeric_display_matrix.m[0][2] = 0;
    g_battle_gfx_numeric_display_matrix.t[2] = 0;
    g_battle_gfx_counter = 0;
    g_battle_gfx_previous_counter = 0;
    g_battle_unknown_800b6290 = 0;
    g_battle_unknown_800b628c = 0;
    /* The in-use 32-bit value just past the last g_battle_units record. */
    g_battle_gfx_sprite_prim_pool = (POLY_FT4*)&g_battle_unit_slot_states[BATTLE_UNIT_RUNTIME_SLOT_COUNT].in_use;
}

/* Reset a unit's own sprite display: neutral color, no flags on its eight
 * parts and none of them in use. */
battle_gfx_sprite_display_data_t* battle_gfx_init_unit_sprite_display(s32 runtime_id) {
    s32 body_part;
    battle_gfx_sprite_display_data_t* display;

    display = &g_battle_units[runtime_id].unit_sprite_display.header;
    display->color.b = 0x80;
    display->color.g = 0x80;
    display->color.r = 0x80;
    for (body_part = 7; body_part >= 0; body_part--) {
        display->parts[body_part].flags = 0;
    }
    display->part_count = 0;
    return display;
}

/* Reset the display of one of a unit's three sprite blocks: neutral color,
 * no flags on its three parts and none of them in use. */
battle_gfx_sprite_display_data_t* battle_gfx_init_weapon_sprite_display(s32 unit_index, s32 display_type) {
    s32 weapon_part;
    battle_gfx_sprite_display_data_t* display;

    display = &g_battle_units[unit_index].weapon_sprite_displays[display_type].header;
    display->color.b = 0x80;
    display->color.g = 0x80;
    display->color.r = 0x80;
    for (weapon_part = 2; weapon_part >= 0; weapon_part--) {
        display->parts[weapon_part].flags = 0;
    }
    display->part_count = 0;
    return display;
}

/* Resets one of a unit's three numeric sprite displays to neutral color and no
 * parts. A display_section outside 0..2 leaves `display` unset; the only caller
 * passes 0, 1 and 2. */
battle_gfx_sprite_display_data_t* battle_gfx_init_numeric_sprite_display(s32 unit_index, s32 display_section) {
    battle_gfx_sprite_display_data_t* display;
    s32 i;

    switch (display_section) {
    case 0:
        display = &g_battle_units[unit_index].numeric_sprite_displays[0].header;
        break;
    case 1:
        display = &g_battle_units[unit_index].numeric_sprite_displays[1].header;
        break;
    case 2:
        display = &g_battle_units[unit_index].numeric_sprite_displays[2].header;
        break;
    }

    display->color.b = 0x80;
    display->color.g = 0x80;
    display->color.r = 0x80;
    for (i = 0; i < 1; i++)
        display->parts[i].flags = 0;
    display->part_count = 0;
    return display;
}

/* Resets a unit's status bubble display to one 14 x 12 part at (0, 0xb0) of the
 * FRAME.BIN page with CLUT 7 of the sprite row, neutral color and scale ONE. */
battle_gfx_sprite_display_data_t* battle_gfx_init_status_bubble_sprite_display(s32 unit_index) {
    battle_gfx_sprite_display_data_t* display;
    s32 i;

    display = &g_battle_units[unit_index].status_bubble_sprite_display.header;
    display->color.b = 0x80;
    display->color.g = 0x80;
    display->color.r = 0x80;
    display->scale_y = ONE;
    display->scale_x = ONE;
    display->tpage = BATTLE_GFX_TPAGE_FRAME;
    display->clut = MAP_CLUT(MAP_CLUT_ROW_SPRITES, 7);
    display->rotation = 0;
    display->part_count = 1;
    for (i = 0; i < 1; i++) {
        display->parts[i].y_shift = 0;
        display->parts[i].x_shift = 0;
        display->parts[i].width = 0xe;
        display->parts[i].height = 0xc;
        display->parts[i].u = 0;
        display->parts[i].v = 0xb0;
        display->parts[i].flags = 0;
    }
    return display;
}

/* Resets a unit's item sprite display to one 16 x 16 part of the ITEM.BIN page
 * with CLUT 7 of the sprite row, neutral color and scale ONE. */
battle_gfx_sprite_display_data_t* battle_gfx_init_item_sprite_display(s32 runtime_id) {
    battle_gfx_sprite_display_data_t* display;
    s32 i;

    display = &g_battle_units[runtime_id].item_sprite_display.header;
    display->color.b = 0x80;
    display->color.g = 0x80;
    display->color.r = 0x80;
    display->scale_y = ONE;
    display->scale_x = ONE;
    display->tpage = BATTLE_GFX_TPAGE_ITEM;
    display->clut = MAP_CLUT(MAP_CLUT_ROW_SPRITES, 7);
    display->rotation = 0;
    display->part_count = 1;
    for (i = 0; i < 1; i++) {
        display->parts[i].y_shift = 0;
        display->parts[i].x_shift = 0;
        display->parts[i].width = 0x10;
        display->parts[i].height = 0x10;
        display->parts[i].flags = 0;
    }
    return display;
}

/* Empty. battle_unit_remove passes the removed unit's sprite display, a member of its
 * g_battle_units record that battle_gfx_init_unit_sprite_display handed out. */
void battle_gfx_release_unit_sprite_display_stub(battle_gfx_sprite_display_data_t* unused_display) {
}

/* Starts a frame of sprite drawing: the GTE offset is reset to (0, 0) and the
 * primitive ring index reached last frame is recorded. */
void battle_gfx_init_render_frame(void) {
    SetGeomOffset(0, 0);
    /* The target loads the counter unsigned (lhu). */
    g_battle_gfx_previous_counter = g_battle_gfx_counter;
}

/* Takes the next of the 256 primitives of g_battle_gfx_sprite_prim_pool, used as
 * a ring, and returns its index. */
s16 battle_gfx_increment_counter(void) {
    g_battle_gfx_counter = (g_battle_gfx_counter + 1) % 0x100;
    return g_battle_gfx_counter;
}

/* Queue one POLY_FT4 per sprite part in [start, end) of a sprite display.
 *
 * The display's rotation, the caller's angle and scale, and position build
 * the GTE matrix. `mode` supplies the whole-sprite flags and blend selector:
 * a flip bit mirrors each part's place about the sprite's origin, and its
 * picture unless the part's own flip bit is set too. Either one's
 * semi-transparent bit applies, and the blend bits of `mode` are added to
 * the display's texture page. A part flagged BATTLE_SPRITE_DISPLAY_NEARER is
 * queued ahead of `ot` by the camera pitch and zoom while the pitch is below
 * a quarter turn.
 *
 * The unused VECTOR reproduces the target's 16-byte locals area. The nested
 * test with its duplicated `addPrim(ot, prim)` arm is the original shape: jump
 * optimization merges the arms, but the references counted before that merge
 * give `ot` the allocation priority that places it in $s5 ahead of `display`. */
void battle_gfx_construct_polygon_data_for_units(battle_gfx_sprite_display_data_t* display, s32 end, s32 start,
    SVECTOR* position, s16 angle, s16 mode, SVECTOR* scale, u32* ot) {
    VECTOR unused;
    battle_gfx_sprite_part_display_data_t* part;
    POLY_FT4* prim;
    s32 i;
    s16 flags;
    s16 index;
    s16 x;
    s16 y;
    s16 cosine;
    s16 sine;

    cosine = rcos(angle + display->rotation);
    g_battle_gfx_numeric_display_matrix.m[0][0] = ((cosine * scale->vx) >> 12) * display->scale_x >> 12;
    g_battle_gfx_numeric_display_matrix.m[1][1] = ((cosine * scale->vy) >> 12) * display->scale_y >> 12;
    sine = rsin(angle + display->rotation);
    g_battle_gfx_numeric_display_matrix.m[0][1] = ((-sine * scale->vy) >> 12) * display->scale_y >> 12;
    g_battle_gfx_numeric_display_matrix.m[1][0] = ((sine * scale->vx) >> 12) * display->scale_x >> 12;
    g_battle_gfx_numeric_display_matrix.t[0] = position->vx;
    g_battle_gfx_numeric_display_matrix.t[1] = position->vy;
    SetRotMatrix(&g_battle_gfx_numeric_display_matrix);
    SetTransMatrix(&g_battle_gfx_numeric_display_matrix);
    for (i = start; i < end; i++) {
        part = &display->parts[i];
        flags = part->flags;
        index = battle_gfx_increment_counter();
        flags ^= mode;
        prim = g_battle_gfx_sprite_prim_pool + index;
        setlen(prim, 9);
        prim->code = 0x2c;
        if (mode & BATTLE_SPRITE_DISPLAY_HORIZONTAL_FLIP) {
            x = -part->x_shift - part->width;
        } else {
            x = part->x_shift;
        }
        if (mode & BATTLE_SPRITE_DISPLAY_VERTICAL_FLIP) {
            y = -part->y_shift - part->height;
        } else {
            y = part->y_shift;
        }
        if (flags & BATTLE_SPRITE_DISPLAY_HORIZONTAL_FLIP) {
            g_battle_gte_rtpt_vxy1.vx = g_battle_gte_rtps_vxy0.vx = x;
            g_battle_gte_rtpt_vxy0.vx = g_battle_gte_rtpt_vxy2.vx = x + part->width;
        } else {
            g_battle_gte_rtpt_vxy0.vx = g_battle_gte_rtpt_vxy2.vx = x;
            g_battle_gte_rtpt_vxy1.vx = g_battle_gte_rtps_vxy0.vx = x + part->width;
        }
        if (flags & BATTLE_SPRITE_DISPLAY_VERTICAL_FLIP) {
            g_battle_gte_rtpt_vxy2.vy = g_battle_gte_rtps_vxy0.vy = y;
            g_battle_gte_rtpt_vxy0.vy = g_battle_gte_rtpt_vxy1.vy = y + part->height;
        } else {
            g_battle_gte_rtpt_vxy0.vy = g_battle_gte_rtpt_vxy1.vy = y;
            g_battle_gte_rtpt_vxy2.vy = g_battle_gte_rtps_vxy0.vy = y + part->height;
        }
        gte_ldv3_split(&g_battle_gte_rtpt_vxy0, &g_battle_gte_rtpt_vxy1, &g_battle_gte_rtpt_vxy2);
        gte_rtpt();
        if ((flags | mode) & BATTLE_SPRITE_DISPLAY_SEMI_TRANSPARENT) {
            prim->code |= 2;
        } else {
            prim->code &= ~2;
        }
        prim->tpage = display->tpage | (mode & BATTLE_SPRITE_DISPLAY_BLEND_MASK);
        prim->clut = display->clut;
        gte_stsxy3_split(&prim->x0, &prim->x1, &prim->x2);
        gte_ldv0(&g_battle_gte_rtps_vxy0);
        gte_rtps();
        /* The retail color transfer combines red and green in one 16-bit value. */
        *(u16*)&prim->r0 = *(u16*)&display->color.r;
        prim->b0 = display->color.b;
        gte_stsxy(&prim->x3);
        prim->u0 = prim->u2 = part->u;
        prim->u1 = prim->u3 = prim->u0 + part->width;
        prim->v0 = prim->v1 = part->v;
        prim->v2 = prim->v3 = prim->v0 + part->height;
        if (flags & BATTLE_SPRITE_DISPLAY_NEARER) {
            if ((g_camera_render_state.vx & 0xfff) < 0x400) {
                addPrim(ot - ((g_camera_render_state.vx & 0xfff) - 0x12e) * g_camera_zoom.vz / 368640 - 1, prim);
            } else {
                addPrim(ot, prim);
            }
        } else {
            addPrim(ot, prim);
        }
    }
}

void battle_gfx_store_sprite_display_data(battle_gfx_sprite_display_data_t* display, s32 index, s16 x_shift,
    s16 y_shift, s16 u, s16 v, s16 width, s16 height, s16 flags) {
    battle_gfx_sprite_part_display_data_t* part;

    part = &display->parts[index];
    part->x_shift = x_shift;
    part->y_shift = y_shift;
    part->u = u;
    part->v = v;
    part->width = width;
    part->height = height;
    part->flags = flags;
}

/* Replace the two mirror bits of a sprite part (2 mirrors x, 4 mirrors y). Never called. */
void battle_gfx_set_sprite_part_flip(battle_gfx_sprite_display_data_t* display, s32 part_index, u32 flags) {
    display->parts[part_index].flags = (display->parts[part_index].flags & 0xf9) | flags;
}

/* Set or clear bit 0 of a sprite part's flags: the part is drawn semi-transparent while it is set. */
void battle_gfx_set_sprite_part_semi_transparent(
    battle_gfx_sprite_display_data_t* display, s32 part_index, s32 enabled) {
    if (enabled != 0) {
        display->parts[part_index].flags |= BATTLE_SPRITE_DISPLAY_SEMI_TRANSPARENT;
    } else {
        display->parts[part_index].flags &= ~BATTLE_SPRITE_DISPLAY_SEMI_TRANSPARENT;
    }
}

void battle_camera_calculate_relative_offset(const SVECTOR* input, SVECTOR* output, s32 offset) {
    u16 x;
    u16 z;

    output->vy = input->vy - 0xc;
    switch (g_camera_render_state.vy & 0xc00) {
    case 0:
        x = input->vx + 0xe;
        output->vx = x - offset;
        z = input->vz - 0xe;
        output->vz = z + offset;
        break;
    case 0x400:
        x = input->vx + 0xe;
        output->vx = x - offset;
        z = input->vz + 0xe;
        output->vz = z - offset;
        break;
    case 0x800:
        x = input->vx - 0xe;
        output->vx = x + offset;
        z = input->vz + 0xe;
        output->vz = z - offset;
        break;
    case 0xc00:
        x = input->vx - 0xe;
        output->vx = x + offset;
        z = input->vz - 0xe;
        output->vz = z + offset;
        break;
    }
}

/* camera_calculate_relative_offset with offset 1: the depth point of a stepping unit on a
 * tile without MAP_TILE_SURFACE_FLAG_DEPTH_BIAS. */
void battle_camera_calculate_relative_offset_by_1(const SVECTOR* input, SVECTOR* output) {
    battle_camera_calculate_relative_offset(input, output, 1);
}

/* camera_calculate_relative_offset with offset 5: the depth point of a stepping unit on a
 * tile with MAP_TILE_SURFACE_FLAG_DEPTH_BIAS. */
void battle_camera_calculate_relative_offset_by_5(const SVECTOR* input, SVECTOR* output) {
    battle_camera_calculate_relative_offset(input, output, 5);
}

/*
 * Snap a screen-space point to the 28-unit tile grid for the current camera
 * yaw, writing the result to output.
 *
 * Same shape as camera_calculate_relative_offset: each of the x and z
 * components picks up the caller's offset with a sign that depends on the yaw
 * quadrant, and the y component is a plain bias. Here each component is first
 * snapped down to a whole tile, and on the side the camera faces away from it
 * is moved on by one further tile. Callers pass an offset of 1 or 5. The
 * far_edge temporaries keep the target's add-then-subtract order; written
 * inline, GCC re-associates them into a longer sequence.
 */
void battle_camera_convert_screen_coords_by_rotation(const SVECTOR* input, SVECTOR* output, s32 offset) {
    s32 yaw;

    output->vy = input->vy - 0xb;
    yaw = g_camera_render_state.vy & 0xc00;

    switch (yaw) {
    case 0: {
        s32 far_edge = (input->vx / MAP_TILE_SIZE) * MAP_TILE_SIZE + MAP_TILE_SIZE;
        output->vx = far_edge - offset;
    }
        output->vz = (input->vz / MAP_TILE_SIZE) * MAP_TILE_SIZE + offset;
        break;
    case 0x400: {
        s32 far_edge = (input->vx / MAP_TILE_SIZE) * MAP_TILE_SIZE + MAP_TILE_SIZE;
        output->vx = far_edge - offset;
    }
        {
            s32 far_edge = (input->vz / MAP_TILE_SIZE) * MAP_TILE_SIZE + MAP_TILE_SIZE;
            output->vz = far_edge - offset;
        }
        break;
    case 0x800:
        output->vx = (input->vx / MAP_TILE_SIZE) * MAP_TILE_SIZE + offset;
        {
            s32 far_edge = (input->vz / MAP_TILE_SIZE) * MAP_TILE_SIZE + MAP_TILE_SIZE;
            output->vz = far_edge - offset;
        }
        break;
    case 0xc00:
        output->vx = (input->vx / MAP_TILE_SIZE) * MAP_TILE_SIZE + offset;
        output->vz = (input->vz / MAP_TILE_SIZE) * MAP_TILE_SIZE + offset;
        break;
    }
}

/* camera_convert_screen_coords_by_rotation with offset 1: the depth point of a standing unit,
 * a cursor or a bubble on a tile without MAP_TILE_SURFACE_FLAG_DEPTH_BIAS. */
void battle_camera_convert_screen_coords_modify_by_1(const SVECTOR* screen, SVECTOR* destination) {
    battle_camera_convert_screen_coords_by_rotation(screen, destination, 1);
}

/* camera_convert_screen_coords_by_rotation with offset 5: the depth point on a tile with
 * MAP_TILE_SURFACE_FLAG_DEPTH_BIAS. */
void battle_camera_convert_screen_coords_modify_by_5(const SVECTOR* screen, SVECTOR* destination) {
    battle_camera_convert_screen_coords_by_rotation(screen, destination, 5);
}

void battle_unit_copy_draw_coords(SVECTOR* destination, const battle_unit_t* unit) {
    /* float_bob_offset is read signed (lh) here. */
    main_util_set_svector(destination, unit->world_coords.vx + unit->event_offset.vx + unit->seq_offset.vx,
        unit->world_coords.vy + unit->event_offset.vy + unit->seq_offset.vy + (s16)unit->float_bob_offset,
        unit->world_coords.vz + unit->event_offset.vz + unit->seq_offset.vz);
}

/* Lays the unit's shadow quad on the terrain surface under the unit.
 *
 * The quad is a 20 x 20 square of world units centerd on the unit's position
 * (with its event, SEQ and float offsets), at the surface of the tile it stands
 * on: height plus depth, 12 world units per level, negative y upwards. On a
 * sloped tile (non-zero slope half-height, rising 12 units per step across the
 * 28-unit tile) the quad follows the slope at the unit's offset inside the tile:
 *
 * - an edge slope interpolates along its axis and tilts the quad by 6 units up
 *   and down across the two edges, whatever the steepness;
 * - a corner slope picks the triangle the unit stands on, by comparing its
 *   offsets from the two sides, and puts the flat quad at that triangle's height
 *   under the unit.
 *
 * battle_gfx_draw_unit_shadow calls it before projecting the quad. */
void battle_gfx_calculate_sprite_shadow_from_tile_slope(battle_unit_t* unit) {
    SVECTOR coords;
    SVECTOR unused; /* unreferenced; keeps the 0x30-byte target frame */
    map_tile_t* tile;
    SVECTOR* quad;
    u8 layer;
    u8 height;
    map_tile_depth_t depth;
    u8 slope_type;
    s32 slope;
    s16 tx;
    s16 ty;
    s32 lhs;
    s32 rhs;
    s32 rise;

    layer = unit->map_z;
    battle_unit_copy_draw_coords(&coords, unit);
    tile = battle_map_get_tile_data_pointer((coords.vx / MAP_TILE_SIZE), (coords.vz / MAP_TILE_SIZE), layer);
    depth = tile->depth_half_height;
    height = tile->height + depth.depth;
    slope_type = tile->slope_type;
    quad = unit->shadow_quad;
    quad[0].vx = coords.vx - 10;
    quad[0].vz = coords.vz + 10;
    quad[1].vx = coords.vx + 10;
    quad[1].vz = coords.vz + 10;
    quad[2].vx = coords.vx - 10;
    quad[2].vz = coords.vz - 10;
    quad[3].vx = coords.vx + 10;
    quad[3].vz = coords.vz - 10;
    quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy = -(height * 12);
    slope = depth.half_height;
    if (slope == 0) {
        return;
    }
    switch (slope_type) {
    case MAP_SLOPE_EDGE_EAST: /* edge up to east */
        tx = coords.vx % MAP_TILE_SIZE;
        rise = tx * 12;
        quad[0].vy = quad[1].vy = quad[3].vy = quad[2].vy = quad[3].vy - rise * slope / MAP_TILE_SIZE;
        quad[0].vy = quad[2].vy = quad[2].vy + 6;
        quad[1].vy = quad[3].vy = quad[3].vy - 6;
        break;
    case MAP_SLOPE_EDGE_WEST: /* edge up to west */
        tx = coords.vx % MAP_TILE_SIZE;
        rise = tx * 12;
        quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy = quad[3].vy - (slope * 12 - rise * slope / MAP_TILE_SIZE);
        quad[1].vy = quad[3].vy = quad[3].vy + 6;
        quad[0].vy = quad[2].vy = quad[2].vy - 6;
        break;
    case MAP_SLOPE_EDGE_SOUTH: /* edge up to south */
        ty = coords.vz % MAP_TILE_SIZE;
        rise = ty * 12;
        quad[0].vy = quad[2].vy = quad[3].vy = quad[1].vy = quad[3].vy - (slope * 12 - rise * slope / MAP_TILE_SIZE);
        quad[0].vy = quad[1].vy = quad[1].vy + 6;
        quad[2].vy = quad[3].vy = quad[3].vy - 6;
        break;
    case MAP_SLOPE_EDGE_NORTH: /* edge up to north */
        ty = coords.vz % MAP_TILE_SIZE;
        rise = ty * 12;
        quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy = quad[3].vy - rise * slope / MAP_TILE_SIZE;
        quad[2].vy = quad[3].vy = quad[3].vy + 6;
        quad[0].vy = quad[1].vy = quad[1].vy - 6;
        break;
    case MAP_SLOPE_LOW_CORNER_NORTH_EAST: /* low corner, north-east up */
        tx = coords.vx % MAP_TILE_SIZE;
        ty = coords.vz % MAP_TILE_SIZE;
        lhs = tx;
        rhs = ty;
        if (lhs < rhs) {
            rise = lhs * 12;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy = quad[3].vy - rise * slope / MAP_TILE_SIZE;
        } else {
            rise = rhs * 12;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy = quad[3].vy - rise * slope / MAP_TILE_SIZE;
        }
        break;
    case MAP_SLOPE_LOW_CORNER_SOUTH_EAST: /* low corner, south-east up */
        tx = coords.vx % MAP_TILE_SIZE;
        ty = coords.vz % MAP_TILE_SIZE;
        lhs = tx;
        rhs = MAP_TILE_SIZE - ty;
        if (lhs >= rhs) {
            rise = ty * 12;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy
                = quad[3].vy - (slope * 12 - rise * slope / MAP_TILE_SIZE);
        } else {
            rise = lhs * 12;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy = quad[3].vy - rise * slope / MAP_TILE_SIZE;
        }
        break;
    case MAP_SLOPE_LOW_CORNER_SOUTH_WEST: /* low corner, south-west up */
        tx = coords.vx % MAP_TILE_SIZE;
        ty = coords.vz % MAP_TILE_SIZE;
        lhs = MAP_TILE_SIZE - tx;
        rhs = MAP_TILE_SIZE - ty;
        if (lhs >= rhs) {
            rise = ty * 12;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy
                = quad[3].vy - (slope * 12 - rise * slope / MAP_TILE_SIZE);
        } else {
            rise = tx * 12;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy
                = quad[3].vy - (slope * 12 - rise * slope / MAP_TILE_SIZE);
        }
        break;
    case MAP_SLOPE_LOW_CORNER_NORTH_WEST: /* low corner, north-west up */
        tx = coords.vx % MAP_TILE_SIZE;
        ty = coords.vz % MAP_TILE_SIZE;
        lhs = MAP_TILE_SIZE - tx;
        rhs = ty;
        if (lhs >= rhs) {
            rise = rhs * 12;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy = quad[3].vy - rise * slope / MAP_TILE_SIZE;
        } else {
            rise = tx * 12;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy
                = quad[3].vy - (slope * 12 - rise * slope / MAP_TILE_SIZE);
        }
        break;
    case MAP_SLOPE_HIGH_CORNER_NORTH_EAST: /* high corner, north-east up */
        tx = coords.vx % MAP_TILE_SIZE;
        ty = coords.vz % MAP_TILE_SIZE;
        lhs = tx;
        rhs = ty;
        if (lhs >= rhs) {
            rise = lhs * 12;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy = quad[3].vy - rise * slope / MAP_TILE_SIZE;
        } else {
            rise = rhs * 12;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy = quad[3].vy - rise * slope / MAP_TILE_SIZE;
        }
        break;
    case MAP_SLOPE_HIGH_CORNER_SOUTH_EAST: /* high corner, south-east up */
        tx = coords.vx % MAP_TILE_SIZE;
        ty = coords.vz % MAP_TILE_SIZE;
        lhs = tx;
        rhs = MAP_TILE_SIZE - ty;
        if (lhs >= rhs) {
            rise = lhs * 12;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy = quad[3].vy - rise * slope / MAP_TILE_SIZE;
        } else {
            rise = ty * 12;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy
                = quad[3].vy - (slope * 12 - rise * slope / MAP_TILE_SIZE);
        }
        break;
    case MAP_SLOPE_HIGH_CORNER_SOUTH_WEST: /* high corner, south-west up */
        tx = coords.vx % MAP_TILE_SIZE;
        ty = coords.vz % MAP_TILE_SIZE;
        lhs = MAP_TILE_SIZE - tx;
        rhs = MAP_TILE_SIZE - ty;
        if (lhs < rhs) {
            rise = ty * 12;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy
                = quad[3].vy - (slope * 12 - rise * slope / MAP_TILE_SIZE);
        } else {
            rise = tx * 12;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy
                = quad[3].vy - (slope * 12 - rise * slope / MAP_TILE_SIZE);
        }
        break;
    case MAP_SLOPE_HIGH_CORNER_NORTH_WEST: /* high corner, north-west up */
        tx = coords.vx % MAP_TILE_SIZE;
        ty = coords.vz % MAP_TILE_SIZE;
        lhs = MAP_TILE_SIZE - tx;
        rhs = ty;
        if (lhs >= rhs) {
            rise = tx * 12;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy
                = quad[3].vy - (slope * 12 - rise * slope / MAP_TILE_SIZE);
        } else {
            rise = rhs * 12;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy = quad[3].vy - rise * slope / MAP_TILE_SIZE;
        }
        break;
    }
}
