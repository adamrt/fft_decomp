/* WLDCORE entry, the map backdrop update, core and subsystem init, world frame drawing, the clip rect, world map state,
 * loop finalization and the restored menu level. */

#include "fft/data.h"
#include "fft/file.h"
#include "fft/main.h"
#include "fft/main_file.h"
#include "fft/main_gfx.h"
#include "fft/main_save.h"
#include "fft/main_sound.h"
#include "fft/main_unit.h"
#include "fft/menu_input.h"
#include "fft/menu_selection.h"
#include "fft/script_variable.h"
#include "fft/text.h"
#include "fft/unit.h"
#include "fft/unit_party.h"
#include "fft/window.h"
#include "fft/wldcore.h"
#include "fft/wldcore_gfx.h"
#include "fft/wldcore_map.h"
#include "fft/wldcore_menu.h"
#include "fft/wldcore_menu_level.h"
#include "fft/wldcore_proposition.h"
#include "fft/wldcore_script.h"
#include "fft/wldcore_sound.h"
#include "fft/wldcore_sound_novel.h"
#include "fft/wldcore_text.h"
#include "fft/wldcore_window.h"
#include "fft/world.h"
#include "fft/world_formation.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/libgs.h"
#include "psx/libgte.h"
#include "psx/pad.h"
#include "psx/types.h"

static void wldcore_finalize_loop(void);
static void wldcore_gfx_draw_world_frame_with_map(void);
static void wldcore_gfx_init_subsystem(void);
static s32 wldcore_handle_exit_request(void);
static void wldcore_init_runtime_state(void);
static void wldcore_init_subsystems(void);
static void wldcore_init_world_map_state(void);
static void wldcore_menu_restore_level_on_world_entry(void);
static void wldcore_gfx_update_map_backdrop(void);

/* WLDCORE's main loop: set up, fade in and run world-map frames until a menu level
 * stops it, then return why as a main_world_result_e. A card-menu restart loops
 * instead. */
s32 wldcore_entrypoint(void) {
    s32 buffer_index;

    for (;;) {
        if (wldcore_handle_exit_request() != 0) {
            return 0;
        }
        wldcore_init_subsystems();
        wldcore_fade_start_screen(0, 0x10);
        for (;;) {
            if ((g_main_system_flags & MAIN_SYSTEM_FLAG_RUNNING) == 0) {
                break;
            }
            main_file_poll_load(&g_main_file_cd_state);
            wldcore_sound_process_audio_queue();
            /* Each display buffer has its own 0xE000-byte GS packet area. */
            buffer_index = GsGetActiveBuff();
            g_main_gfx_active_buffer_index = buffer_index;
            GsSetWorkBase(g_wldcore_gfx_world_primitive_buffers + buffer_index * 0xE000);
            wldcore_fade_step_screen();
            wldcore_map_pulse_dot_colors();
            wldcore_menu_dispatch_idle_handler();
            wldcore_gfx_update_map_backdrop();
            if (g_main_system_flags & MAIN_SYSTEM_FLAG_DRAW_MAP) {
                wldcore_gfx_draw_world_frame_with_map();
            } else {
                wldcore_gfx_draw_world_frame();
            }
            if (g_main_system_flags & MAIN_SYSTEM_FLAG_DISPLAY_OFF) {
                SetDispMask(0);
            } else {
                SetDispMask(1);
            }
            main_system_frame_stub();
        }
        if (g_main_system_flags & MAIN_SYSTEM_FLAG_RESTART) {
            continue;
        }
        wldcore_finalize_loop();
        {
            s32 flags = g_main_system_flags;

            if (flags & MAIN_SYSTEM_FLAG_RETURN_TO_TITLE) {
                return MAIN_WORLD_RESULT_TITLE;
            }
            if (flags & MAIN_SYSTEM_FLAG_DEEP_DUNGEON_BATTLE) {
                return MAIN_WORLD_RESULT_DEEP_DUNGEON_BATTLE;
            }
            if (flags & MAIN_SYSTEM_FLAG_TUTORIAL_EVENT) {
                return MAIN_WORLD_RESULT_EVENT;
            }
            /* Returning through a local keeps two separate MAIN_WORLD_RESULT_EVENT
             * returns; a plain return here cross-jumps into the one above. */
            {
                s32 result;

                if (flags & MAIN_SYSTEM_FLAG_STORY_EVENT_REPLAY) {
                    result = MAIN_WORLD_RESULT_EVENT;
                } else {
                    /* MAIN_WORLD_RESULT_RANDOM_BATTLE or MAIN_WORLD_RESULT_NONE */
                    result = (flags & MAIN_SYSTEM_FLAG_RANDOM_BATTLE) > 0;
                }
                return result;
            }
        }
    }
}

