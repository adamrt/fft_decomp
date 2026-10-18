/* Load a battle map, run deployment and drive the battle state loop. This unit
 * also handles the live-unit queries and sprite loading/drawing used by that
 * loop and the unit animation code. */

#include "fft/ability.h"
#include "fft/animation.h"
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
#include "fft/status.h"
#include "fft/system.h"
#include "fft/unit.h"
#include "fft/unit_runtime.h"
#include "fft/vector.h"
#include "psx/gte_inline.h"
#include "psx/gte_inline_split.h"
#include "psx/libc.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/stddef.h"
#include "psx/types.h"

/* Sprite loading and rendering state. */
extern u8 g_sprite_sheet_ids_by_vram_slot[];
extern file_extent_t g_sprite_sp2_files[];
extern file_extent_t g_sprite_worker8_sp2_files[];
extern palette_t g_status_bubble_palette;
extern MATRIX g_sprite_display_matrix;
extern s16 g_sprite_prim_index;
extern s16 g_sprite_prim_frame_start_index;

/* Cleared with the sprite counters at render init; nothing reads them. */
extern s16 g_sprite_unread_1;
extern s16 g_sprite_unread_2;

extern s32 g_system_battle_frame_hsync_count_max;
extern palette_t g_cursor_map_selection_palette;
extern palette_t g_unit_facing_wait_arrow_palette;

/* Cursor state for the compressed attack-image tail of a unit SPR file. */
extern const u8* g_sprite_compressed_data;
extern s32 g_sprite_compressed_nibble_phase;
extern s32 g_sprite_compressed_offset;

/* Loading BATTLE initializes the result; MAIN reads it when the battle loop returns. */
system_game_flow_e g_game_flow_state = SYSTEM_GAME_FLOW_BATTLE;

static SVECTOR g_sprite_gte_rtpt_vxy0 = { 0 };
static SVECTOR g_sprite_gte_rtpt_vxy1 = { 0 };
static SVECTOR g_sprite_gte_rtpt_vxy2 = { 0 };
static SVECTOR g_sprite_gte_rtps_vxy0 = { 0 };

static void sprite_init_render_frame(void);
static void sprite_init_render_state(void);

/* Uploads the fixed CLUTs of the sprite row: 1 and 2 the cursor tile glow palettes,
 * 3 the wait-direction arrow, 4 the map selection cursor, 5 the shadow and 7 the
 * status bubble. */
static void sprite_palette_load_fixed_cluts(void) {
    RECT rect; /* never used; retail reserves its 8 bytes */

    battle_map_palette_load_data(&g_cursor_tile_glow_palettes[0], BATTLE_MAP_CLUT_ROW_SPRITES, 1, 0);
    battle_map_palette_load_data(&g_cursor_tile_glow_palettes[1], BATTLE_MAP_CLUT_ROW_SPRITES, 2, 0);
    battle_map_palette_load_data(&g_unit_facing_wait_arrow_palette, BATTLE_MAP_CLUT_ROW_SPRITES, 3, 0);
    battle_map_palette_load_data(&g_cursor_map_selection_palette, BATTLE_MAP_CLUT_ROW_SPRITES, 4, 0);
    battle_map_palette_load_data(&g_sprite_shadow_palette, BATTLE_MAP_CLUT_ROW_SPRITES, 5, 0);
    battle_map_palette_load_data(&g_status_bubble_palette, BATTLE_MAP_CLUT_ROW_SPRITES, 7, 0);
}

