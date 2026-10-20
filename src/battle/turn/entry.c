/* Load the battle and deployment screen, then run the battle state loop. */

#include "fft/battle_map.h"
#include "fft/battle_map_load.h"
#include "fft/battle_map_mesh.h"
#include "fft/battle_map_palette.h"
#include "fft/camera.h"
#include "fft/command.h"
#include "fft/cursor.h"
#include "fft/deploy.h"
#include "fft/effect_script.h"
#include "fft/fade.h"
#include "fft/file.h"
#include "fft/gfx.h"
#include "fft/input.h"
#include "fft/menu.h"
#include "fft/music.h"
#include "fft/option.h"
#include "fft/render.h"
#include "fft/script_variable.h"
#include "fft/sprite.h"
#include "fft/sprite_sheet.h"
#include "fft/state.h"
#include "fft/system.h"
#include "fft/unit_runtime.h"
#include "psx/gte_inline.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Sprite loading and rendering state. */
extern u8 g_sprite_sheet_ids_by_vram_slot[];
extern palette_t g_status_bubble_palette;
extern s32 g_system_battle_frame_hsync_count_max;
extern palette_t g_cursor_map_selection_palette;
extern palette_t g_unit_facing_wait_arrow_palette;

/* Loading BATTLE initializes the result; MAIN reads it when the battle loop returns. */
system_game_flow_e g_game_flow_state = SYSTEM_GAME_FLOW_BATTLE;

/* Uploads the fixed CLUTs of the sprite row: 1 and 2 the cursor tile glow palettes,
 * 3 the wait-direction arrow, 4 the map selection cursor, 5 the shadow and 7 the
 * status bubble. */
static void fixed_cluts_load(void) {
    RECT rect; /* never used; retail reserves its 8 bytes */

    battle_map_palette_load_data(&g_cursor_tile_glow_palettes[0], BATTLE_MAP_CLUT_ROW_SPRITES, 1, 0);
    battle_map_palette_load_data(&g_cursor_tile_glow_palettes[1], BATTLE_MAP_CLUT_ROW_SPRITES, 2, 0);
    battle_map_palette_load_data(&g_unit_facing_wait_arrow_palette, BATTLE_MAP_CLUT_ROW_SPRITES, 3, 0);
    battle_map_palette_load_data(&g_cursor_map_selection_palette, BATTLE_MAP_CLUT_ROW_SPRITES, 4, 0);
    battle_map_palette_load_data(&g_sprite_shadow_palette, BATTLE_MAP_CLUT_ROW_SPRITES, 5, 0);
    battle_map_palette_load_data(&g_status_bubble_palette, BATTLE_MAP_CLUT_ROW_SPRITES, 7, 0);
}

/* Empty. map_load_run calls it each frame while the map is still being set up. */
static void deployment_frame_stub(void) {
}

/* Load the battle map, its units, sprites, event and music, showing a loading screen.
 *
 * A battle entered directly (g_system_go_straight_to_battle, unless a saved
 * game is being resumed) shows the deployment banner: it fades in from black,
 * stays over a black screen while map initialization steps run up to step 0xd
 * (done), then fades back out to black. Otherwise the NOW LOADING display is
 * drawn instead. The fade polygons subtract their gray level from the screen, so
 * 0xff is black and 0 leaves the picture as drawn. The battle starts from a black
 * screen (g_fade_intensity 0xff).
 *
 * banner_p and the loop's fade = 0xff reproduce the target's preheader order and
 * register choice for the hoisted color. The dead fade = 0 before done keeps the
 * step-0xd exit inline ahead of the per-frame path, as in the target; with a
 * plain break GCC moves that block to the loop tail. */