/* When MAIN_SYSTEM_FLAG_DRAW_MAP goes from set to clear, capture the map
 * backdrop again (wldcore_gfx_capture_map_backdrop) for the frames that no longer redraw the
 * map. */
static void wldcore_gfx_update_map_backdrop(void) {
    if ((g_main_system_flags & MAIN_SYSTEM_FLAG_DRAW_MAP) == 0
        && (g_wldcore_previous_system_flags & MAIN_SYSTEM_FLAG_DRAW_MAP) != 0) {
        wldcore_gfx_capture_map_backdrop();
    }
    g_wldcore_previous_system_flags = g_main_system_flags;
}

/* Capture the map backdrop: draw the map tiles, routes, dots and auxiliary
 * records once, store the drawn image in g_wldcore_scratch_buffer (which
 * wldcore_gfx_draw_world_frame uploads behind each later frame), then draw
 * the menu frame over it. */
void wldcore_gfx_capture_map_backdrop(void) {
    RECT rect;
    s32 buffer_index;

    DrawSync(0);
    VSync(0);
    GsSwapDispBuff();
    buffer_index = GsGetActiveBuff();
    g_main_gfx_active_buffer_index = buffer_index;
    GsSetWorkBase(g_wldcore_gfx_world_primitive_buffers + buffer_index * 0xE000);
    GsClearOt(0, 0, &g_wldcore_gfx_aux_ordering_tables[g_main_gfx_active_buffer_index]);
    wldcore_draw_close_indicator_if_pending(&g_wldcore_gfx_aux_ordering_tables[g_main_gfx_active_buffer_index]);
    /* The map passes run with the stack moved into the 1 KB scratchpad, starting
     * at its last word. */
    wldcore_switch_to_stack(getScratchAddr(255));
    wldcore_gfx_draw_projected_map_tiles(&g_wldcore_gfx_aux_ordering_tables[g_main_gfx_active_buffer_index]);
    wldcore_map_draw_visible_routes(&g_wldcore_gfx_aux_ordering_tables[g_main_gfx_active_buffer_index]);
    wldcore_map_project_and_cull_dots();
    window_dispatch_record_list(&g_wldcore_gfx_aux_ordering_tables[g_main_gfx_active_buffer_index],
        g_wldcore_window_aux_render_object_queue, g_wldcore_window_aux_render_object_count);
    wldcore_restore_previous_stack();
    GsSortClear(0, 0, 0, &g_wldcore_gfx_aux_ordering_tables[g_main_gfx_active_buffer_index]);
    GsDrawOt(&g_wldcore_gfx_aux_ordering_tables[g_main_gfx_active_buffer_index]);
    DrawSync(0);
    wldcore_gfx_set_display_rect(g_main_gfx_active_buffer_index, &rect);
    StoreImage(&rect, g_wldcore_scratch_buffer);
    DrawSync(0);
    g_wldcore_loaded_background_set = -1;
    GsClearOt(0, 0, &g_wldcore_gfx_ordering_tables[g_main_gfx_active_buffer_index]);
    wldcore_map_update_camera_direction_and_projection();
    wldcore_menu_run_world_frame(&g_wldcore_gfx_ordering_tables[g_main_gfx_active_buffer_index]);
    wldcore_gfx_draw_context_value_display(&g_wldcore_gfx_ordering_tables[g_main_gfx_active_buffer_index]);
    wldcore_gfx_draw_calendar_date(&g_wldcore_gfx_ordering_tables[g_main_gfx_active_buffer_index]);
    wldcore_map_draw_location_label(&g_wldcore_gfx_ordering_tables[g_main_gfx_active_buffer_index]);
    window_dispatch_record_list(&g_wldcore_gfx_ordering_tables[g_main_gfx_active_buffer_index],
        g_wldcore_window_render_object_queue, g_wldcore_window_render_object_count);
    GsDrawOt(&g_wldcore_gfx_ordering_tables[g_main_gfx_active_buffer_index]);
    DrawSync(0);
}

