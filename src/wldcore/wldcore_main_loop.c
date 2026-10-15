/* Entrypoint, init, frame draw and exit handling. */

#include "fft/script.h"
#include "fft/wldcore.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* WLDCORE's entrypoint; the overlay is loaded at 0x80067000. */
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
            buffer_index = GsGetActiveBuff();
            g_active_graphics_buffer_index = buffer_index;
            GsSetWorkBase(g_wldcore_gfx_world_primitive_buffers + buffer_index * 0xE000);
            wldcore_fade_step_screen();
            wldcore_map_pulse_dot_colors();
            wldcore_menu_dispatch_idle_handler();
            wldcore_process_frame_gate();
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
                return 5;
            }
            if (flags & MAIN_SYSTEM_FLAG_DEEP_DUNGEON_BATTLE) {
                return 4;
            }
            if (flags & MAIN_SYSTEM_FLAG_TUTORIAL_EVENT) {
                return 2;
            }
            /* Returning through a local keeps two separate `return 2`
             * blocks; a plain `return 2` here cross-jumps into the one above. */
            {
                s32 result;

                if (flags & MAIN_SYSTEM_FLAG_STORY_EVENT_REPLAY) {
                    result = 2;
                } else {
                    result = (flags & MAIN_SYSTEM_FLAG_RANDOM_BATTLE) > 0;
                }
                return result;
            }
        }
    }
}

/* Reinitialize WLDCORE when system-flag bit 1 transitions from set to clear. */
void wldcore_process_frame_gate(void) {
    if ((g_main_system_flags & MAIN_SYSTEM_FLAG_DRAW_MAP) == 0
        && (g_wldcore_previous_system_flags & MAIN_SYSTEM_FLAG_DRAW_MAP) != 0) {
        wldcore_init_core();
    }
    g_wldcore_previous_system_flags = g_main_system_flags;
}

/* Initialize the WLDCORE world buffers, menu state, and runtime tables. */
void wldcore_init_core(void) {
    RECT rect;
    s32 count;

    DrawSync(0);
    VSync(0);
    GsSwapDispBuff();
    count = GsGetActiveBuff();
    g_active_graphics_buffer_index = count;
    GsSetWorkBase(g_wldcore_gfx_world_primitive_buffers + count * 0xE000);
    GsClearOt(0, 0, &g_wldcore_gfx_aux_ordering_tables[g_active_graphics_buffer_index]);
    wldcore_draw_close_indicator_if_pending(&g_wldcore_gfx_aux_ordering_tables[g_active_graphics_buffer_index]);
    wldcore_switch_to_stack(getScratchAddr(255));
    wldcore_gfx_draw_projected_map_tiles(&g_wldcore_gfx_aux_ordering_tables[g_active_graphics_buffer_index]);
    wldcore_map_draw_visible_routes(&g_wldcore_gfx_aux_ordering_tables[g_active_graphics_buffer_index]);
    wldcore_map_project_and_cull_dots();
    window_dispatch_record_list_variant(&g_wldcore_gfx_aux_ordering_tables[g_active_graphics_buffer_index],
        g_wldcore_window_aux_render_object_queue, g_wldcore_window_aux_render_object_count);
    wldcore_restore_previous_stack();
    GsSortClear(0, 0, 0, &g_wldcore_gfx_aux_ordering_tables[g_active_graphics_buffer_index]);
    GsDrawOt(&g_wldcore_gfx_aux_ordering_tables[g_active_graphics_buffer_index]);
    DrawSync(0);
    wldcore_gfx_set_display_rect(g_active_graphics_buffer_index, &rect);
    StoreImage(&rect, g_wldcore_scratch_buffer);
    DrawSync(0);
    g_wldcore_loaded_background_set = -1;
    GsClearOt(0, 0, &g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index]);
    wldcore_map_update_camera_direction_and_projection();
    wldcore_menu_run_world_frame(&g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index]);
    wldcore_gfx_draw_context_value_display(&g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index]);
    wldcore_gfx_draw_calendar_date(&g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index]);
    wldcore_proposition_submit_marker_request(&g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index]);
    window_dispatch_record_list_variant(&g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index],
        g_wldcore_window_render_object_queue, g_wldcore_window_render_object_count);
    GsDrawOt(&g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index]);
    DrawSync(0);
}

/* Per-frame WLDCORE render pass with the 3D map: clears both ordering
 * tables, projects and draws the map tiles into the auxiliary table and the
 * map dots and menus into the main one, then sorts the auxiliary table's
 * clear, draws both tables and clears system flag 2. Sibling of
 * wldcore_gfx_draw_world_frame. */