static void map_load_run(void) {
    RECT window = { 0, 0, 0, 0 };
    POLY_FT4 banner[2];
    u8 show_banner;
    s32 map_id;
    s16 fade;
    POLY_FT4* banner_p;

    deploy_init_display(0x100, 0xf0, 0x200, 0, 0, 0);
    show_banner = g_system_go_straight_to_battle;
    if (script_variable_get(EVENT_SCRIPT_VAR_SAVE_IN_PROGRESS) != 0) {
        show_banner = 0;
    }
    g_deploy_skipped = deploy_open();
    loading_centered_message_build(g_deploy_skipped);
    map_id = g_battle_map_id = script_variable_get(EVENT_SCRIPT_VAR_CURRENT_MAP);
    g_state_map_init_step = 0;
    g_deploy_finished = 0;

    SetPolyF4(&g_fade_polygons[0]);
    setRGB0(&g_fade_polygons[0], 0, 0, 0);
    setXY4(&g_fade_polygons[0], 0x80, 0, 0x180, 0, 0, 0x100, 0x180, 0x100);
    SetSemiTrans(&g_fade_polygons[0], 1);
    g_fade_polygons[1] = g_fade_polygons[0];
    g_fade_tint_polygons[0] = g_fade_polygons[0];
    g_fade_tint_polygons[1] = g_fade_polygons[0];
    SetDrawMode(&g_fade_draw_modes[0], 0, 0, PSYQ_GPU_TPAGE_ABR_SUBTRACT, &window);
    g_fade_draw_modes[1] = g_fade_draw_modes[0];
    g_fade_tint_draw_modes[0] = g_fade_draw_modes[0];
    g_fade_tint_draw_modes[1] = g_fade_draw_modes[0];

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

    /* Fade the banner in. Primitives added later to an ordering-table entry are
     * drawn first, so the fade polygon covers the banner. */
    fade = 0xf8;
    if (show_banner != 0) {
        do {
            gfx_battle_otag_swap();
            (g_fade_polygons + g_gfx_screen_polarity)->r0 = fade;
            (g_fade_polygons + g_gfx_screen_polarity)->g0 = fade;
            (g_fade_polygons + g_gfx_screen_polarity)->b0 = fade;
            AddPrim(gfx_battle_otag_get(), &g_fade_polygons[g_gfx_screen_polarity]);
            AddPrim(gfx_battle_otag_get(), &g_fade_draw_modes[g_gfx_screen_polarity]);
            AddPrim(gfx_battle_otag_get(), &banner[g_gfx_screen_polarity]);
            g_gfx_screen_polarity = g_gfx_screen_polarity == 0;
            g_system_battle_frame_hsync_count = deploy_sync_and_submit_frame(gfx_battle_otag_get() + 0x17f);
            fade -= 8;
        } while (fade >= 0);
    }

    banner_p = banner;
    fade = 0xff;
    while (deploy_update_controller_input() != -1) {
        gfx_battle_otag_swap();
        if (show_banner != 0) {
            AddPrim(gfx_battle_otag_get(), &banner_p[g_gfx_screen_polarity]);
        } else {
            loading_otag_append(gfx_battle_otag_get());
        }
        g_state_map_init_step = battle_map_deployment_init_update(
            map_id, g_state_map_init_step, deploy_run_frame_and_check(gfx_battle_otag_get(), g_input_current_buttons));
        if (g_state_map_init_step == 0xd) {
            fixed_cluts_load();
            state_enter_event_at_battle_start();
            g_fade_intensity = fade;
            /* Not `break`: jump optimization would move this exit block
             * past the loop end, which the target keeps inline. */
            goto done;
        }
        deployment_frame_stub();
        (g_fade_polygons + g_gfx_screen_polarity)->r0 = fade;
        (g_fade_polygons + g_gfx_screen_polarity)->g0 = fade;
        (g_fade_polygons + g_gfx_screen_polarity)->b0 = fade;
        AddPrim(gfx_battle_otag_get(), &g_fade_polygons[g_gfx_screen_polarity]);
        AddPrim(gfx_battle_otag_get(), &g_fade_draw_modes[g_gfx_screen_polarity]);
        g_gfx_screen_polarity = g_gfx_screen_polarity == 0;
        g_system_battle_frame_hsync_count = deploy_sync_and_submit_frame(gfx_battle_otag_get() + 0x17f);
        file_load_poll(&g_file_cd_state);
        system_frame_stub();
    }
    fade = 0;
done:
    /* Fade the banner out to black. */
    if (show_banner != 0) {
        fade = 0;
        do {
            if (fade >= 0xf8) {
                fade = 0xff;
            }
            gfx_battle_otag_swap();
            (g_fade_polygons + g_gfx_screen_polarity)->r0 = fade;
            (g_fade_polygons + g_gfx_screen_polarity)->g0 = fade;
            (g_fade_polygons + g_gfx_screen_polarity)->b0 = fade;
            AddPrim(gfx_battle_otag_get(), &g_fade_polygons[g_gfx_screen_polarity]);
            AddPrim(gfx_battle_otag_get(), &g_fade_draw_modes[g_gfx_screen_polarity]);
            AddPrim(gfx_battle_otag_get(), &banner[g_gfx_screen_polarity]);
            g_gfx_screen_polarity = g_gfx_screen_polarity == 0;
            g_system_battle_frame_hsync_count = deploy_sync_and_submit_frame(gfx_battle_otag_get() + 0x17f);
            fade += 8;
        } while (fade < 0x100);
    }
}