/* Per-frame WLDCORE render pass with the 3D map: clears both ordering
 * tables, projects and draws the map tiles into the auxiliary table and the
 * map dots and menus into the main one, then sorts the auxiliary table's
 * clear, draws both tables and clears MAIN_SYSTEM_FLAG_DRAW_MAP. The map and
 * HUD passes run on the scratchpad stack. Sibling of
 * wldcore_gfx_draw_world_frame. */
static void wldcore_gfx_draw_world_frame_with_map(void) {
    GsClearOt(0, 0, &g_wldcore_gfx_aux_ordering_tables[g_main_gfx_active_buffer_index]);
    GsClearOt(0, 0, &g_wldcore_gfx_ordering_tables[g_main_gfx_active_buffer_index]);
    wldcore_draw_close_indicator_if_pending(&g_wldcore_gfx_ordering_tables[g_main_gfx_active_buffer_index]);
    wldcore_switch_to_stack(getScratchAddr(255));
    wldcore_map_project_and_cull_tiles();
    wldcore_gfx_draw_projected_map_tiles(&g_wldcore_gfx_aux_ordering_tables[g_main_gfx_active_buffer_index]);
    wldcore_restore_previous_stack();
    wldcore_map_step_route_travel();
    wldcore_switch_to_stack(getScratchAddr(255));
    wldcore_map_draw_visible_routes(&g_wldcore_gfx_aux_ordering_tables[g_main_gfx_active_buffer_index]);
    wldcore_map_draw_path_animation(&g_wldcore_gfx_aux_ordering_tables[g_main_gfx_active_buffer_index]);
    wldcore_map_project_and_cull_dots();
    wldcore_map_update_camera_direction_and_projection();
    wldcore_restore_previous_stack();
    wldcore_menu_run_world_frame(&g_wldcore_gfx_ordering_tables[g_main_gfx_active_buffer_index]);
    wldcore_switch_to_stack(getScratchAddr(255));
    wldcore_gfx_draw_context_value_display(&g_wldcore_gfx_ordering_tables[g_main_gfx_active_buffer_index]);
    wldcore_gfx_draw_calendar_date(&g_wldcore_gfx_ordering_tables[g_main_gfx_active_buffer_index]);
    wldcore_map_draw_location_label(&g_wldcore_gfx_ordering_tables[g_main_gfx_active_buffer_index]);
    window_dispatch_record_list(&g_wldcore_gfx_aux_ordering_tables[g_main_gfx_active_buffer_index],
        g_wldcore_window_aux_render_object_queue, g_wldcore_window_aux_render_object_count);
    window_dispatch_record_list(&g_wldcore_gfx_ordering_tables[g_main_gfx_active_buffer_index],
        g_wldcore_window_render_object_queue, g_wldcore_window_render_object_count);
    wldcore_restore_previous_stack();
    DrawSync(0);
    wldcore_input_update_controller();
    VSync(0);
    GsSwapDispBuff();
    GsSortClear(0, 0, 0, &g_wldcore_gfx_aux_ordering_tables[g_main_gfx_active_buffer_index]);
    GsDrawOt(&g_wldcore_gfx_aux_ordering_tables[g_main_gfx_active_buffer_index]);
    GsDrawOt(&g_wldcore_gfx_ordering_tables[g_main_gfx_active_buffer_index]);
    g_main_system_flags &= ~MAIN_SYSTEM_FLAG_DRAW_MAP;
}

