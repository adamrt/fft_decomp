/* Load the battle and deployment screen, then run the battle state loop. */

#include "fft/battle/battle_state.h"
#include "fft/battle/camera.h"
#include "fft/battle/command.h"
#include "fft/battle/cursor.h"
#include "fft/battle/deploy.h"
#include "fft/effect/effect_script.h"
#include "fft/gfx/fade.h"
#include "fft/gfx/gfx.h"
#include "fft/gfx/sprite.h"
#include "fft/gfx/sprite_sheet.h"
#include "fft/map/map.h"
#include "fft/map/map_load.h"
#include "fft/map/map_mesh.h"
#include "fft/map/map_palette.h"
#include "fft/map/map_unit.h"
#include "fft/menu/option.h"
#include "fft/script/script_variable.h"
#include "fft/sys/file.h"
#include "fft/sys/input.h"
#include "fft/sys/music.h"
#include "fft/sys/system.h"
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
static void load_fixed_cluts(void) {
    RECT rect; /* never used; retail reserves its 8 bytes */

    map_palette_load_data(&g_cursor_tile_glow_palettes[0], MAP_CLUT_ROW_SPRITES, 1, 0);
    map_palette_load_data(&g_cursor_tile_glow_palettes[1], MAP_CLUT_ROW_SPRITES, 2, 0);
    map_palette_load_data(&g_unit_facing_wait_arrow_palette, MAP_CLUT_ROW_SPRITES, 3, 0);
    map_palette_load_data(&g_cursor_map_selection_palette, MAP_CLUT_ROW_SPRITES, 4, 0);
    map_palette_load_data(&g_sprite_shadow_palette, MAP_CLUT_ROW_SPRITES, 5, 0);
    map_palette_load_data(&g_status_bubble_palette, MAP_CLUT_ROW_SPRITES, 7, 0);
}

/* Empty. run_map_load calls it each frame while the map is still being set up. */
static void update_map_load(void) {
}

/* Load the battle map, its units, sprites, event and music, showing a loading screen.
 *
 * A battle entered directly (g_system_go_straight_to_battle, unless a saved
 * game is being resumed) shows the deployment banner: it fades in from black,
 * stays over a black screen while map initialization steps run up to step 0xd
 * (done), then fades back out to black. Otherwise the NOW LOADING display is
 * drawn instead. The fade polygons subtract their gray level from the screen, so
 * 0xff is black and 0 leaves the picture as drawn. The battle starts from a black
 * screen (g_fade_battle_level 0xff).
 *
 * banner_p and the loop's fade = 0xff reproduce the target's preheader order and
 * register choice for the hoisted color. The dead fade = 0 before done keeps the
 * step-0xd exit inline ahead of the per-frame path, as in the target; with a
 * plain break GCC moves that block to the loop tail. */