void wldcore_gfx_draw_world_frame_with_map(void) {
    GsClearOt(0, 0, &g_wldcore_gfx_aux_ordering_tables[g_active_graphics_buffer_index]);
    GsClearOt(0, 0, &g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index]);
    wldcore_draw_close_indicator_if_pending(&g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index]);
    wldcore_switch_to_stack(getScratchAddr(255));
    wldcore_map_project_and_cull_tiles();
    wldcore_gfx_draw_projected_map_tiles(&g_wldcore_gfx_aux_ordering_tables[g_active_graphics_buffer_index]);
    wldcore_restore_previous_stack();
    wldcore_map_step_route_travel();
    wldcore_switch_to_stack(getScratchAddr(255));
    wldcore_map_draw_visible_routes(&g_wldcore_gfx_aux_ordering_tables[g_active_graphics_buffer_index]);
    wldcore_map_draw_path_animation(&g_wldcore_gfx_aux_ordering_tables[g_active_graphics_buffer_index]);
    wldcore_map_project_and_cull_dots();
    wldcore_map_update_camera_direction_and_projection();
    wldcore_restore_previous_stack();
    wldcore_menu_run_world_frame(&g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index]);
    wldcore_switch_to_stack(getScratchAddr(255));
    wldcore_gfx_draw_context_value_display(&g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index]);
    wldcore_gfx_draw_calendar_date(&g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index]);
    wldcore_proposition_submit_marker_request(&g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index]);
    window_dispatch_record_list_variant(&g_wldcore_gfx_aux_ordering_tables[g_active_graphics_buffer_index],
        g_wldcore_window_aux_render_object_queue, g_wldcore_window_aux_render_object_count);
    window_dispatch_record_list_variant(&g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index],
        g_wldcore_window_render_object_queue, g_wldcore_window_render_object_count);
    wldcore_restore_previous_stack();
    DrawSync(0);
    wldcore_reset_game_if_special_keycode_is_pressed();
    VSync(0);
    GsSwapDispBuff();
    GsSortClear(0, 0, 0, &g_wldcore_gfx_aux_ordering_tables[g_active_graphics_buffer_index]);
    GsDrawOt(&g_wldcore_gfx_aux_ordering_tables[g_active_graphics_buffer_index]);
    GsDrawOt(&g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index]);
    g_main_system_flags &= ~MAIN_SYSTEM_FLAG_DRAW_MAP;
}

/* Per-frame WLDCORE render pass: clears the current ordering table, runs the
 * menu frame and marker/display-object passes, draws screen fades, waits for
 * VSync and, unless system flag 0x200 is set, uploads the scratch image to the
 * other display buffer before drawing the ordering table. */
void wldcore_gfx_draw_world_frame(void) {
    RECT rect;

    GsClearOt(0, 0, &g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index]);
    DrawSync(0);
    wldcore_draw_close_indicator_if_pending(&g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index]);
    wldcore_menu_run_world_frame(&g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index]);
    wldcore_switch_to_stack(getScratchAddr(255));
    wldcore_gfx_draw_context_value_display(&g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index]);
    wldcore_gfx_draw_calendar_date(&g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index]);
    wldcore_proposition_submit_marker_request(&g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index]);
    window_dispatch_record_list_variant(&g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index],
        g_wldcore_window_render_object_queue, g_wldcore_window_render_object_count);
    wldcore_fade_draw_screen_overlays();
    wldcore_restore_previous_stack();
    DrawSync(0);
    wldcore_reset_game_if_special_keycode_is_pressed();
    VSync(0);
    GsSwapDispBuff();
    if (!(g_main_system_flags & MAIN_SYSTEM_FLAG_NO_BACKGROUND_IMAGE)) {
        wldcore_gfx_set_display_rect(g_active_graphics_buffer_index == 0, &rect);
        LoadImage(&rect, g_wldcore_scratch_buffer);
    }
    GsDrawOt(&g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index]);
}

/* Initialize the WLDCORE subsystems before entering the world loop.
 * The matched range includes the jr delay slot at 0x80067cb0. */
void wldcore_init_subsystems(void) {
    SetDispMask(0);
    wldcore_init_world_map_state();
    wldcore_input_init_repeat_state();
    wldcore_load_scratch_data_blocking();
    wldcore_gfx_init_subsystem();
    window_init_record_pools_variant();
    wldcore_map_init_tile_atlas();
    wldcore_load_formation_unit_name_and_face();
    wldcore_map_color_and_draw_dots();
    wldcore_init_stub();
    wldcore_map_init_location_marker_and_camera();
    wldcore_menu_reset_window_list_and_store_origin();
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
 * buffered ordering-table descriptors are pointed at their tag arrays.
 *
 * The descriptor stride of 0x14 matches the libgs GsOT record already assumed
 * by src/wldcore/wldcore_init_core.c, which indexes these same globals
 * with `g_active_graphics_buffer_index * 0x14`.
 */

void wldcore_gfx_init_subsystem(void) {
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

void wldcore_init_world_map_state(void) {
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
    g_active_graphics_buffer_index = 0;
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
    g_wldcore_proposition_selected_entry = 0;

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
                wldcore_write_nine_bit_record(g_main_treasure_acquisition_date_bits, i, &date);
            }
            for (i = 0; i < EVENT_SCRIPT_VAR_UNEXPLORED_LAND_COUNT; i++) {
                script_set_variable(EVENT_SCRIPT_VAR_UNEXPLORED_LAND_FIRST + i, 1);
                wldcore_write_nine_bit_record(g_main_land_discovery_date_bits, i, &date);
            }
            for (i = 0; i < EVENT_SCRIPT_VAR_PROPOSITION_COUNT; i++)
                script_set_variable(EVENT_SCRIPT_VAR_PROPOSITION_FIRST + i, 4);
        }

        script_set_variable(EVENT_SCRIPT_VAR_WAR_FUNDS, 0x7530);
        script_set_variable(EVENT_SCRIPT_VAR_SHOP_ITEM_AVAILABILITY, 0x14);
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
        zodiac = ((day_of_year & 0x100) >> 8) | ((month - 1) << 4);
        party->zodiac = zodiac;
    }
}