/* Per-frame WLDCORE render pass: clears the current ordering table, runs the
 * menu frame and marker/display-object passes, draws screen fades, waits for
 * VSync and, unless MAIN_SYSTEM_FLAG_NO_BACKGROUND_IMAGE is set, uploads the
 * captured map backdrop to the new draw buffer before drawing the ordering
 * table over it. */
void wldcore_gfx_draw_world_frame(void) {
    RECT rect;

    GsClearOt(0, 0, &g_wldcore_gfx_ordering_tables[g_main_gfx_active_buffer_index]);
    DrawSync(0);
    wldcore_draw_close_indicator_if_pending(&g_wldcore_gfx_ordering_tables[g_main_gfx_active_buffer_index]);
    wldcore_menu_run_world_frame(&g_wldcore_gfx_ordering_tables[g_main_gfx_active_buffer_index]);
    wldcore_switch_to_stack(getScratchAddr(255));
    wldcore_gfx_draw_context_value_display(&g_wldcore_gfx_ordering_tables[g_main_gfx_active_buffer_index]);
    wldcore_gfx_draw_calendar_date(&g_wldcore_gfx_ordering_tables[g_main_gfx_active_buffer_index]);
    wldcore_map_draw_location_label(&g_wldcore_gfx_ordering_tables[g_main_gfx_active_buffer_index]);
    window_dispatch_record_list(&g_wldcore_gfx_ordering_tables[g_main_gfx_active_buffer_index],
        g_wldcore_window_render_object_queue, g_wldcore_window_render_object_count);
    wldcore_fade_draw_screen_overlays();
    wldcore_restore_previous_stack();
    DrawSync(0);
    wldcore_input_update_controller();
    VSync(0);
    GsSwapDispBuff();
    if (!(g_main_system_flags & MAIN_SYSTEM_FLAG_NO_BACKGROUND_IMAGE)) {
        wldcore_gfx_set_display_rect(g_main_gfx_active_buffer_index == 0, &rect);
        LoadImage(&rect, g_wldcore_scratch_buffer);
    }
    GsDrawOt(&g_wldcore_gfx_ordering_tables[g_main_gfx_active_buffer_index]);
}

/* Initialize the WLDCORE subsystems before entering the world loop, ending
 * with the menu level the world entry opens. */
static void wldcore_init_subsystems(void) {
    SetDispMask(0);
    wldcore_init_world_map_state();
    wldcore_input_init_repeat_state();
    wldcore_map_load_texture();
    wldcore_gfx_init_subsystem();
    window_init_record_pools();
    wldcore_map_init_tile_atlas();
    wldcore_system_init_world_menus_and_date();
    wldcore_map_init_dots();
    wldcore_map_init_stub();
    wldcore_map_init_location_marker_and_camera();
    wldcore_menu_reset_stack();
    wldcore_init_runtime_state();
    wldcore_menu_restore_level_on_world_entry();
}

/* Clear the whole 256x480 frame buffer, then install the draw-clip rectangle
 * for the requested world-map layout: mode 0 leaves an 8-pixel top margin and
 * a 252x228 area, any other mode uses the full 256x238 screen. */
void wldcore_clear_screen_and_set_map_clip_rect(s32 mode) {
    RECT rect;

    rect.x = 0;
    rect.y = 0;
    rect.w = 256;
    rect.h = 480;
    g_main_system_flags |= MAIN_SYSTEM_FLAG_SCREEN_CLEARED;
    ClearImage(&rect, 0, 0, 0);
    DrawSync(0);
    if (mode == 0) {
        rect.x = 0;
        rect.y = 8;
        rect.w = 252;
        rect.h = 228;
    } else {
        rect.x = 0;
        rect.y = 0;
        rect.w = 256;
        rect.h = 238;
    }
    GsSetClip2D(&rect);
    GsSetDrawBuffClip();
}