static void run_map_load(void) {
    RECT window = { 0, 0, 0, 0 };
    POLY_FT4 banner[2];
    u8 show_banner;
    s32 map_id;
    s16 fade;
    POLY_FT4* banner_p;

    battle_init_display(0x100, 0xf0, 0x200, 0, 0, 0);
    show_banner = g_system_go_straight_to_battle;
    if (script_variable_get(EVENT_SCRIPT_VAR_SAVE_IN_PROGRESS)) {
        show_banner = 0;
    }
    g_deploy_skipped = deploy_open();
    loading_message_build_offset(g_deploy_skipped);
    map_id = g_map_id = script_variable_get(EVENT_SCRIPT_VAR_CURRENT_MAP);
    g_map_init_step = 0;
    g_deploy_finished = 0;

    SetPolyF4(&g_fade_battle_polygons[0]);
    setRGB0(&g_fade_battle_polygons[0], 0, 0, 0);
    setXY4(&g_fade_battle_polygons[0], 128, 0, 384, 0, 0, 256, 384, 256);
    SetSemiTrans(&g_fade_battle_polygons[0], 1);
    g_fade_battle_polygons[1] = g_fade_battle_polygons[0];
    g_fade_tint_polygons[0] = g_fade_battle_polygons[0];
    g_fade_tint_polygons[1] = g_fade_battle_polygons[0];
    SetDrawMode(&g_fade_battle_draw_modes[0], 0, 0, PSYQ_GPU_TPAGE_ABR_SUBTRACT, &window);
    g_fade_battle_draw_modes[1] = g_fade_battle_draw_modes[0];
    g_fade_tint_draw_modes[0] = g_fade_battle_draw_modes[0];
    g_fade_tint_draw_modes[1] = g_fade_battle_draw_modes[0];

    setPolyFT4(&banner[0]);
    setRGB0(&banner[0], 0x80, 0x80, 0x80);
    setXY4(&banner[0], 128, 116, 383, 116, 128, 140, 383, 140);
    setUV4(&banner[0], 0, 0, 255, 0, 0, 24, 255, 24);
    SetSemiTrans(&banner[0], 0);
    banner[0].tpage = GetTPage(0, 0, 0x380, 0x100);
    banner[0].clut = GetClut(0x380, 0x11f);
    banner[1] = banner[0];
    SetDispMask(1);

    /* Fade the banner in. Primitives added later to an ordering-table entry are
     * drawn first, so the fade polygon covers the banner. */
    fade = 0xf8;
    if (show_banner) {
        do {
            gfx_swap_battle_otag();
            /* Pointer arithmetic, not indexing, in all three loops: indexing compiles larger. */
            (g_fade_battle_polygons + g_gfx_screen_polarity)->r0 = fade;
            (g_fade_battle_polygons + g_gfx_screen_polarity)->g0 = fade;
            (g_fade_battle_polygons + g_gfx_screen_polarity)->b0 = fade;
            AddPrim(gfx_get_battle_otag(), &g_fade_battle_polygons[g_gfx_screen_polarity]);
            AddPrim(gfx_get_battle_otag(), &g_fade_battle_draw_modes[g_gfx_screen_polarity]);
            AddPrim(gfx_get_battle_otag(), &banner[g_gfx_screen_polarity]);
            g_gfx_screen_polarity = g_gfx_screen_polarity == 0;
            g_system_battle_frame_hsync_count = deploy_submit_frame(gfx_get_battle_otag() + 0x17f);
            fade -= 8;
        } while (fade >= 0);
    }

    banner_p = banner;
    fade = 0xff;
    while (deploy_update_input() != -1) {
        gfx_swap_battle_otag();
        if (show_banner) {
            AddPrim(gfx_get_battle_otag(), &banner_p[g_gfx_screen_polarity]);
        } else {
            loading_message_append_to_otag(gfx_get_battle_otag());
        }
        g_map_init_step = map_update_deployment_init_sequence(
            map_id, g_map_init_step, deploy_update_frame(gfx_get_battle_otag(), g_input_current_buttons));
        if (g_map_init_step == 0xd) {
            load_fixed_cluts();
            battle_state_battle_start_event_enter();
            g_fade_battle_level = fade;
            /* Not `break`: jump optimization would move this exit block
             * past the loop end, which the target keeps inline. */
            goto done;
        }
        update_map_load();
        (g_fade_battle_polygons + g_gfx_screen_polarity)->r0 = fade;
        (g_fade_battle_polygons + g_gfx_screen_polarity)->g0 = fade;
        (g_fade_battle_polygons + g_gfx_screen_polarity)->b0 = fade;
        AddPrim(gfx_get_battle_otag(), &g_fade_battle_polygons[g_gfx_screen_polarity]);
        AddPrim(gfx_get_battle_otag(), &g_fade_battle_draw_modes[g_gfx_screen_polarity]);
        g_gfx_screen_polarity = g_gfx_screen_polarity == 0;
        g_system_battle_frame_hsync_count = deploy_submit_frame(gfx_get_battle_otag() + 0x17f);
        file_poll_load(&g_file_cd_state);
        system_frame_stub();
    }
    fade = 0;
done:
    /* Fade the banner out to black. */
    if (show_banner) {
        fade = 0;
        do {
            if (fade >= 0xf8) {
                fade = 0xff;
            }
            gfx_swap_battle_otag();
            (g_fade_battle_polygons + g_gfx_screen_polarity)->r0 = fade;
            (g_fade_battle_polygons + g_gfx_screen_polarity)->g0 = fade;
            (g_fade_battle_polygons + g_gfx_screen_polarity)->b0 = fade;
            AddPrim(gfx_get_battle_otag(), &g_fade_battle_polygons[g_gfx_screen_polarity]);
            AddPrim(gfx_get_battle_otag(), &g_fade_battle_draw_modes[g_gfx_screen_polarity]);
            AddPrim(gfx_get_battle_otag(), &banner[g_gfx_screen_polarity]);
            g_gfx_screen_polarity = g_gfx_screen_polarity == 0;
            g_system_battle_frame_hsync_count = deploy_submit_frame(gfx_get_battle_otag() + 0x17f);
            fade += 8;
        } while (fade < 0x100);
    }
}