/* Finalize one WLDCORE loop pass and publish the world status. */
void wldcore_finalize_loop(void) {
    DrawSync(0);
    VSync(0);
    SetDispMask(0);
    wldcore_sound_wait_for_queue_drain();
    if ((g_main_system_flags & MAIN_SYSTEM_FLAG_TUTORIAL_EVENT) == 0) {
        main_sound_unload_scenario_music_and_tunes();
    }
    script_set_variable(EVENT_SCRIPT_VAR_CURRENT_MAP, g_wldcore_next_map_id[0]);
}

/* Initialize runtime tables and graphics state used by WLDCORE.
 * The matched range includes the jr delay slot at 0x800683f8. */
void wldcore_init_runtime_state(void) {
    s32 reserved_stack_words[2];
    s32 count;
    SetDispMask(0);
    g_wldcore_map_projection_state.marker.sub_kind
        = wldcore_proposition_load_picture(g_wldcore_map_projection_state.marker.kind);
    file_wait_for_pending_variant();
    count = GsGetActiveBuff();
    g_active_graphics_buffer_index = count;
    GsSetWorkBase(g_wldcore_gfx_world_primitive_buffers + count * 0xE000);
    wldcore_init_core();
    VSync(0);
    ResetGraph(1);
    GsClearOt(0, 0, &g_wldcore_gfx_aux_ordering_tables[g_active_graphics_buffer_index]);
    GsClearOt(0, 0, &g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index]);
    wldcore_gfx_clear_vram_and_scratch(0);
    GsSwapDispBuff();
    SetDispMask(0);
}

/* Clear system flag 0x10000 and act on the pending world-menu request flags.
 *
 * With none of 0x202c0000 pending, a selected proposition with a Text action
 * reopens its menu level and text (0x5800 + script value); otherwise pending
 * script requests are dispatched. Either way two audio commands follow. Flags
 * 0x40000 and 0x200000 are consumed after publishing g_main_saved_weather_variable as script
 * variable 0x23; each request ends in wldcore_menu_push_screen_transition_level. */
void wldcore_menu_restore_level_on_world_entry(void) {
    s32 flags;

    flags = g_main_system_flags & ~MAIN_SYSTEM_FLAG_RESTART;
    g_main_system_flags = flags;
    if ((flags
            & (MAIN_SYSTEM_FLAG_STORY_EVENT_REPLAY | MAIN_SYSTEM_FLAG_TITLE_TUTORIAL | MAIN_SYSTEM_FLAG_TUTORIAL_EVENT
                | MAIN_SYSTEM_FLAG_TITLE_BRAVE_STORY))
        == 0) {
        if (wldcore_script_process_conditional_set(
                g_wldcore_map_projection_state.marker.kind, WLDCORE_SCRIPT_ACTION_TEXT)
            != 0) {
            wldcore_window_reset_state_and_scroll(&g_wldcore_menu_stack_records[g_wldcore_menu_stack_depth]);
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
        wldcore_menu_push_screen_transition_level(WLDCORE_MENU_LEVEL_TUTORIAL_MASK_1_ENTRIES, 1, 0);
    } else if (flags & MAIN_SYSTEM_FLAG_TITLE_TUTORIAL) {
        wldcore_menu_push_screen_transition_level(WLDCORE_MENU_LEVEL_TUTORIAL_CATEGORIES, 0, 1);
    } else if (flags & MAIN_SYSTEM_FLAG_TITLE_BRAVE_STORY) {
        wldcore_menu_push_screen_transition_level(WLDCORE_MENU_LEVEL_BRAVE_STORY, 0, 0);
    }
}

/* Called at the top of wldcore_entrypoint's loop; a nonzero script
 * `EVENT_SCRIPT_VAR_FORMATION_RETURN_EVENT` clears system flag 0x10000 and
 * makes the entrypoint return. */
s32 wldcore_handle_exit_request(void) {
    if (script_get_variable(EVENT_SCRIPT_VAR_FORMATION_RETURN_EVENT) != 0) {
        g_main_system_flags &= ~MAIN_SYSTEM_FLAG_RESTART;
        world_formation_restore_menu_after_core_exit();
        return 1;
    }

    return 0;
}