/*
 * WORLD-map graphics-subsystem bring-up: display mask off, GS graph init at
 * 256x240, draw offsets/3D init, GTE screen offset, then the two double-
 * buffered ordering-table descriptors are pointed at their tag arrays. A GsOT
 * length is a power of two: the auxiliary tables have 4 entries and the main
 * tables 16.
 */
static void wldcore_gfx_init_subsystem(void) {
    SetDispMask(0);
    GsInitGraph2(256, 240, 4, 0, 0);
    GsDefDispBuff(0, 0, 0, 240);
    GsInit3D();
    SetGeomOffset(0, 0);
    wldcore_clear_screen_and_set_map_clip_rect(0);

    g_wldcore_gfx_aux_ordering_tables[1].length = 2;
    g_wldcore_gfx_aux_ordering_tables[0].length = 2;
    g_wldcore_gfx_aux_ordering_tables[0].org = g_wldcore_gfx_aux_ot_tags[0];
    g_wldcore_gfx_aux_ordering_tables[1].org = g_wldcore_gfx_aux_ot_tags[1];

    g_wldcore_gfx_ordering_tables[1].length = 4;
    g_wldcore_gfx_ordering_tables[0].length = 4;
    g_wldcore_gfx_ordering_tables[0].org = g_wldcore_gfx_ot_tags[0];
    g_wldcore_gfx_ordering_tables[1].org = g_wldcore_gfx_ot_tags[1];
}

/* Resets state used when entering the world map.
 *
 * The target derives each fade box's 32-bit attribute through the record base
 * while it walks the rectangle fields separately.
 */