/* Empty. state_run_map_load calls it each frame while the map is still being set up. */
static void deploy_frame_stub(void) {
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
static void state_run_map_load(void) {
    RECT window = { 0, 0, 0, 0 };
    POLY_FT4 banner[2];
    u8 show_banner;
    s32 map_id;
    s16 fade;
    POLY_FT4* banner_p;

    deploy_init_display(0x100, 0xf0, 0x200, 0, 0, 0);
    show_banner = g_system_go_straight_to_battle;
    if (script_get_variable(EVENT_SCRIPT_VAR_SAVE_IN_PROGRESS) != 0) {
        show_banner = 0;
    }
    g_deploy_skipped = deploy_open();
    loading_build_message_centered(g_deploy_skipped);
    map_id = g_battle_map_id = script_get_variable(EVENT_SCRIPT_VAR_CURRENT_MAP);
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
            gfx_swap_and_clear_battle_otag();
            (g_fade_polygons + g_gfx_screen_polarity)->r0 = fade;
            (g_fade_polygons + g_gfx_screen_polarity)->g0 = fade;
            (g_fade_polygons + g_gfx_screen_polarity)->b0 = fade;
            AddPrim(gfx_get_battle_otag(), &g_fade_polygons[g_gfx_screen_polarity]);
            AddPrim(gfx_get_battle_otag(), &g_fade_draw_modes[g_gfx_screen_polarity]);
            AddPrim(gfx_get_battle_otag(), &banner[g_gfx_screen_polarity]);
            g_gfx_screen_polarity = g_gfx_screen_polarity == 0;
            g_system_battle_frame_hsync_count = deploy_sync_and_submit_frame(gfx_get_battle_otag() + 0x17f);
            fade -= 8;
        } while (fade >= 0);
    }

    banner_p = banner;
    fade = 0xff;
    while (deploy_update_controller_input() != -1) {
        gfx_swap_and_clear_battle_otag();
        if (show_banner != 0) {
            AddPrim(gfx_get_battle_otag(), &banner_p[g_gfx_screen_polarity]);
        } else {
            loading_add_to_otag(gfx_get_battle_otag());
        }
        g_state_map_init_step = battle_map_step_deployment_init_sequence(
            map_id, g_state_map_init_step, deploy_run_frame_and_check(gfx_get_battle_otag(), g_input_current_buttons));
        if (g_state_map_init_step == 0xd) {
            sprite_palette_load_fixed_cluts();
            state_enter_event_at_battle_start();
            g_fade_intensity = fade;
            /* Not `break`: jump optimization would move this exit block
             * past the loop end, which the target keeps inline. */
            goto done;
        }
        deploy_frame_stub();
        (g_fade_polygons + g_gfx_screen_polarity)->r0 = fade;
        (g_fade_polygons + g_gfx_screen_polarity)->g0 = fade;
        (g_fade_polygons + g_gfx_screen_polarity)->b0 = fade;
        AddPrim(gfx_get_battle_otag(), &g_fade_polygons[g_gfx_screen_polarity]);
        AddPrim(gfx_get_battle_otag(), &g_fade_draw_modes[g_gfx_screen_polarity]);
        g_gfx_screen_polarity = g_gfx_screen_polarity == 0;
        g_system_battle_frame_hsync_count = deploy_sync_and_submit_frame(gfx_get_battle_otag() + 0x17f);
        file_poll_load(&g_file_cd_state);
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
            gfx_swap_and_clear_battle_otag();
            (g_fade_polygons + g_gfx_screen_polarity)->r0 = fade;
            (g_fade_polygons + g_gfx_screen_polarity)->g0 = fade;
            (g_fade_polygons + g_gfx_screen_polarity)->b0 = fade;
            AddPrim(gfx_get_battle_otag(), &g_fade_polygons[g_gfx_screen_polarity]);
            AddPrim(gfx_get_battle_otag(), &g_fade_draw_modes[g_gfx_screen_polarity]);
            AddPrim(gfx_get_battle_otag(), &banner[g_gfx_screen_polarity]);
            g_gfx_screen_polarity = g_gfx_screen_polarity == 0;
            g_system_battle_frame_hsync_count = deploy_sync_and_submit_frame(gfx_get_battle_otag() + 0x17f);
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

    render_init_buffers();
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
        state_run_map_load();
        g_menu_status_enabled = 0;
        g_cursor_input_mode = g_game_options.fields.cursor_movement;

        for (;;) {
            state_update_controller_input();
            sprite_init_render_frame();
            SetGeomScreen(0x200);
            gfx_swap_and_clear_battle_otag();
            camera_update_vector_from_game_state();

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
            battle_map_draw_mesh_and_weather(&g_camera_matrix);
            /* The SEQ wait commands read this: 2 while an effect is updating, 0 otherwise. */
            if (effect_update_cycle() != 0) {
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
            music_update_tunes();

            /* A closing battle leaves before this frame is presented. */
            if (g_game_state == GAME_STATE_CLOSE_BATTLE) {
                break;
            }

            file_poll_load(&g_file_cd_state);
            g_gfx_screen_polarity = !g_gfx_screen_polarity;
            g_frame_counter++;
            battle_map_clut_upload_rows();
            frame_measurement = state_sync_frame(gfx_get_battle_otag() + 0x17f);
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

        battle_map_stop_animations();
        unit_clear_all();
    } while (g_game_flow_state == SYSTEM_GAME_FLOW_BATTLE);

    /* Let drawing and two vertical blanks finish before leaving. */
    DrawSync(0);
    VSync(0);
    DrawSync(0);
    VSync(0);
}

/* The runtime record of the casting unit (g_unit_casting_runtime_id), or 0. It is the source
 * unit except while a reaction or Hamedo counter resolves, or while a rider's mount takes its
 * move. */
unit_t* unit_get_casting(void) {
    unit_t* unit;

    for (unit = g_unit_list_head; unit != 0; unit = unit->previous) {
        if (unit->runtime_id == g_unit_casting_runtime_id) {
            return unit;
        }
    }
    return 0;
}

/* The runtime record of the source unit, whose turn or queued between-turn event is being resolved
 * (g_unit_source_runtime_id), or 0. */
unit_t* unit_get_source(void) {
    unit_t* unit = g_unit_list_head;
    s32 source_id;

    if (unit != 0) {
        source_id = g_unit_source_runtime_id;
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
unit_t* unit_find_runtime_at_tile(s32 map_x, s32 map_y, s32 layer) {
    unit_t* unit = g_unit_list_head;

    while (unit != 0) {
        if (unit->map_x == map_x && unit->map_y == map_y && unit->map_layer == layer) {
            return unit;
        }
        unit = unit->previous;
    }
    return 0;
}

/* Returns the unit the cursor selects on a tile: one with battle stats in an
 * occupied slot that is not a crystal, a treasure or in the air from Jump.
 *
 * Among several, the source unit (g_unit_source_runtime_id) is preferred,
 * then a rider over its mount, then the first in list order. Returns 0 for none.
 * The candidate array holds 16 entries and is filled without a bound check. */
unit_t* unit_find_selectable_at_tile(s32 map_x, s32 map_y, s32 layer) {
    unit_t* matches[UNIT_RUNTIME_SLOT_COUNT];
    unit_t* unit;
    s32 count;
    s32 index;

    count = 0;
    unit = g_unit_list_head;
    while (unit != 0) {
        if ((unit->map_x == map_x) && (unit->map_y == map_y) && (unit->map_layer == layer) && (unit->stats != 0)
            && (unit->stats->active_battle_id != UNIT_ACTIVE_BATTLE_ID_NONE)
            && ((unit->status_flags_5_6 & (UNIT_STATUS_CRYSTAL | UNIT_STATUS_TREASURE | UNIT_STATUS_JUMP)) == 0)) {
            matches[count++] = unit;
        }
        unit = unit->previous;
    }
    if (count != 0) {
        if (count != 1) {
            for (index = 0; index < count; index++) {
                if (matches[index]->runtime_id == g_unit_source_runtime_id) {
                    return matches[index];
                }
            }
            for (index = 0; index < count; index++) {
                if (matches[index]->mount_state == UNIT_MOUNT_STATE_RIDER) {
                    return matches[index];
                }
            }
        }
        return matches[0];
    }
    return 0;
}

/* The crystal or treasure lying on a tile, or 0. Among several, the source unit
 * (g_unit_source_runtime_id) is preferred, then the first in list order. The
 * candidate array holds 16 entries and is filled without a bound check. */
unit_t* unit_get_crystal_or_treasure_at_map_coords(s32 map_x, s32 map_y, s32 layer) {
    unit_t* matches[UNIT_RUNTIME_SLOT_COUNT];
    unit_t* unit;
    s32 count;
    s32 index;

    count = 0;
    unit = g_unit_list_head;
    while (unit != 0) {
        if ((unit->map_x == map_x) && (unit->map_y == map_y) && (unit->map_layer == layer) && (unit->stats != 0)
            && (unit->stats->active_battle_id != UNIT_ACTIVE_BATTLE_ID_NONE)
            && ((unit->status_flags_5_6 & (UNIT_STATUS_CRYSTAL | UNIT_STATUS_TREASURE)) != 0)) {
            matches[count++] = unit;
        }
        unit = unit->previous;
    }
    if (count != 0) {
        for (index = 0; index < count; index++) {
            if (matches[index]->runtime_id == g_unit_source_runtime_id) {
                return matches[index];
            }
        }
        return matches[0];
    }
    return 0;
}

/* The first unit drawn with the chocobo sprite sheet at (x, y) on the specified layer that is
 * neither the source nor the casting unit, or 0: the chocobo a unit arriving on
 * the tile mounts (unit_mount_onto_pending). The candidate array
 * holds 16 entries and is filled without a bound check. */
unit_t* unit_find_other_chocobo_at_map_coords(s32 x, s32 y, s32 layer) {
    unit_t* unit;
    unit_t** cursor;
    s32 count;
    s32 id;
    s32 spritesheet_id;
    unit_t* list[UNIT_RUNTIME_SLOT_COUNT];
    unit_t* result;
    s32 index;

    unit = g_unit_list_head;
    count = 0;
    if (unit != 0) {
        spritesheet_id = SPRITE_SHEET_ID_CHOCOBO;
        cursor = list;
        for (; unit != 0; unit = unit->previous) {
            if (unit->map_x == x && unit->map_y == y && unit->map_layer == layer
                && unit->spritesheet_id == spritesheet_id) {
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
            if (id != g_unit_source_runtime_id && id != g_unit_casting_runtime_id) {
                return result;
            }
        }
    }
    return 0;
}

/* Finds another unit standing at (x, y) on the specified layer: one with battle stats in an
 * occupied slot, other than self and, when self is mounted or ridden, other than
 * its mount partner.
 *
 * Among several, the source unit (g_unit_source_runtime_id) is preferred,
 * then the first in list order. Returns 0 for none. The candidate array holds 16
 * entries and is filled without a bound check. */
unit_t* unit_find_overlapping_at_tile(unit_t* self, s32 x, s32 y, s32 layer) {
    unit_t* list[UNIT_RUNTIME_SLOT_COUNT];
    unit_t* unit;
    unit_stats_t* stats;
    s32 count;
    s32 i;

    count = 0;
    for (unit = g_unit_list_head; unit != 0; unit = unit->previous) {
        if (unit != self && unit->map_x == x && unit->map_y == y && unit->map_layer == layer) {
            stats = unit->stats;
            if (stats != 0 && stats->active_battle_id != UNIT_ACTIVE_BATTLE_ID_NONE) {
                if (self->mount_state == UNIT_MOUNT_STATE_NONE || unit->runtime_id != self->mount_partner_runtime_id) {
                    list[count] = unit;
                    count += 1;
                }
            }
        }
    }
    if (count != 0) {
        for (i = 0; i < count; i++) {
            if (list[i]->runtime_id == g_unit_source_runtime_id) {
                return list[i];
            }
        }
        return list[0];
    }
    return 0;
}

/* The runtime (display) record whose runtime_id matches the requested ID, or 0. */
unit_t* unit_get_by_runtime_id(u16 runtime_id) {
    unit_t* unit = g_unit_list_head;

    while (unit != 0) {
        if (unit->runtime_id == runtime_id) {
            return unit;
        }
        unit = unit->previous;
    }
    return 0;
}

/* The runtime (display) record whose battle stats occupy unit slot battle_id, or 0. */
unit_t* unit_get_by_battle_id(u16 battle_id) {
    unit_t* unit = g_unit_list_head;

    while (unit != 0) {
        if ((unit->stats != 0) && (unit->stats->battle_id == battle_id)) {
            return unit;
        }
        unit = unit->previous;
    }
    return 0;
}

/* Count the runtime records drawn with the crystal sprite sheet. */
s32 unit_count_crystals(void) {
    unit_t* unit;
    s32 count = 0;

    unit = g_unit_list_head;
    while (unit != 0) {
        if (unit->spritesheet_id == SPRITE_SHEET_ID_CRYSTAL) {
            count++;
        }
        unit = unit->previous;
    }
    return count;
}

/* Moves a unit's record to the head of the runtime unit list, so list walks reach it
 * first. Returns 1 when moved, 0 when no record has the runtime id. */
s32 unit_move_to_head(u32 runtime_id) {
    unit_t* unit = unit_get_by_runtime_id(runtime_id);
    unit_t** link;
    unit_t** head;

    if (unit != 0) {
        /* Walk link addresses so the list head needs no fictitious unit record. */
        for (link = &g_unit_list_head; link != 0; link = *link != 0 ? &(*link)->previous : 0) {
            if (*link == unit) {
                *link = unit->previous;
                head = &g_unit_list_head;
                unit->previous = *head;
                *head = unit;
                return 1;
            }
        }
        return 0;
    }
    return 0;
}

/* Start reading the SP2 file that holds the attack animation of the unit's SP2 ability into
 * destination. Worker 8 uses its own file table, indexed from animation 0x73. Returns -1 when the
 * animation is in the SP1 range or has no file, else whether the read was queued. */
s32 sprite_open_sp2_file(unit_t* unit, u32* destination) {
    s32 spritesheet_id;
    u8 animation_id;
    s32 index;
    s32 lba;
    s32 byte_length;

    spritesheet_id = g_sprite_sheet_slots[unit->spritesheet_vram_slot].spritesheet_id;
    animation_id = g_ability_animation_data[unit->sp2_ability_id].attack_animation;
    if (spritesheet_id == SPRITE_SHEET_ID_STEEL_GIANT) {
        /* Nothing checks for an animation below 0x73, which would read before the table. */
        index = animation_id - 0x73;
        lba = g_sprite_worker8_sp2_files[index].sector;
        if (lba == 0) {
            return -1;
        }
        if (animation_id >= (u32)unit->animation.seq->first_sp2_sequence >> 1) {
            byte_length = g_sprite_worker8_sp2_files[index].size;
            return file_request_read_bytes(lba, byte_length, destination) != 0;
        }
    } else {
        lba = g_sprite_sp2_files[spritesheet_id].sector;
        if (lba == 0) {
            return -1;
        }
        if (animation_id >= (u32)unit->animation.seq->first_sp2_sequence >> 1) {
            byte_length = g_sprite_sp2_files[spritesheet_id].size;
            return file_request_read_bytes(lba, byte_length, destination) != 0;
        }
    }
    return -1;
}

/* Free the EVTCHR slots whose page holds the unit's SP2 image. */
void sprite_evtchr_invalidate_sp2(unit_t* unit) {
    u32 invalid; /* loaded before the loop, as in the target */
    sprite_evtchr_slot_t* slot;
    s32 count = 0;

    invalid = SPRITE_EVTCHR_SLOT_OWNER_NONE;
    slot = g_sprite_evtchr_slots;
    for (; count < SPRITE_EVTCHR_SLOT_COUNT; count++, slot++) {
        u32 descriptor = slot->owner;

        if (((descriptor & SPRITE_EVTCHR_SLOT_OWNER_ID_MASK) == unit->runtime_id)
            && ((descriptor & SPRITE_EVTCHR_SLOT_OWNER_SP2) != 0)) {
            slot->owner = invalid;
        }
    }
}

/* The next nibble of the compressed spritesheet stream, the high nibble of each
 * byte first. */
static u32 sprite_read_compressed_nibble(void) {
    u32 nibble;
    u8 value;

    if (g_sprite_compressed_nibble_phase == 0) {
        nibble = g_sprite_compressed_data[g_sprite_compressed_offset] >> 4;
    } else {
        value = g_sprite_compressed_data[g_sprite_compressed_offset];
        g_sprite_compressed_offset++;
        nibble = value & 0xf;
    }
    g_sprite_compressed_nibble_phase ^= 1;
    return nibble;
}

/* Expands a nibble-RLE attack spritesheet into 0x6400 bytes of 4bpp pixels.
 *
 * A non-zero nibble is one pixel. A zero nibble starts a run of transparent
 * (index 0) pixels; the next nibble is the run length itself, except that 0
 * takes the length from one more nibble, and 7 and 8 from two or three more,
 * low nibble first. Output packs the first pixel into the high nibble.
 * Decoding stops when the output is full. */
void sprite_decompress_attack_image(const u8* source, u8* destination) {
    s32 high = 0;
    s32 count = 0;
    s32 value;
    s32 i;
    s32 size = 0x6400;
    u8* out = destination;

    g_sprite_compressed_data = source;
    g_sprite_compressed_offset = 0;
    g_sprite_compressed_nibble_phase = 0;
    while (1) {
        value = sprite_read_compressed_nibble();
        if (value == 0) {
            value = sprite_read_compressed_nibble();
            switch (value) {
            case 0:
                value = sprite_read_compressed_nibble();
                break;
            case 7:
                value = sprite_read_compressed_nibble();
                value |= sprite_read_compressed_nibble() << 4;
                break;
            case 8:
                value = sprite_read_compressed_nibble();
                value |= sprite_read_compressed_nibble() << 4;
                value |= sprite_read_compressed_nibble() << 8;
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

/* Reset the unit sprite renderer at battle start: the identity third row and column of
 * the sprite display matrix, depth 0x200 in the GTE projection inputs, the frame
 * counters, and the sprite primitive pool, which begins just past the unit slots. */
static void sprite_init_render_state(void) {
    g_sprite_display_matrix.m[2][2] = ONE;
    g_sprite_gte_rtps_vxy0.vz = 0x200;
    g_sprite_gte_rtpt_vxy2.vz = 0x200;
    g_sprite_gte_rtpt_vxy1.vz = 0x200;
    g_sprite_gte_rtpt_vxy0.vz = 0x200;
    g_sprite_display_matrix.m[2][1] = 0;
    g_sprite_display_matrix.m[2][0] = 0;
    g_sprite_display_matrix.m[1][2] = 0;
    g_sprite_display_matrix.m[0][2] = 0;
    g_sprite_display_matrix.t[2] = 0;
    g_sprite_prim_index = 0;
    g_sprite_prim_frame_start_index = 0;
    g_sprite_unread_2 = 0;
    g_sprite_unread_1 = 0;
    /* The primitive pool begins immediately after the allocation slots. */
    g_sprite_prim_pool = (POLY_FT4*)(g_unit_slot_states + UNIT_RUNTIME_SLOT_COUNT);
}

/* Reset a unit's own sprite display: neutral color, no flags on its eight
 * parts and none of them in use. The display initializers derive addresses from
 * slot bases because direct nested member addresses add instructions. */
sprite_display_t* sprite_display_init_unit(s32 runtime_id) {
    s32 body_part;
    sprite_display_t* display = (sprite_display_t*)((u8*)&g_unit_slot_states[runtime_id]
        + offsetof(unit_slot_state_t, unit.unit_sprite_display.header));

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
sprite_display_t* sprite_display_init_weapon(s32 unit_index, s32 display_type) {
    s32 weapon_part;
    sprite_weapon_display_t* weapons = (sprite_weapon_display_t*)((u8*)&g_unit_slot_states[unit_index]
        + offsetof(unit_slot_state_t, unit.weapon_sprite_displays));
    sprite_display_t* display = &weapons[display_type].header;

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
sprite_display_t* sprite_display_init_numeric(s32 unit_index, s32 display_section) {
    sprite_display_t* display;
    s32 i;

    switch (display_section) {
    case 0:
        display = &g_unit_slot_states[unit_index].unit.numeric_sprite_displays[0].header;
        break;
    case 1:
        display = &g_unit_slot_states[unit_index].unit.numeric_sprite_displays[1].header;
        break;
    case 2:
        display = &g_unit_slot_states[unit_index].unit.numeric_sprite_displays[2].header;
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
sprite_display_t* sprite_display_init_status_bubble(s32 unit_index) {
    sprite_display_t* display = (sprite_display_t*)((u8*)&g_unit_slot_states[unit_index]
        + offsetof(unit_slot_state_t, unit.status_bubble_sprite_display.header));
    s32 i;

    display->color.b = 0x80;
    display->color.g = 0x80;
    display->color.r = 0x80;
    display->scale_y = ONE;
    display->scale_x = ONE;
    display->tpage = SPRITE_TPAGE_FRAME;
    display->clut = BATTLE_MAP_CLUT(BATTLE_MAP_CLUT_ROW_SPRITES, 7);
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
sprite_display_t* sprite_display_init_item(s32 runtime_id) {
    sprite_display_t* display = (sprite_display_t*)((u8*)&g_unit_slot_states[runtime_id]
        + offsetof(unit_slot_state_t, unit.item_sprite_display.header));
    s32 i;

    display->color.b = 0x80;
    display->color.g = 0x80;
    display->color.r = 0x80;
    display->scale_y = ONE;
    display->scale_x = ONE;
    display->tpage = SPRITE_TPAGE_ITEM;
    display->clut = BATTLE_MAP_CLUT(BATTLE_MAP_CLUT_ROW_SPRITES, 7);
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

/* Empty. unit_remove passes the removed unit's sprite display, a member of its
 * unit payload that sprite_display_init_unit handed out. */
void sprite_display_release_unit_stub(sprite_display_t* unused_display) {
}

/* Starts a frame of sprite drawing: the GTE offset is reset to (0, 0) and the
 * primitive ring index reached last frame is recorded. */
static void sprite_init_render_frame(void) {
    SetGeomOffset(0, 0);
    /* The target loads the counter unsigned (lhu). */
    g_sprite_prim_frame_start_index = g_sprite_prim_index;
}

/* Takes the next of the 256 primitives of g_sprite_prim_pool, used as
 * a ring, and returns its index. */
s16 sprite_take_prim(void) {
    g_sprite_prim_index = (g_sprite_prim_index + 1) % 0x100;
    return g_sprite_prim_index;
}

/* Queue one POLY_FT4 per sprite part in [start, end) of a sprite display.
 *
 * The display's rotation, the caller's angle and scale, and position build
 * the GTE matrix. `mode` supplies the whole-sprite flags and blend selector:
 * a flip bit mirrors each part's place about the sprite's origin, and its
 * picture unless the part's own flip bit is set too. Either one's
 * semi-transparent bit applies, and the blend bits of `mode` are added to
 * the display's texture page. A part flagged SPRITE_DISPLAY_NEARER is
 * queued ahead of `ot` by the camera pitch and zoom while the pitch is below
 * a quarter turn.
 *
 * The unused VECTOR reproduces the target's 16-byte locals area. The nested
 * test with its duplicated `addPrim(ot, prim)` arm is the original shape: jump
 * optimization merges the arms, but the references counted before that merge
 * give `ot` the allocation priority that places it in $s5 ahead of `display`. */
void sprite_display_construct_polygon_data(
    sprite_display_t* display, s32 end, s32 start, SVECTOR* position, s16 angle, s16 mode, SVECTOR* scale, u32* ot) {
    VECTOR unused;
    sprite_display_part_t* part;
    POLY_FT4* prim;
    s32 i;
    s16 flags;
    s16 index;
    s16 x;
    s16 y;
    s16 cosine;
    s16 sine;

    cosine = rcos(angle + display->rotation);
    g_sprite_display_matrix.m[0][0] = ((cosine * scale->vx) >> 12) * display->scale_x >> 12;
    g_sprite_display_matrix.m[1][1] = ((cosine * scale->vy) >> 12) * display->scale_y >> 12;
    sine = rsin(angle + display->rotation);
    g_sprite_display_matrix.m[0][1] = ((-sine * scale->vy) >> 12) * display->scale_y >> 12;
    g_sprite_display_matrix.m[1][0] = ((sine * scale->vx) >> 12) * display->scale_x >> 12;
    g_sprite_display_matrix.t[0] = position->vx;
    g_sprite_display_matrix.t[1] = position->vy;
    SetRotMatrix(&g_sprite_display_matrix);
    SetTransMatrix(&g_sprite_display_matrix);
    for (i = start; i < end; i++) {
        part = &display->parts[i];
        flags = part->flags;
        index = sprite_take_prim();
        flags ^= mode;
        prim = g_sprite_prim_pool + index;
        setlen(prim, 9);
        prim->code = 0x2c;
        if (mode & SPRITE_DISPLAY_HORIZONTAL_FLIP) {
            x = -part->x_shift - part->width;
        } else {
            x = part->x_shift;
        }
        if (mode & SPRITE_DISPLAY_VERTICAL_FLIP) {
            y = -part->y_shift - part->height;
        } else {
            y = part->y_shift;
        }
        if (flags & SPRITE_DISPLAY_HORIZONTAL_FLIP) {
            g_sprite_gte_rtpt_vxy1.vx = g_sprite_gte_rtps_vxy0.vx = x;
            g_sprite_gte_rtpt_vxy0.vx = g_sprite_gte_rtpt_vxy2.vx = x + part->width;
        } else {
            g_sprite_gte_rtpt_vxy0.vx = g_sprite_gte_rtpt_vxy2.vx = x;
            g_sprite_gte_rtpt_vxy1.vx = g_sprite_gte_rtps_vxy0.vx = x + part->width;
        }
        if (flags & SPRITE_DISPLAY_VERTICAL_FLIP) {
            g_sprite_gte_rtpt_vxy2.vy = g_sprite_gte_rtps_vxy0.vy = y;
            g_sprite_gte_rtpt_vxy0.vy = g_sprite_gte_rtpt_vxy1.vy = y + part->height;
        } else {
            g_sprite_gte_rtpt_vxy0.vy = g_sprite_gte_rtpt_vxy1.vy = y;
            g_sprite_gte_rtpt_vxy2.vy = g_sprite_gte_rtps_vxy0.vy = y + part->height;
        }
        gte_ldv3_split(&g_sprite_gte_rtpt_vxy0, &g_sprite_gte_rtpt_vxy1, &g_sprite_gte_rtpt_vxy2);
        gte_rtpt();
        if ((flags | mode) & SPRITE_DISPLAY_SEMI_TRANSPARENT) {
            prim->code |= 2;
        } else {
            prim->code &= ~2;
        }
        prim->tpage = display->tpage | (mode & SPRITE_DISPLAY_BLEND_MASK);
        prim->clut = display->clut;
        gte_stsxy3_split(&prim->x0, &prim->x1, &prim->x2);
        gte_ldv0(&g_sprite_gte_rtps_vxy0);
        gte_rtps();
        /* The retail color transfer combines red and green in one 16-bit value. */
        memcpy(&prim->r0, &display->color.r, sizeof(u16));
        prim->b0 = display->color.b;
        gte_stsxy(&prim->x3);
        prim->u0 = prim->u2 = part->u;
        prim->u1 = prim->u3 = prim->u0 + part->width;
        prim->v0 = prim->v1 = part->v;
        prim->v2 = prim->v3 = prim->v0 + part->height;
        if (flags & SPRITE_DISPLAY_NEARER) {
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

/* Fill in part `index` of a sprite display: its offset from the sprite origin, its texture
 * rectangle and its flags. */
void sprite_display_store_data(
    sprite_display_t* display, s32 index, s16 x_shift, s16 y_shift, s16 u, s16 v, s16 width, s16 height, s16 flags) {
    sprite_display_part_t* part;

    part = &display->parts[index];
    part->x_shift = x_shift;
    part->y_shift = y_shift;
    part->u = u;
    part->v = v;
    part->width = width;
    part->height = height;
    part->flags = flags;
}

/* Replace the two mirror bits of a sprite part (SPRITE_DISPLAY_HORIZONTAL_FLIP and
 * _VERTICAL_FLIP) with `flags`. Never called. */
void sprite_display_set_part_flip(sprite_display_t* display, s32 part_index, u32 flags) {
    display->parts[part_index].flags
        = (display->parts[part_index].flags & ~(SPRITE_DISPLAY_HORIZONTAL_FLIP | SPRITE_DISPLAY_VERTICAL_FLIP)) | flags;
}

/* Set or clear bit 0 of a sprite part's flags: the part is drawn semi-transparent while it is set. */
void sprite_display_set_part_semi_transparent(sprite_display_t* display, s32 part_index, s32 enabled) {
    if (enabled != 0) {
        display->parts[part_index].flags |= SPRITE_DISPLAY_SEMI_TRANSPARENT;
    } else {
        display->parts[part_index].flags &= ~SPRITE_DISPLAY_SEMI_TRANSPARENT;
    }
}

/* Move a point half a tile (14 units) on x and z, with signs set by the camera yaw quadrant, back
 * by `offset` on both, and 12 units up: the depth point of a stepping unit. */
static void camera_get_step_depth_point(const SVECTOR* input, SVECTOR* output, s32 offset) {
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

/* camera_get_step_depth_point with offset 1: the depth point of a stepping unit on a
 * tile without BATTLE_MAP_TILE_SURFACE_FLAG_DEPTH_BIAS. */
void camera_get_step_depth_point_unbiased(const SVECTOR* input, SVECTOR* output) {
    camera_get_step_depth_point(input, output, 1);
}

/* camera_get_step_depth_point with offset 5: the depth point of a stepping unit on a
 * tile with BATTLE_MAP_TILE_SURFACE_FLAG_DEPTH_BIAS. */
void camera_get_step_depth_point_biased(const SVECTOR* input, SVECTOR* output) {
    camera_get_step_depth_point(input, output, 5);
}

/*
 * Snap an integer world-space point to the 28-unit tile grid for the current camera
 * yaw, writing the result to output.
 *
 * Same shape as camera_get_step_depth_point: each of the x and z
 * components picks up the caller's offset with a sign that depends on the yaw
 * quadrant, and the y component is a plain bias. Here each component is first
 * snapped down to a whole tile, and on the side the camera faces away from it
 * is moved on by one further tile. Callers pass an offset of 1 or 5. The
 * far_edge temporaries keep the target's add-then-subtract order; written
 * inline, GCC re-associates them into a longer sequence.
 */
static void camera_get_tile_depth_point(const SVECTOR* input, SVECTOR* output, s32 offset) {
    s32 yaw;

    output->vy = input->vy - 0xb;
    yaw = g_camera_render_state.vy & 0xc00;

    switch (yaw) {
    case 0: {
        s32 far_edge = (input->vx / BATTLE_MAP_TILE_SIZE) * BATTLE_MAP_TILE_SIZE + BATTLE_MAP_TILE_SIZE;
        output->vx = far_edge - offset;
    }
        output->vz = (input->vz / BATTLE_MAP_TILE_SIZE) * BATTLE_MAP_TILE_SIZE + offset;
        break;
    case 0x400: {
        s32 far_edge = (input->vx / BATTLE_MAP_TILE_SIZE) * BATTLE_MAP_TILE_SIZE + BATTLE_MAP_TILE_SIZE;
        output->vx = far_edge - offset;
    }
        {
            s32 far_edge = (input->vz / BATTLE_MAP_TILE_SIZE) * BATTLE_MAP_TILE_SIZE + BATTLE_MAP_TILE_SIZE;
            output->vz = far_edge - offset;
        }
        break;
    case 0x800:
        output->vx = (input->vx / BATTLE_MAP_TILE_SIZE) * BATTLE_MAP_TILE_SIZE + offset;
        {
            s32 far_edge = (input->vz / BATTLE_MAP_TILE_SIZE) * BATTLE_MAP_TILE_SIZE + BATTLE_MAP_TILE_SIZE;
            output->vz = far_edge - offset;
        }
        break;
    case 0xc00:
        output->vx = (input->vx / BATTLE_MAP_TILE_SIZE) * BATTLE_MAP_TILE_SIZE + offset;
        output->vz = (input->vz / BATTLE_MAP_TILE_SIZE) * BATTLE_MAP_TILE_SIZE + offset;
        break;
    }
}

/* camera_get_tile_depth_point with offset 1: the depth point of a standing unit,
 * a cursor or a bubble on a tile without BATTLE_MAP_TILE_SURFACE_FLAG_DEPTH_BIAS. */
void camera_get_tile_depth_point_unbiased(const SVECTOR* world, SVECTOR* destination) {
    camera_get_tile_depth_point(world, destination, 1);
}

/* camera_get_tile_depth_point with offset 5: the depth point on a tile with
 * BATTLE_MAP_TILE_SURFACE_FLAG_DEPTH_BIAS. */
void camera_get_tile_depth_point_biased(const SVECTOR* world, SVECTOR* destination) {
    camera_get_tile_depth_point(world, destination, 5);
}

/* The unit's draw position: its world coordinates plus its event and SEQ offsets, with the float
 * bob added to y. */
void unit_copy_draw_coords(SVECTOR* destination, const unit_t* unit) {
    /* float_bob_offset is read signed (lh) here. */
    vector_set_svector(destination, unit->world_coords.vx + unit->event_offset.vx + unit->seq_offset.vx,
        unit->world_coords.vy + unit->event_offset.vy + unit->seq_offset.vy + (s16)unit->float_bob_offset,
        unit->world_coords.vz + unit->event_offset.vz + unit->seq_offset.vz);
}

/* Lays the unit's shadow quad on the terrain surface under the unit.
 *
 * The quad is a 20 x 20 square of world units centered on the unit's position
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
 * sprite_shadow_draw calls it before projecting the quad. */
void sprite_shadow_build_quad(unit_t* unit) {
    SVECTOR coords;
    SVECTOR unused; /* unreferenced; keeps the 0x30-byte target frame */
    battle_map_tile_t* tile;
    SVECTOR* quad;
    u8 layer = unit->map_layer;
    u8 height;
    battle_map_tile_depth_t depth;
    u8 slope_type;
    s32 slope;
    s16 tx;
    s16 ty;
    s32 lhs;
    s32 rhs;
    s32 rise;

    unit_copy_draw_coords(&coords, unit);
    tile = battle_map_tile_get_pointer((coords.vx / BATTLE_MAP_TILE_SIZE), (coords.vz / BATTLE_MAP_TILE_SIZE), layer);
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
    quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy = -(height * BATTLE_MAP_HEIGHT_STEP);

    slope = depth.half_height;
    if (slope == 0) {
        return;
    }

    switch (slope_type) {
    case BATTLE_MAP_SLOPE_EDGE_EAST: /* edge up to east */
        tx = coords.vx % BATTLE_MAP_TILE_SIZE;
        rise = tx * BATTLE_MAP_HEIGHT_STEP;
        quad[0].vy = quad[1].vy = quad[3].vy = quad[2].vy = quad[3].vy - rise * slope / BATTLE_MAP_TILE_SIZE;
        quad[0].vy = quad[2].vy = quad[2].vy + 6;
        quad[1].vy = quad[3].vy = quad[3].vy - 6;
        break;
    case BATTLE_MAP_SLOPE_EDGE_WEST: /* edge up to west */
        tx = coords.vx % BATTLE_MAP_TILE_SIZE;
        rise = tx * BATTLE_MAP_HEIGHT_STEP;
        quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy
            = quad[3].vy - (slope * BATTLE_MAP_HEIGHT_STEP - rise * slope / BATTLE_MAP_TILE_SIZE);
        quad[1].vy = quad[3].vy = quad[3].vy + 6;
        quad[0].vy = quad[2].vy = quad[2].vy - 6;
        break;
    case BATTLE_MAP_SLOPE_EDGE_SOUTH: /* edge up to south */
        ty = coords.vz % BATTLE_MAP_TILE_SIZE;
        rise = ty * BATTLE_MAP_HEIGHT_STEP;
        quad[0].vy = quad[2].vy = quad[3].vy = quad[1].vy
            = quad[3].vy - (slope * BATTLE_MAP_HEIGHT_STEP - rise * slope / BATTLE_MAP_TILE_SIZE);
        quad[0].vy = quad[1].vy = quad[1].vy + 6;
        quad[2].vy = quad[3].vy = quad[3].vy - 6;
        break;
    case BATTLE_MAP_SLOPE_EDGE_NORTH: /* edge up to north */
        ty = coords.vz % BATTLE_MAP_TILE_SIZE;
        rise = ty * BATTLE_MAP_HEIGHT_STEP;
        quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy = quad[3].vy - rise * slope / BATTLE_MAP_TILE_SIZE;
        quad[2].vy = quad[3].vy = quad[3].vy + 6;
        quad[0].vy = quad[1].vy = quad[1].vy - 6;
        break;
    case BATTLE_MAP_SLOPE_LOW_CORNER_NORTH_EAST: /* low corner, north-east up */
        tx = coords.vx % BATTLE_MAP_TILE_SIZE;
        ty = coords.vz % BATTLE_MAP_TILE_SIZE;
        lhs = tx;
        rhs = ty;
        if (lhs < rhs) {
            rise = lhs * BATTLE_MAP_HEIGHT_STEP;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy = quad[3].vy - rise * slope / BATTLE_MAP_TILE_SIZE;
        } else {
            rise = rhs * BATTLE_MAP_HEIGHT_STEP;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy = quad[3].vy - rise * slope / BATTLE_MAP_TILE_SIZE;
        }
        break;
    case BATTLE_MAP_SLOPE_LOW_CORNER_SOUTH_EAST: /* low corner, south-east up */
        tx = coords.vx % BATTLE_MAP_TILE_SIZE;
        ty = coords.vz % BATTLE_MAP_TILE_SIZE;
        lhs = tx;
        rhs = BATTLE_MAP_TILE_SIZE - ty;
        if (lhs >= rhs) {
            rise = ty * BATTLE_MAP_HEIGHT_STEP;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy
                = quad[3].vy - (slope * BATTLE_MAP_HEIGHT_STEP - rise * slope / BATTLE_MAP_TILE_SIZE);
        } else {
            rise = lhs * BATTLE_MAP_HEIGHT_STEP;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy = quad[3].vy - rise * slope / BATTLE_MAP_TILE_SIZE;
        }
        break;
    case BATTLE_MAP_SLOPE_LOW_CORNER_SOUTH_WEST: /* low corner, south-west up */
        tx = coords.vx % BATTLE_MAP_TILE_SIZE;
        ty = coords.vz % BATTLE_MAP_TILE_SIZE;
        lhs = BATTLE_MAP_TILE_SIZE - tx;
        rhs = BATTLE_MAP_TILE_SIZE - ty;
        if (lhs >= rhs) {
            rise = ty * BATTLE_MAP_HEIGHT_STEP;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy
                = quad[3].vy - (slope * BATTLE_MAP_HEIGHT_STEP - rise * slope / BATTLE_MAP_TILE_SIZE);
        } else {
            rise = tx * BATTLE_MAP_HEIGHT_STEP;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy
                = quad[3].vy - (slope * BATTLE_MAP_HEIGHT_STEP - rise * slope / BATTLE_MAP_TILE_SIZE);
        }
        break;
    case BATTLE_MAP_SLOPE_LOW_CORNER_NORTH_WEST: /* low corner, north-west up */
        tx = coords.vx % BATTLE_MAP_TILE_SIZE;
        ty = coords.vz % BATTLE_MAP_TILE_SIZE;
        lhs = BATTLE_MAP_TILE_SIZE - tx;
        rhs = ty;
        if (lhs >= rhs) {
            rise = rhs * BATTLE_MAP_HEIGHT_STEP;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy = quad[3].vy - rise * slope / BATTLE_MAP_TILE_SIZE;
        } else {
            rise = tx * BATTLE_MAP_HEIGHT_STEP;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy
                = quad[3].vy - (slope * BATTLE_MAP_HEIGHT_STEP - rise * slope / BATTLE_MAP_TILE_SIZE);
        }
        break;
    case BATTLE_MAP_SLOPE_HIGH_CORNER_NORTH_EAST: /* high corner, north-east up */
        tx = coords.vx % BATTLE_MAP_TILE_SIZE;
        ty = coords.vz % BATTLE_MAP_TILE_SIZE;
        lhs = tx;
        rhs = ty;
        if (lhs >= rhs) {
            rise = lhs * BATTLE_MAP_HEIGHT_STEP;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy = quad[3].vy - rise * slope / BATTLE_MAP_TILE_SIZE;
        } else {
            rise = rhs * BATTLE_MAP_HEIGHT_STEP;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy = quad[3].vy - rise * slope / BATTLE_MAP_TILE_SIZE;
        }
        break;
    case BATTLE_MAP_SLOPE_HIGH_CORNER_SOUTH_EAST: /* high corner, south-east up */
        tx = coords.vx % BATTLE_MAP_TILE_SIZE;
        ty = coords.vz % BATTLE_MAP_TILE_SIZE;
        lhs = tx;
        rhs = BATTLE_MAP_TILE_SIZE - ty;
        if (lhs >= rhs) {
            rise = lhs * BATTLE_MAP_HEIGHT_STEP;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy = quad[3].vy - rise * slope / BATTLE_MAP_TILE_SIZE;
        } else {
            rise = ty * BATTLE_MAP_HEIGHT_STEP;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy
                = quad[3].vy - (slope * BATTLE_MAP_HEIGHT_STEP - rise * slope / BATTLE_MAP_TILE_SIZE);
        }
        break;
    case BATTLE_MAP_SLOPE_HIGH_CORNER_SOUTH_WEST: /* high corner, south-west up */
        tx = coords.vx % BATTLE_MAP_TILE_SIZE;
        ty = coords.vz % BATTLE_MAP_TILE_SIZE;
        lhs = BATTLE_MAP_TILE_SIZE - tx;
        rhs = BATTLE_MAP_TILE_SIZE - ty;
        if (lhs < rhs) {
            rise = ty * BATTLE_MAP_HEIGHT_STEP;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy
                = quad[3].vy - (slope * BATTLE_MAP_HEIGHT_STEP - rise * slope / BATTLE_MAP_TILE_SIZE);
        } else {
            rise = tx * BATTLE_MAP_HEIGHT_STEP;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy
                = quad[3].vy - (slope * BATTLE_MAP_HEIGHT_STEP - rise * slope / BATTLE_MAP_TILE_SIZE);
        }
        break;
    case BATTLE_MAP_SLOPE_HIGH_CORNER_NORTH_WEST: /* high corner, north-west up */
        tx = coords.vx % BATTLE_MAP_TILE_SIZE;
        ty = coords.vz % BATTLE_MAP_TILE_SIZE;
        lhs = BATTLE_MAP_TILE_SIZE - tx;
        rhs = ty;
        if (lhs >= rhs) {
            rise = tx * BATTLE_MAP_HEIGHT_STEP;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy
                = quad[3].vy - (slope * BATTLE_MAP_HEIGHT_STEP - rise * slope / BATTLE_MAP_TILE_SIZE);
        } else {
            rise = rhs * BATTLE_MAP_HEIGHT_STEP;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy = quad[3].vy - rise * slope / BATTLE_MAP_TILE_SIZE;
        }
        break;
    }
}