/* The battle program's main loop. After deployment, each frame reads the pad,
 * renders the map, runs the handler of the current game state, updates the camera,
 * map animations and music, and presents the frame. Entering CloseBattle ends the
 * loop; it starts over with deployment while the game flow is still battle. */
void battle_entrypoint(void) {
    sprite_evtchr_slot_t* sprite_slot;
    SVECTOR* camera_integer;
    VECTOR* camera_fixed;
    s32 frame_measurement;
    s32 buffer_marker;
    s32 timer;
    s32 camera_component;
    s32 init_offset;
    s32 i;
    s32 timer_offset;

    gfx_init_render_buffers();
    sprite_init_render_state();
    effect_init_system();

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
        run_map_load();
        g_menu_status_enabled = 0;
        g_cursor_input_mode = g_game_options.fields.cursor_movement;

        for (;;) {
            battle_update_input();
            sprite_init_render_frame();
            SetGeomScreen(0x200);
            gfx_swap_battle_otag();
            camera_update_scene_focus();

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

            camera_update_matrices(&g_camera_matrix, &g_camera_render_state, &g_camera_screen_offset, &g_camera_zoom);
            map_mesh_draw_weather(&g_camera_matrix);
            /* The SEQ wait commands read this: 2 while an effect is updating, 0 otherwise. */
            if (effect_playback_update()) {
                g_battle_animation_wait_state = 2;
            } else {
                g_battle_animation_wait_state = 0;
            }

            switch (g_battle_state) {
            default:
            case BATTLE_STATE_FREE_CURSOR:
                battle_state_free_cursor_update();
                break;
            case BATTLE_STATE_FREE_CURSOR_HELP:
                battle_state_free_cursor_help_update();
                break;
            case BATTLE_STATE_HIGHLIGHT_UNITS:
                battle_state_highlight_units_update();
                break;
            case BATTLE_STATE_OPEN_ACTION_MENUS:
                battle_state_open_action_menus_update();
                break;
            case BATTLE_STATE_IDLING_ACTION_MENUS:
                battle_state_idling_action_menus_update();
                break;
            case BATTLE_STATE_AI_COMMAND:
                battle_state_ai_command_update();
                break;
            case BATTLE_STATE_ACTION_HELP_MENU:
                battle_state_action_help_menu_update();
                break;
            case BATTLE_STATE_DISPLAY_MOVE_AREA:
                battle_state_display_move_area_update();
                break;
            case BATTLE_STATE_MINI_MENU:
                battle_state_mini_menu_update();
                break;
            case BATTLE_STATE_MINI_MENU_HELP:
                battle_state_mini_menu_help_update();
                break;
            case BATTLE_STATE_AT_LIST_INSPECTION:
                battle_state_at_list_inspection_update();
                break;
            case BATTLE_STATE_AFTER_COMMAND:
                battle_state_after_command_update();
                break;
            case BATTLE_STATE_CONTINUE_TURN:
                battle_state_continue_turn_update();
                break;
            case BATTLE_STATE_CHANGE_TURN:
                battle_state_change_turn_update();
                break;
            case BATTLE_STATE_STATUS_EXECUTE:
                battle_state_status_execute_update();
                break;
            case BATTLE_STATE_UNIT_MOVE:
                battle_state_unit_move_update();
                break;
            case BATTLE_STATE_SELECT_MOVE_TILE:
                battle_state_select_move_tile_update();
                break;
            case BATTLE_STATE_MOVE_RANGE_EXCEPTION:
                battle_state_move_range_exception_update();
                break;
            case BATTLE_STATE_ILLEGAL_MOVE_MENU:
                battle_state_illegal_move_menu_update();
                break;
            case BATTLE_STATE_UNIT_MOVING_SETUP:
                battle_state_unit_moving_setup_update();
                break;
            case BATTLE_STATE_UNIT_MOVING:
                battle_state_unit_moving_update();
                break;
            case BATTLE_STATE_MOVE_CONFIRM_MENU:
                battle_state_move_confirm_menu_update();
                break;
            case BATTLE_STATE_WAIT_DIRECTION:
                battle_state_wait_direction_update();
                break;
            case BATTLE_STATE_WAIT_MENU:
                battle_state_wait_menu_update();
                break;
            case BATTLE_STATE_POST_MOVE_EVENTS:
                battle_state_post_move_events_update();
                break;
            case BATTLE_STATE_TARGETING_PROMPT:
                battle_state_targeting_prompt_update();
                break;
            case BATTLE_STATE_TARGETING_RANGE:
                battle_state_targeting_range_update();
                break;
            case BATTLE_STATE_ILLEGAL_RANGE:
                battle_state_illegal_range_update();
                break;
            case BATTLE_STATE_ABILITY_PREVIEW_HANDLING:
                battle_state_ability_preview_update();
                break;
            case BATTLE_STATE_ABILITY_PREVIEW_HELP:
                battle_state_ability_preview_help_update();
                break;
            case BATTLE_STATE_CONFIRM_ACTION:
                battle_state_confirm_action_update();
                break;
            case BATTLE_STATE_PRE_ATTACK_ANIMATION:
                battle_state_pre_attack_animation_update();
                break;
            case BATTLE_STATE_ACTION_CAST:
                battle_state_action_cast_update();
                break;
            case BATTLE_STATE_COMMENCE_ATTACK_PHASE:
                battle_state_commence_attack_phase_update();
                break;
            case BATTLE_STATE_ACTION_ANNOUNCEMENT:
                battle_state_action_announcement_update();
                break;
            case BATTLE_STATE_OPEN_SP2_FILES:
                battle_state_open_sp2_files_update();
                break;
            case BATTLE_STATE_START_EFFECT_FILE_OPEN:
                battle_state_start_effect_file_open_update();
                break;
            case BATTLE_STATE_ACTION_EXECUTE_START:
                battle_state_action_execute_start_update();
                break;
            case BATTLE_STATE_ACTION_EXECUTE:
                battle_state_action_execute_update();
                break;
            case BATTLE_STATE_BATTLE_MESSAGE_DISPLAY:
                battle_state_battle_message_display_update();
                break;
            case BATTLE_STATE_RESUME_ATTACK_PHASE:
                battle_state_resume_attack_phase_update();
                break;
            case BATTLE_STATE_POST_ACTION_REPORTS:
                battle_state_post_action_reports_update();
                break;
            case BATTLE_STATE_DEEP_DUNGEON_MESH_LOAD:
                battle_state_deep_dungeon_mesh_load_update();
                break;
            case BATTLE_STATE_DEEP_DUNGEON_MESH_FINISH:
                battle_state_deep_dungeon_mesh_finish_update();
                break;
            case BATTLE_STATE_TARGET_SELECT_START:
                battle_state_target_select_start_update();
                break;
            case BATTLE_STATE_TARGET_SELECT:
                battle_state_target_select_update();
                break;
            case BATTLE_STATE_TARGET_SELECT_DENIED:
                battle_state_target_select_denied_update();
                break;
            case BATTLE_STATE_TARGET_SELECT_CONFIRM:
                battle_state_target_select_confirm_update();
                break;
            case BATTLE_STATE_TARGET_DISPLAY_START:
                battle_state_target_display_start_update();
                break;
            case BATTLE_STATE_TARGET_DISPLAY:
                battle_state_target_display_update();
                break;
            case BATTLE_STATE_EFFECT:
                battle_state_effect_update();
                break;
            case BATTLE_STATE_EVENT:
                battle_state_event_update();
                break;
            case BATTLE_STATE_MAP_JUMPING_OUT:
                battle_state_map_jumping_out_update();
                break;
            case BATTLE_STATE_MAP_INITIALIZE:
                battle_state_map_init_update();
                break;
            case BATTLE_STATE_MAP_JUMPING_IN:
                battle_state_map_jumping_in_update();
                break;
            case BATTLE_STATE_MAP_JUMPING_OUT_2:
                battle_state_change_map_jumping_out_update();
                break;
            case BATTLE_STATE_MAP_INITIALIZE_2:
                battle_state_change_map_init_update();
                break;
            case BATTLE_STATE_MAP_JUMPING_IN_2:
                battle_state_change_map_jumping_in_update();
                break;
            case BATTLE_STATE_CLOSE_BATTLE:
                battle_state_close_battle_update();
                break;
            }

            camera_update_pan();
            camera_update_rotation();
            camera_update_zoom();
            camera_update_tilt();
            map_palette_update_color_animations();
            music_update_tunes();

            /* A closing battle leaves before this frame is presented. */
            if (g_battle_state == BATTLE_STATE_CLOSE_BATTLE) {
                break;
            }

            file_poll_load(&g_file_cd_state);
            g_gfx_screen_polarity = !g_gfx_screen_polarity;
            g_frame_counter++;
            map_palette_upload_clut_rows();
            frame_measurement = battle_submit_frame(gfx_get_battle_otag() + 0x17f);
            g_system_battle_frame_hsync_count = frame_measurement;
            i = 0;
            if (g_system_battle_frame_hsync_count_max < frame_measurement) {
                g_system_battle_frame_hsync_count_max = frame_measurement;
            }

            /* Count down the image buffers' upload timers; 0 (free) and BUFFER_EVTCHR are markers,
             * not timers. */
            buffer_marker = SPRITE_EVTCHR_SLOT_BUFFER_EVTCHR;
            sprite_slot = g_sprite_evtchr_slots;
            timer_offset = 0;
            do {
                /* The byte offset preserves the target's separate indexed load. */
                timer = ((sprite_evtchr_slot_t*)((u8*)g_sprite_evtchr_slots + timer_offset))->image_buffer_state;
                timer_offset += sizeof(*sprite_slot);
                if (timer != 0 && timer != buffer_marker) {
                    sprite_slot->image_buffer_state = timer - 1;
                }
                i++;
                sprite_slot++;
            } while (i < SPRITE_EVTCHR_SLOT_COUNT);

            /* A frame that ran long during effects arms the pacing timer, which battle_submit_frame
             * uses to lengthen the following waits. */
            if (g_battle_state == BATTLE_STATE_EFFECT || g_battle_state == BATTLE_STATE_ACTION_EXECUTE) {
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

        map_stop_animations();
        map_unit_clear_all();
    } while (g_game_flow_state == SYSTEM_GAME_FLOW_BATTLE);

    /* Let drawing and two vertical blanks finish before leaving. */
    DrawSync(0);
    VSync(0);
    DrawSync(0);
    VSync(0);
}