static void wldcore_init_world_map_state(void) {
    menu_cursor_state_t* cursor;
    GsBOXF* box;
    party_unit_t* party;
    calendar_date_t date;
    s32 day_of_year;
    s32 zodiac;
    s32 month;
    s32 day;
    s32 i;

    g_wldcore_scratch_buffer = (u32*)0x801d0000;
    g_wldcore_loaded_message_block = -1;
    g_wldcore_loaded_picture_set = -1;
    g_wldcore_menu_ordering_table_offset = 1;
    g_wldcore_screen_brightness_rgb.r = 0x80;
    g_wldcore_screen_brightness_rgb.g = 0x80;
    g_wldcore_screen_brightness_rgb.b = 0x80;
    g_wldcore_previous_system_flags = 0;
    g_main_gfx_active_buffer_index = 0;
    g_wldcore_map_zoom_motion.flags = 0;
    g_wldcore_random_battle_squad_id = 0;
    g_wldcore_random_battle_entd_id = 0;
    g_menu_blocked_buttons = 0;
    g_main_system_flags
        = (g_main_system_flags
              & (MAIN_SYSTEM_FLAG_RESTART | MAIN_SYSTEM_FLAG_STORY_EVENT_REPLAY | MAIN_SYSTEM_FLAG_TITLE_TUTORIAL
                  | MAIN_SYSTEM_FLAG_TUTORIAL_EVENT | MAIN_SYSTEM_FLAG_UNLOCK_ALL | MAIN_SYSTEM_FLAG_TITLE_BRAVE_STORY))
        | MAIN_SYSTEM_FLAG_RUNNING;
    g_wldcore_script_base = g_wldcore_default_script_base;
    g_wldcore_sound_novel_sound_resource = g_wldcore_default_sound_novel_sound_resource;
    /* Tile descriptors share the buffer later used for picture TIMs. */
    g_wldcore_picture_buffer = (u32*)g_wldcore_map_tile_descriptors;

    for (i = 0; i < 2; i++) {
        g_wldcore_screen_fade_state.flags[i] = 0;
        g_wldcore_screen_fade_state.priorities[i] = 0xf;
        g_wldcore_screen_fade_state.boxes[i].attribute = 0x60000000;
        box = &g_wldcore_screen_fade_state.boxes[i];
        box->x = -0x80;
        box->y = -0x78;
        box->w = 0x100;
        box->h = 0xf0;
        box->r = 0;
        box->g = 0;
        box->b = 0;
    }
    g_wldcore_screen_fade_state.flags[0] = WLDCORE_SCREEN_FADE_OUT;

    cursor = g_wldcore_menu_cursor_states;
    for (i = 0; i < WLDCORE_CURSOR_SLOT_COUNT; i++) {
        cursor->scroll_offset = 0;
        g_wldcore_menu_cursor_states[i].selected_index = 0;
        cursor++;
    }

    g_wldcore_hud_color.r = 0x80;
    g_wldcore_hud_color.g = 0x80;
    g_wldcore_hud_color.b = 0x80;
    g_wldcore_calendar_display_x = -0x78;
    g_wldcore_calendar_display_y = -0x64;
    g_wldcore_location_marker_shade = 0x80;
    g_wldcore_map_selected_location = 0;

    if (!(g_main_system_flags & MAIN_SYSTEM_FLAG_TUTORIAL_EVENT)) {
        main_sound_unload_scenario_music_and_tunes();
    }

    g_wldcore_audio_queue.volume = 0x7f;
    g_wldcore_context_value_display_position.x = 0x30;
    g_wldcore_context_value_display_position.y = 0x54;
    g_wldcore_hud_ot_priority = 4;
    g_wldcore_audio_queue.count = 0;
    g_wldcore_audio_queue.music.flags = 0;
    g_wldcore_audio_queue.music.tracks[1] = 0;
    g_wldcore_audio_queue.music.tracks[0] = 0;
    g_wldcore_audio_queue.current_command = WLDCORE_AUDIO_COMMAND_NONE;
    g_wldcore_sound_novel_countdown_frames = 0;
    g_wldcore_location_entry_state.flags = 0;

    /* Debug unlock (nothing sets MAIN_SYSTEM_FLAG_UNLOCK_ALL): every profile,
     * rumor, route, location and story event; unless Start is held, every
     * treasure and unexplored land found on 1 January and every proposition
     * variable at 4; then 30,000 war funds, shop stock level 20, propositions
     * and the fur shop enabled, and a 1 January birthday for the first party
     * member. */
    if (g_main_system_flags & MAIN_SYSTEM_FLAG_UNLOCK_ALL) {
        for (i = 0; i < EVENT_SCRIPT_VAR_PERSON_PROFILE_COUNT; i++)
            script_set_variable(EVENT_SCRIPT_VAR_PERSON_PROFILE_FIRST + i, 1);
        for (i = 0; i < EVENT_SCRIPT_VAR_RUMOR_COUNT; i++)
            script_set_variable(EVENT_SCRIPT_VAR_RUMOR_FIRST + i, 1);
        for (i = 0; i < WLDCORE_ROUTE_COUNT; i++)
            script_set_variable(EVENT_SCRIPT_VAR_PATH_VISIBLE_FIRST + i, 1);
        for (i = 0; i < WORLD_LOCATION_COUNT; i++)
            script_set_variable(EVENT_SCRIPT_VAR_LOCATION_VISIBLE_FIRST + i, 1);
        for (i = 0; i < EVENT_SCRIPT_VAR_STORY_EVENT_COUNT; i++)
            script_set_variable(EVENT_SCRIPT_VAR_STORY_EVENT_FIRST + i, 1);

        if (!(PadRead(0) & PSX_PAD_START)) {
            date.month = 1;
            date.day = 1;
            for (i = 0; i < EVENT_SCRIPT_VAR_TREASURE_COUNT; i++) {
                script_set_variable(EVENT_SCRIPT_VAR_TREASURE_FIRST + i, 1);
                wldcore_calendar_write_date(g_main_treasure_acquisition_date_bits, i, &date);
            }
            for (i = 0; i < EVENT_SCRIPT_VAR_UNEXPLORED_LAND_COUNT; i++) {
                script_set_variable(EVENT_SCRIPT_VAR_UNEXPLORED_LAND_FIRST + i, 1);
                wldcore_calendar_write_date(g_main_land_discovery_date_bits, i, &date);
            }
            for (i = 0; i < EVENT_SCRIPT_VAR_PROPOSITION_COUNT; i++)
                script_set_variable(EVENT_SCRIPT_VAR_PROPOSITION_FIRST + i, 4);
        }

        script_set_variable(EVENT_SCRIPT_VAR_WAR_FUNDS, 30000);
        script_set_variable(EVENT_SCRIPT_VAR_SHOP_ITEM_AVAILABILITY, 20);
        script_set_variable(EVENT_SCRIPT_VAR_PROPOSITIONS_ENABLED, 1);
        script_set_variable(EVENT_SCRIPT_VAR_FUR_SHOP_ENABLED, 1);

        month = 1;
        day = 1;
        script_set_variable(EVENT_SCRIPT_VAR_PLAYER_BIRTH_MONTH, 1);
        script_set_variable(EVENT_SCRIPT_VAR_PLAYER_BIRTH_DAY, day);
        day_of_year = unit_birthday_calculate_day_of_year(month, day);
        wldcore_calendar_convert_date_to_zodiac(&month, &day);
        party = main_party_get_unit(0);
        party->birthday_day = day_of_year;
        /* The separate s32 temp keeps the `month` reload a full 32-bit value: folding
         * the expression straight into the u8 field narrows it to an lbu. */
        zodiac = ((day_of_year & (PARTY_BIRTHDAY_DAY_HIGH_BIT_MASK << 8)) >> 8) | ((month - 1) << PARTY_ZODIAC_SHIFT);
        party->zodiac = zodiac;
    }
}