/* The battle program's main loop. After deployment, each frame reads the pad,
 * renders the map, runs the handler of the current game state, updates the camera,
 * map animations and music, and presents the frame. Entering CloseBattle ends the
 * loop; it starts over with deployment while the game flow is still battle. */
void battle_entrypoint(void) {
    sprite_evtchr_slot_t* timer_field;
    SVECTOR* camera_integer;
    VECTOR* camera_fixed;
    s32 frame_measurement;
    s32 empty_timer;
    s32 timer;
    s32 camera_component;
    s32 init_offset;
    s32 i;
    s32 timer_offset;

    render_buffers_init();
    sprite_render_state_init();
    effect_system_init();

    /* Clear the spritesheet id of each slot, last first. The byte offset preserves
     * the original reset loop; typed pointer and reverse index walks change it. */
    init_offset = (SPRITE_SHEET_SLOT_COUNT - 1) * sizeof(sprite_sheet_slot_t);
    do {
        g_sprite_sheet_ids_by_vram_slot[init_offset] = 0;
        init_offset -= sizeof(sprite_sheet_slot_t);
    } while (init_offset >= 0);

    camera_integer = &g_camera_integer_coords;
    camera_fixed = &g_camera_real_coords;

    do {
        map_load_run();
        g_menu_status_enabled = 0;
        g_cursor_input_mode = g_game_options.fields.cursor_movement;

        for (;;) {
            state_update_controller_input();
            sprite_render_frame_init();
            SetGeomScreen(0x200);
            gfx_battle_otag_swap();
            camera_scene_focus_update();

            /* Negate the Q12 camera position into whole units; the x conversion spells out the
             * round-toward-zero that / ONE does for y and z. */
            camera_component = -camera_fixed->vx;
            if (camera_component < 0) {
                camera_component += 0xfff;
            }
            camera_integer->vx = camera_component >> 12;
            camera_component = -camera_fixed->vy;
            camera_integer->vy = camera_component / ONE;
            camera_integer->vz = -camera_fixed->vz / ONE;

            camera_matrices_update(&g_camera_matrix, &g_camera_render_state, &g_camera_screen_offset, &g_camera_zoom);
            battle_map_mesh_weather_draw(&g_camera_matrix);
            /* The SEQ wait commands read this: 2 while an effect is updating, 0 otherwise. */
            if (effect_playback_update() != 0) {
                g_state_animation_continue_check = 2;
            } else {
                g_state_animation_continue_check = 0;
            }

            switch (g_game_state) {
            default:
            case GAME_STATE_FREE_CURSOR:
                state_handle_free_cursor();
                break;
            case GAME_STATE_FREE_CURSOR_HELP:
                state_handle_free_cursor_help();
                break;
            case GAME_STATE_HIGHLIGHT_UNITS:
                state_handle_highlight_units();
                break;
            case GAME_STATE_OPEN_ACTION_MENUS:
                state_handle_open_action_menus();
                break;
            case GAME_STATE_IDLING_ACTION_MENUS:
                state_handle_idling_action_menus();
                break;
            case GAME_STATE_AI_COMMAND:
                state_handle_ai_command();
                break;
            case GAME_STATE_ACTION_HELP_MENU:
                state_handle_action_help_menu();
                break;
            case GAME_STATE_DISPLAY_MOVE_AREA:
                state_handle_display_move_area();
                break;
            case GAME_STATE_MINI_MENU:
                state_handle_mini_menu();
                break;
            case GAME_STATE_MINI_MENU_HELP:
                state_handle_mini_menu_help();
                break;
            case GAME_STATE_AT_LIST_INSPECTION:
                state_handle_at_list_inspection();
                break;
            case GAME_STATE_AFTER_COMMAND:
                state_handle_after_command();
                break;
            case GAME_STATE_CONTINUE_TURN:
                state_handle_continue_turn();
                break;
            case GAME_STATE_CHANGE_TURN:
                state_handle_change_turn();
                break;
            case GAME_STATE_STATUS_EXECUTE:
                state_handle_status_execute();
                break;
            case GAME_STATE_UNIT_MOVE:
                state_handle_unit_move();
                break;
            case GAME_STATE_SELECT_MOVE_TILE:
                state_handle_select_move_tile();
                break;
            case GAME_STATE_MOVE_RANGE_EXCEPTION:
                state_handle_move_range_exception();
                break;
            case GAME_STATE_ILLEGAL_MOVE_MENU:
                state_handle_illegal_move_menu();
                break;
            case GAME_STATE_UNIT_MOVING_SETUP:
                state_handle_unit_moving_setup();
                break;
            case GAME_STATE_UNIT_MOVING:
                state_handle_unit_moving();
                break;
            case GAME_STATE_MOVE_CONFIRM_MENU:
                state_handle_move_confirm_menu();
                break;
            case GAME_STATE_WAIT_DIRECTION:
                state_handle_wait_direction();
                break;
            case GAME_STATE_WAIT_MENU:
                state_handle_wait_menu();
                break;
            case GAME_STATE_POST_MOVE_EVENTS:
                state_handle_post_move_events();
                break;
            case GAME_STATE_TARGETING_PROMPT:
                state_handle_targeting_prompt();
                break;
            case GAME_STATE_TARGETING_RANGE:
                state_handle_targeting_range();
                break;
            case GAME_STATE_ILLEGAL_RANGE:
                state_handle_illegal_range();
                break;
            case GAME_STATE_ABILITY_PREVIEW_HANDLING:
                state_handle_ability_preview();
                break;
            case GAME_STATE_ABILITY_PREVIEW_HELP:
                state_handle_ability_preview_help();
                break;
            case GAME_STATE_CONFIRM_ACTION:
                state_handle_confirm_action();
                break;
            case GAME_STATE_PRE_ATTACK_ANIMATION:
                state_handle_pre_attack_animation();
                break;
            case GAME_STATE_ACTION_CAST:
                state_handle_action_cast();
                break;
            case GAME_STATE_COMMENCE_ATTACK_PHASE:
                state_handle_commence_attack_phase();
                break;
            case GAME_STATE_ACTION_ANNOUNCEMENT:
                state_handle_action_announcement();
                break;
            case GAME_STATE_OPEN_SP2_FILES:
                state_handle_open_sp2_files();
                break;
            case GAME_STATE_START_EFFECT_FILE_OPEN:
                state_handle_start_effect_file_open();
                break;
            case GAME_STATE_ACTION_EXECUTE_START:
                state_handle_action_execute_start();
                break;
            case GAME_STATE_ACTION_EXECUTE:
                state_handle_action_execute();
                break;
            case GAME_STATE_BATTLE_MESSAGE_DISPLAY:
                state_handle_battle_message_display();
                break;
            case GAME_STATE_RESUME_ATTACK_PHASE:
                state_handle_resume_attack_phase();
                break;
            case GAME_STATE_POST_ACTION_REPORTS:
                state_handle_post_action_reports();
                break;
            case GAME_STATE_DEEP_DUNGEON_MESH_LOAD:
                state_handle_deep_dungeon_mesh_load();
                break;
            case GAME_STATE_DEEP_DUNGEON_MESH_FINISH:
                state_handle_deep_dungeon_mesh_finish();
                break;
            case GAME_STATE_TARGET_SELECT_START:
                state_handle_target_select_start();
                break;
            case GAME_STATE_TARGET_SELECT:
                state_handle_target_select();
                break;
            case GAME_STATE_TARGET_SELECT_DENIED:
                state_handle_target_select_denied();
                break;
            case GAME_STATE_TARGET_SELECT_CONFIRM:
                state_handle_target_select_confirm();
                break;
            case GAME_STATE_TARGET_DISPLAY_START:
                state_handle_target_display_start();
                break;
            case GAME_STATE_TARGET_DISPLAY:
                state_handle_target_display();
                break;
            case GAME_STATE_EFFECT:
                state_handle_effect();
                break;
            case GAME_STATE_EVENT:
                state_handle_event();
                break;
            case GAME_STATE_MAP_JUMPING_OUT:
                state_handle_map_jumping_out();
                break;
            case GAME_STATE_MAP_INITIALIZE:
                state_handle_map_init();
                break;
            case GAME_STATE_MAP_JUMPING_IN:
                state_handle_map_jumping_in();
                break;
            case GAME_STATE_MAP_JUMPING_OUT_2:
                state_handle_change_map_jumping_out();
                break;
            case GAME_STATE_MAP_INITIALIZE_2:
                state_handle_change_map_init();
                break;
            case GAME_STATE_MAP_JUMPING_IN_2:
                state_handle_change_map_jumping_in();
                break;
            case GAME_STATE_CLOSE_BATTLE:
                state_handle_close_battle();
                break;
            }

            camera_pan_update();
            camera_rotation_update();
            camera_zoom_update();
            camera_tilt_update();
            battle_map_update_color_animations();
            music_tunes_update();

            /* A closing battle leaves before this frame is presented. */
            if (g_game_state == GAME_STATE_CLOSE_BATTLE) {
                break;
            }

            file_load_poll(&g_file_cd_state);
            g_gfx_screen_polarity = !g_gfx_screen_polarity;
            g_frame_counter++;
            battle_map_clut_upload_rows();
            frame_measurement = state_sync_frame(gfx_battle_otag_get() + 0x17f);
            g_system_battle_frame_hsync_count = frame_measurement;
            i = 0;
            if (g_system_battle_frame_hsync_count_max < frame_measurement) {
                g_system_battle_frame_hsync_count_max = frame_measurement;
            }

            /* Count down the image buffers' upload timers; 0 (free) and BUFFER_EVTCHR are markers,
             * not timers. */
            empty_timer = SPRITE_EVTCHR_SLOT_BUFFER_EVTCHR;
            timer_field = g_sprite_evtchr_slots;
            timer_offset = 0;
            do {
                /* The byte offset preserves the target's separate indexed load. */
                timer = ((sprite_evtchr_slot_t*)((u8*)g_sprite_evtchr_slots + timer_offset))->image_buffer_state;
                timer_offset += sizeof(*timer_field);
                if (timer != 0 && timer != empty_timer) {
                    timer_field->image_buffer_state = timer - 1;
                }
                i++;
                timer_field++;
            } while (i < SPRITE_EVTCHR_SLOT_COUNT);

            /* A frame that ran long during effects arms the pacing timer, which state_sync_frame
             * uses to lengthen the following waits. */
            if (g_game_state == GAME_STATE_EFFECT || g_game_state == GAME_STATE_ACTION_EXECUTE) {
                if (g_system_vsync_interval == 1) {
                    g_system_frame_pacing_timer = 0;
                } else if (g_system_frame_pacing_suppressed == 0) {
                    if (g_system_battle_frame_hsync_count >= 0x301) {
                        g_system_frame_pacing_timer = 0x1e;
                    } else if (g_system_battle_frame_hsync_count >= 0x201 && g_system_frame_pacing_timer < 0x0f) {
                        g_system_frame_pacing_timer = 0x0f;
                    }
                }
            }

            g_system_frame_pacing_suppressed = 0;
            system_frame_stub();
        }

        battle_map_animations_stop();
        unit_all_clear();
    } while (g_game_flow_state == SYSTEM_GAME_FLOW_BATTLE);

    /* Let drawing and two vertical blanks finish before leaving. */
    DrawSync(0);
    VSync(0);
    DrawSync(0);
    VSync(0);
}