/* Finalize one WLDCORE loop pass and publish the world status. */
static void wldcore_finalize_loop(void) {
    DrawSync(0);
    VSync(0);
    SetDispMask(0);
    wldcore_sound_wait_for_queue_drain();
    if ((g_main_system_flags & MAIN_SYSTEM_FLAG_TUTORIAL_EVENT) == 0) {
        main_sound_unload_scenario_music_and_tunes();
    }
    script_set_variable(EVENT_SCRIPT_VAR_CURRENT_MAP, g_wldcore_next_map_id[0]);
}

/* Load the picture for the marker's location, capture the first map backdrop,
 * then reset the GPU, clear both ordering tables and VRAM, and swap buffers
 * with the display still off. */
static void wldcore_init_runtime_state(void) {
    s32 reserved_stack_words[2];
    s32 buffer_index;

    SetDispMask(0);
    g_wldcore_map_projection_state.marker.sub_kind
        = wldcore_gfx_load_location_picture(g_wldcore_map_projection_state.marker.kind);
    file_wait_for_pending();
    buffer_index = GsGetActiveBuff();
    g_main_gfx_active_buffer_index = buffer_index;
    GsSetWorkBase(g_wldcore_gfx_world_primitive_buffers + buffer_index * 0xE000);
    wldcore_gfx_capture_map_backdrop();
    VSync(0);
    ResetGraph(1);
    GsClearOt(0, 0, &g_wldcore_gfx_aux_ordering_tables[g_main_gfx_active_buffer_index]);
    GsClearOt(0, 0, &g_wldcore_gfx_ordering_tables[g_main_gfx_active_buffer_index]);
    wldcore_gfx_clear_vram_and_scratch(0);
    GsSwapDispBuff();
    SetDispMask(0);
}

/* Clear MAIN_SYSTEM_FLAG_RESTART and open the menu level the world entry
 * starts on.
 *
 * On an ordinary entry, a Text action staged by the marker location's map
 * script opens a scrollable window on help message
 * TEXT_ID_WORLD_HELP_MESSAGE_BASE + its argument; otherwise the pending script
 * requests are dispatched. Either way the world map music is queued and faded
 * in.
 *
 * Returning from a story-event replay or a tutorial event consumes its flag,
 * restores the weather script variable saved before the event and reopens the
 * list it was chosen from (the tutorial with its two music tracks queued). The
 * title's tutorial and Brave Story requests open those screens. These all go
 * through wldcore_menu_push_screen_transition_level. */
static void wldcore_menu_restore_level_on_world_entry(void) {
    s32 flags = g_main_system_flags & ~MAIN_SYSTEM_FLAG_RESTART;

    g_main_system_flags = flags;
    if ((flags
            & (MAIN_SYSTEM_FLAG_STORY_EVENT_REPLAY | MAIN_SYSTEM_FLAG_TITLE_TUTORIAL | MAIN_SYSTEM_FLAG_TUTORIAL_EVENT
                | MAIN_SYSTEM_FLAG_TITLE_BRAVE_STORY))
        == 0) {
        if (wldcore_script_process_conditional_set(
                g_wldcore_map_projection_state.marker.kind, WLDCORE_SCRIPT_ACTION_TEXT)
            != 0) {
            wldcore_menu_suspend_root_level(&g_wldcore_menu_stack_records[g_wldcore_menu_stack_depth]);
            wldcore_menu_push_scrollable_text_window_level(
                g_wldcore_script_state.args[0] + TEXT_ID_WORLD_HELP_MESSAGE_BASE);
        } else {
            wldcore_menu_dispatch_pending_script_requests();
        }
        wldcore_sound_enqueue_audio_command(
            WLDCORE_AUDIO_COMMAND_PLAY_MUSIC, WLDCORE_AUDIO_MUSIC(1, MAIN_MUSIC_WORLD_MAP));
        wldcore_sound_enqueue_audio_command(WLDCORE_AUDIO_COMMAND_FADE_IN, 4);
        return;
    }
    if (flags & MAIN_SYSTEM_FLAG_STORY_EVENT_REPLAY) {
        g_main_system_flags = flags ^ MAIN_SYSTEM_FLAG_STORY_EVENT_REPLAY;
        script_set_variable(EVENT_SCRIPT_VAR_WEATHER, g_main_saved_weather_variable);
        wldcore_menu_push_screen_transition_level(WLDCORE_MENU_LEVEL_STORY_EVENTS, 0, 0);
    } else if (flags & MAIN_SYSTEM_FLAG_TUTORIAL_EVENT) {
        g_main_system_flags = flags ^ MAIN_SYSTEM_FLAG_TUTORIAL_EVENT;
        script_set_variable(EVENT_SCRIPT_VAR_WEATHER, g_main_saved_weather_variable);
        g_wldcore_audio_queue.music.tracks[0] = 0x1B;
        g_wldcore_audio_queue.music.tracks[1] = 0x1A;
        g_wldcore_audio_queue.current_slot = 2;
        wldcore_menu_push_screen_transition_level(WLDCORE_MENU_LEVEL_BATTLE_TUTORIALS, 1, 0);
    } else if (flags & MAIN_SYSTEM_FLAG_TITLE_TUTORIAL) {
        wldcore_menu_push_screen_transition_level(WLDCORE_MENU_LEVEL_TUTORIAL_CATEGORIES, 0, 1);
    } else if (flags & MAIN_SYSTEM_FLAG_TITLE_BRAVE_STORY) {
        wldcore_menu_push_screen_transition_level(WLDCORE_MENU_LEVEL_BRAVE_STORY, 0, 0);
    }
}

/* Called at the top of wldcore_entrypoint's loop. A nonzero
 * EVENT_SCRIPT_VAR_FORMATION_RETURN_EVENT clears MAIN_SYSTEM_FLAG_RESTART,
 * restores WORLD's formation menu and makes the entrypoint return 0. */
static s32 wldcore_handle_exit_request(void) {
    if (script_get_variable(EVENT_SCRIPT_VAR_FORMATION_RETURN_EVENT) != 0) {
        g_main_system_flags &= ~MAIN_SYSTEM_FLAG_RESTART;
        world_formation_restore_menu_after_core_exit();
        return 1;
    }

    return 0;
}
