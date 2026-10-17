/* WLDCORE entry, the map backdrop update, core and subsystem init, world frame drawing, the clip rect, world map state,
 * loop finalization and the restored menu level. */

#include "fft/brave_story.h"
#include "fft/calendar.h"
#include "fft/file.h"
#include "fft/formation.h"
#include "fft/gfx.h"
#include "fft/location.h"
#include "fft/menu_input.h"
#include "fft/menu_selection.h"
#include "fft/menu_stack.h"
#include "fft/menu_stack_records.h"
#include "fft/message.h"
#include "fft/music.h"
#include "fft/party.h"
#include "fft/script_variable.h"
#include "fft/sound.h"
#include "fft/sound_novel.h"
#include "fft/system.h"
#include "fft/text.h"
#include "fft/unit_party.h"
#include "fft/window.h"
#include "fft/world_map.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/libgs.h"
#include "psx/libgte.h"
#include "psx/pad.h"
#include "psx/types.h"

static void world_map_finalize_loop(void);
static void world_map_draw_frame_with_map(void);
static void world_map_init_gfx(void);
static s32 world_map_handle_exit_request(void);
static void world_map_init_screen(void);
static void world_map_init_subsystems(void);
static void world_map_init_state(void);
static void world_map_restore_entry_level(void);
static void world_map_update_backdrop(void);

/* WLDCORE's main loop: set up, fade in and run world-map frames until a menu level
 * stops it, then return why as a world_map_result_e. A card-menu restart loops
 * instead. */
s32 wldcore_entrypoint(void) {
    s32 buffer_index;

    for (;;) {
        if (world_map_handle_exit_request() != 0) {
            return 0;
        }
        world_map_init_subsystems();
        world_map_fade_start(0, 0x10);
        for (;;) {
            if ((g_system_flags & SYSTEM_FLAG_RUNNING) == 0) {
                break;
            }
            file_poll_load(&g_file_cd_state);
            audio_queue_process();
            /* Each display buffer has its own 0xE000-byte GS packet area. */
            buffer_index = GsGetActiveBuff();
            g_gfx_active_buffer_index = buffer_index;
            GsSetWorkBase(g_world_map_primitive_buffers + buffer_index * 0xE000);
            world_map_fade_step();
            world_map_pulse_dots();
            menu_stack_dispatch_idle();
            world_map_update_backdrop();
            if (g_system_flags & SYSTEM_FLAG_DRAW_MAP) {
                world_map_draw_frame_with_map();
            } else {
                world_map_draw_frame();
            }
            if (g_system_flags & SYSTEM_FLAG_DISPLAY_OFF) {
                SetDispMask(0);
            } else {
                SetDispMask(1);
            }
            system_frame_stub();
        }
        if (g_system_flags & SYSTEM_FLAG_RESTART) {
            continue;
        }
        world_map_finalize_loop();
        {
            s32 flags = g_system_flags;

            if (flags & SYSTEM_FLAG_RETURN_TO_TITLE) {
                return WORLD_MAP_RESULT_TITLE;
            }
            if (flags & SYSTEM_FLAG_DEEP_DUNGEON_BATTLE) {
                return WORLD_MAP_RESULT_DEEP_DUNGEON_BATTLE;
            }
            if (flags & SYSTEM_FLAG_TUTORIAL_EVENT) {
                return WORLD_MAP_RESULT_EVENT;
            }
            /* Returning through a local keeps two separate WORLD_MAP_RESULT_EVENT
             * returns; a plain return here cross-jumps into the one above. */
            {
                s32 result;

                if (flags & SYSTEM_FLAG_STORY_EVENT_REPLAY) {
                    result = WORLD_MAP_RESULT_EVENT;
                } else {
                    /* WORLD_MAP_RESULT_RANDOM_BATTLE or WORLD_MAP_RESULT_NONE */
                    result = (flags & SYSTEM_FLAG_RANDOM_BATTLE) > 0;
                }
                return result;
            }
        }
    }
}

/* When SYSTEM_FLAG_DRAW_MAP goes from set to clear, capture the map
 * backdrop again (world_map_capture_backdrop) for the frames that no longer redraw the
 * map. */
static void world_map_update_backdrop(void) {
    if ((g_system_flags & SYSTEM_FLAG_DRAW_MAP) == 0
        && (g_world_map_previous_system_flags & SYSTEM_FLAG_DRAW_MAP) != 0) {
        world_map_capture_backdrop();
    }
    g_world_map_previous_system_flags = g_system_flags;
}

/* Capture the map backdrop: draw the map tiles, routes, dots and auxiliary
 * records once, store the drawn image in g_world_map_scratch_buffer (which
 * world_map_draw_frame uploads behind each later frame), then draw
 * the menu frame over it. */
void world_map_capture_backdrop(void) {
    RECT rect;
    s32 buffer_index;

    DrawSync(0);
    VSync(0);
    GsSwapDispBuff();
    buffer_index = GsGetActiveBuff();
    g_gfx_active_buffer_index = buffer_index;
    GsSetWorkBase(g_world_map_primitive_buffers + buffer_index * 0xE000);
    GsClearOt(0, 0, &g_world_map_aux_ordering_tables[g_gfx_active_buffer_index]);
    event_transition_draw_sprite(&g_world_map_aux_ordering_tables[g_gfx_active_buffer_index]);
    /* The map passes run with the stack moved into the 1 KB scratchpad, starting
     * at its last word. */
    world_map_switch_to_stack(getScratchAddr(255));
    world_map_draw_tiles(&g_world_map_aux_ordering_tables[g_gfx_active_buffer_index]);
    world_map_draw_routes(&g_world_map_aux_ordering_tables[g_gfx_active_buffer_index]);
    world_map_project_dots();
    window_dispatch_gs_record_list(&g_world_map_aux_ordering_tables[g_gfx_active_buffer_index],
        g_window_aux_render_object_queue, g_window_aux_render_object_count);
    world_map_restore_previous_stack();
    GsSortClear(0, 0, 0, &g_world_map_aux_ordering_tables[g_gfx_active_buffer_index]);
    GsDrawOt(&g_world_map_aux_ordering_tables[g_gfx_active_buffer_index]);
    DrawSync(0);
    world_map_set_display_rect(g_gfx_active_buffer_index, &rect);
    StoreImage(&rect, g_world_map_scratch_buffer);
    DrawSync(0);
    g_picture_background_loaded_set = -1;
    GsClearOt(0, 0, &g_world_map_ordering_tables[g_gfx_active_buffer_index]);
    world_map_update_marker();
    world_map_run_world_menus(&g_world_map_ordering_tables[g_gfx_active_buffer_index]);
    hud_draw_value(&g_world_map_ordering_tables[g_gfx_active_buffer_index]);
    calendar_draw_date(&g_world_map_ordering_tables[g_gfx_active_buffer_index]);
    location_draw_label(&g_world_map_ordering_tables[g_gfx_active_buffer_index]);
    window_dispatch_gs_record_list(&g_world_map_ordering_tables[g_gfx_active_buffer_index],
        g_window_render_object_queue, g_window_render_object_count);
    GsDrawOt(&g_world_map_ordering_tables[g_gfx_active_buffer_index]);
    DrawSync(0);
}

/* Per-frame WLDCORE render pass with the 3D map: clears both ordering
 * tables, projects and draws the map tiles into the auxiliary table and the
 * map dots and menus into the main one, then sorts the auxiliary table's
 * clear, draws both tables and clears SYSTEM_FLAG_DRAW_MAP. The map and
 * HUD passes run on the scratchpad stack. Sibling of
 * world_map_draw_frame. */
static void world_map_draw_frame_with_map(void) {
    GsClearOt(0, 0, &g_world_map_aux_ordering_tables[g_gfx_active_buffer_index]);
    GsClearOt(0, 0, &g_world_map_ordering_tables[g_gfx_active_buffer_index]);
    event_transition_draw_sprite(&g_world_map_ordering_tables[g_gfx_active_buffer_index]);
    world_map_switch_to_stack(getScratchAddr(255));
    world_map_project_tiles();
    world_map_draw_tiles(&g_world_map_aux_ordering_tables[g_gfx_active_buffer_index]);
    world_map_restore_previous_stack();
    travel_step();
    world_map_switch_to_stack(getScratchAddr(255));
    world_map_draw_routes(&g_world_map_aux_ordering_tables[g_gfx_active_buffer_index]);
    world_map_draw_path_animation(&g_world_map_aux_ordering_tables[g_gfx_active_buffer_index]);
    world_map_project_dots();
    world_map_update_marker();
    world_map_restore_previous_stack();
    world_map_run_world_menus(&g_world_map_ordering_tables[g_gfx_active_buffer_index]);
    world_map_switch_to_stack(getScratchAddr(255));
    hud_draw_value(&g_world_map_ordering_tables[g_gfx_active_buffer_index]);
    calendar_draw_date(&g_world_map_ordering_tables[g_gfx_active_buffer_index]);
    location_draw_label(&g_world_map_ordering_tables[g_gfx_active_buffer_index]);
    window_dispatch_gs_record_list(&g_world_map_aux_ordering_tables[g_gfx_active_buffer_index],
        g_window_aux_render_object_queue, g_window_aux_render_object_count);
    window_dispatch_gs_record_list(&g_world_map_ordering_tables[g_gfx_active_buffer_index],
        g_window_render_object_queue, g_window_render_object_count);
    world_map_restore_previous_stack();
    DrawSync(0);
    world_map_read_pad();
    VSync(0);
    GsSwapDispBuff();
    GsSortClear(0, 0, 0, &g_world_map_aux_ordering_tables[g_gfx_active_buffer_index]);
    GsDrawOt(&g_world_map_aux_ordering_tables[g_gfx_active_buffer_index]);
    GsDrawOt(&g_world_map_ordering_tables[g_gfx_active_buffer_index]);
    g_system_flags &= ~SYSTEM_FLAG_DRAW_MAP;
}

/* Per-frame WLDCORE render pass: clears the current ordering table, runs the
 * menu frame and location-label/display-object passes, draws screen fades, waits for
 * VSync and, unless SYSTEM_FLAG_NO_BACKGROUND_IMAGE is set, uploads the
 * captured map backdrop to the new draw buffer before drawing the ordering
 * table over it. */
void world_map_draw_frame(void) {
    RECT rect;

    GsClearOt(0, 0, &g_world_map_ordering_tables[g_gfx_active_buffer_index]);
    DrawSync(0);
    event_transition_draw_sprite(&g_world_map_ordering_tables[g_gfx_active_buffer_index]);
    world_map_run_world_menus(&g_world_map_ordering_tables[g_gfx_active_buffer_index]);
    world_map_switch_to_stack(getScratchAddr(255));
    hud_draw_value(&g_world_map_ordering_tables[g_gfx_active_buffer_index]);
    calendar_draw_date(&g_world_map_ordering_tables[g_gfx_active_buffer_index]);
    location_draw_label(&g_world_map_ordering_tables[g_gfx_active_buffer_index]);
    window_dispatch_gs_record_list(&g_world_map_ordering_tables[g_gfx_active_buffer_index],
        g_window_render_object_queue, g_window_render_object_count);
    world_map_fade_draw_boxes();
    world_map_restore_previous_stack();
    DrawSync(0);
    world_map_read_pad();
    VSync(0);
    GsSwapDispBuff();
    if (!(g_system_flags & SYSTEM_FLAG_NO_BACKGROUND_IMAGE)) {
        world_map_set_display_rect(g_gfx_active_buffer_index == 0, &rect);
        LoadImage(&rect, g_world_map_scratch_buffer);
    }
    GsDrawOt(&g_world_map_ordering_tables[g_gfx_active_buffer_index]);
}

/* Initialize the WLDCORE subsystems before entering the world loop, ending
 * with the menu level the world entry opens. */
static void world_map_init_subsystems(void) {
    SetDispMask(0);
    world_map_init_state();
    world_map_init_input_repeat();
    world_map_load_texture();
    world_map_init_gfx();
    window_init_record_pools();
    world_map_init_tile_atlas();
    world_map_init_menus_and_date();
    world_map_init_dots();
    world_map_init_stub();
    world_map_init_marker_and_camera();
    menu_stack_reset();
    world_map_init_screen();
    world_map_restore_entry_level();
}

/* Clear the whole 256x480 frame buffer, then install the draw-clip rectangle
 * for the requested world-map layout: mode 0 leaves an 8-pixel top margin and
 * a 252x228 area, any other mode uses the full 256x238 screen. */
void world_map_clear_screen_and_set_clip_rect(s32 mode) {
    RECT rect;

    rect.x = 0;
    rect.y = 0;
    rect.w = 256;
    rect.h = 480;
    g_system_flags |= SYSTEM_FLAG_SCREEN_CLEARED;
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
static void world_map_init_gfx(void) {
    SetDispMask(0);
    GsInitGraph2(256, 240, 4, 0, 0);
    GsDefDispBuff(0, 0, 0, 240);
    GsInit3D();
    SetGeomOffset(0, 0);
    world_map_clear_screen_and_set_clip_rect(0);

    g_world_map_aux_ordering_tables[1].length = 2;
    g_world_map_aux_ordering_tables[0].length = 2;
    g_world_map_aux_ordering_tables[0].org = g_world_map_aux_ot_tags[0];
    g_world_map_aux_ordering_tables[1].org = g_world_map_aux_ot_tags[1];

    g_world_map_ordering_tables[1].length = 4;
    g_world_map_ordering_tables[0].length = 4;
    g_world_map_ordering_tables[0].org = g_world_map_ot_tags[0];
    g_world_map_ordering_tables[1].org = g_world_map_ot_tags[1];
}

/* Resets state used when entering the world map.
 *
 * The target derives each fade box's 32-bit attribute through the record base
 * while it walks the rectangle fields separately.
 */
static void world_map_init_state(void) {
    menu_cursor_state_t* cursor;
    GsBOXF* box;
    party_unit_t* party;
    calendar_date_t date;
    s32 day_of_year;
    s32 zodiac;
    s32 month;
    s32 day;
    s32 i;

    g_world_map_scratch_buffer = (u32*)0x801d0000;
    g_message_block_loaded = -1;
    g_picture_loaded_set = -1;
    g_menu_stack_ot_offset = 1;
    g_world_map_brightness_rgb.r = 0x80;
    g_world_map_brightness_rgb.g = 0x80;
    g_world_map_brightness_rgb.b = 0x80;
    g_world_map_previous_system_flags = 0;
    g_gfx_active_buffer_index = 0;
    g_world_map_zoom.flags = 0;
    g_random_battle_squad_id = 0;
    g_random_battle_entd_id = 0;
    g_menu_blocked_buttons = 0;
    g_system_flags = (g_system_flags
                         & (SYSTEM_FLAG_RESTART | SYSTEM_FLAG_STORY_EVENT_REPLAY | SYSTEM_FLAG_TITLE_TUTORIAL
                             | SYSTEM_FLAG_TUTORIAL_EVENT | SYSTEM_FLAG_UNLOCK_ALL | SYSTEM_FLAG_TITLE_BRAVE_STORY))
        | SYSTEM_FLAG_RUNNING;
    g_location_script_base = g_location_script_default_base;
    g_sound_novel_sound_resource = g_sound_novel_default_sound_resource;
    /* Tile descriptors share the buffer later used for picture TIMs. */
    g_picture_buffer = (u32*)g_world_map_tile_descriptors;

    for (i = 0; i < 2; i++) {
        g_world_map_fade.flags[i] = 0;
        g_world_map_fade.priorities[i] = 0xf;
        g_world_map_fade.boxes[i].attribute = 0x60000000;
        box = &g_world_map_fade.boxes[i];
        box->x = -0x80;
        box->y = -0x78;
        box->w = 0x100;
        box->h = 0xf0;
        box->r = 0;
        box->g = 0;
        box->b = 0;
    }
    g_world_map_fade.flags[0] = WORLD_MAP_FADE_OUT;

    cursor = g_menu_stack_cursor_states;
    for (i = 0; i < MENU_STACK_CURSOR_SLOT_COUNT; i++) {
        cursor->scroll_offset = 0;
        g_menu_stack_cursor_states[i].selected_index = 0;
        cursor++;
    }

    g_hud_color.r = 0x80;
    g_hud_color.g = 0x80;
    g_hud_color.b = 0x80;
    g_calendar_display_x = -0x78;
    g_calendar_display_y = -0x64;
    g_location_label_shade = 0x80;
    g_location_selected = 0;

    if (!(g_system_flags & SYSTEM_FLAG_TUTORIAL_EVENT)) {
        music_unload_scenario_and_tunes();
    }

    g_audio_queue.volume = 0x7f;
    g_hud_value_position.x = 0x30;
    g_hud_value_position.y = 0x54;
    g_hud_ot_priority = 4;
    g_audio_queue.count = 0;
    g_audio_queue.music.flags = 0;
    g_audio_queue.music.tracks[1] = 0;
    g_audio_queue.music.tracks[0] = 0;
    g_audio_queue.current_command = AUDIO_COMMAND_NONE;
    g_sound_novel_countdown_frames = 0;
    g_travel.flags = 0;

    /* Debug unlock (nothing sets SYSTEM_FLAG_UNLOCK_ALL): every profile,
     * rumor, route, location and story event; unless Start is held, every
     * treasure and unexplored land found on 1 January and every proposition
     * variable at 4; then 30,000 war funds, shop stock level 20, propositions
     * and the fur shop enabled, and a 1 January birthday for the first party
     * member. */
    if (g_system_flags & SYSTEM_FLAG_UNLOCK_ALL) {
        for (i = 0; i < EVENT_SCRIPT_VAR_PERSON_PROFILE_COUNT; i++)
            script_set_variable(EVENT_SCRIPT_VAR_PERSON_PROFILE_FIRST + i, 1);
        for (i = 0; i < EVENT_SCRIPT_VAR_RUMOR_COUNT; i++)
            script_set_variable(EVENT_SCRIPT_VAR_RUMOR_FIRST + i, 1);
        for (i = 0; i < WORLD_MAP_ROUTE_COUNT; i++)
            script_set_variable(EVENT_SCRIPT_VAR_PATH_VISIBLE_FIRST + i, 1);
        for (i = 0; i < LOCATION_COUNT; i++)
            script_set_variable(EVENT_SCRIPT_VAR_LOCATION_VISIBLE_FIRST + i, 1);
        for (i = 0; i < EVENT_SCRIPT_VAR_STORY_EVENT_COUNT; i++)
            script_set_variable(EVENT_SCRIPT_VAR_STORY_EVENT_FIRST + i, 1);

        if (!(PadRead(0) & PSX_PAD_START)) {
            date.month = 1;
            date.day = 1;
            for (i = 0; i < EVENT_SCRIPT_VAR_TREASURE_COUNT; i++) {
                script_set_variable(EVENT_SCRIPT_VAR_TREASURE_FIRST + i, 1);
                calendar_write_date(g_brave_story_treasure_acquisition_date_bits, i, &date);
            }
            for (i = 0; i < EVENT_SCRIPT_VAR_UNEXPLORED_LAND_COUNT; i++) {
                script_set_variable(EVENT_SCRIPT_VAR_UNEXPLORED_LAND_FIRST + i, 1);
                calendar_write_date(g_brave_story_land_discovery_date_bits, i, &date);
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
        day_of_year = calendar_calculate_day_of_year(month, day);
        calendar_convert_date_to_zodiac(&month, &day);
        party = party_get_unit(0);
        party->birthday_day = day_of_year;
        /* The separate s32 temp keeps the `month` reload a full 32-bit value: folding
         * the expression straight into the u8 field narrows it to an lbu. */
        zodiac = ((day_of_year & (PARTY_BIRTHDAY_DAY_HIGH_BIT_MASK << 8)) >> 8) | ((month - 1) << PARTY_ZODIAC_SHIFT);
        party->zodiac = zodiac;
    }
}

/* Finalize one WLDCORE loop pass and publish the world status. */
static void world_map_finalize_loop(void) {
    DrawSync(0);
    VSync(0);
    SetDispMask(0);
    audio_queue_wait_until_empty();
    if ((g_system_flags & SYSTEM_FLAG_TUTORIAL_EVENT) == 0) {
        music_unload_scenario_and_tunes();
    }
    script_set_variable(EVENT_SCRIPT_VAR_CURRENT_MAP, g_travel_next_map_id[0]);
}

/* Load the picture for the marker's location, capture the first map backdrop,
 * then reset the GPU, clear both ordering tables and VRAM, and swap buffers
 * with the display still off. */
static void world_map_init_screen(void) {
    s32 reserved_stack_words[2];
    s32 buffer_index;

    SetDispMask(0);
    g_world_map_marker_state.marker.picture_id = location_load_picture(g_world_map_marker_state.marker.location_id);
    file_wait_for_pending();
    buffer_index = GsGetActiveBuff();
    g_gfx_active_buffer_index = buffer_index;
    GsSetWorkBase(g_world_map_primitive_buffers + buffer_index * 0xE000);
    world_map_capture_backdrop();
    VSync(0);
    ResetGraph(1);
    GsClearOt(0, 0, &g_world_map_aux_ordering_tables[g_gfx_active_buffer_index]);
    GsClearOt(0, 0, &g_world_map_ordering_tables[g_gfx_active_buffer_index]);
    world_map_clear_vram_and_scratch(0);
    GsSwapDispBuff();
    SetDispMask(0);
}

/* Clear SYSTEM_FLAG_RESTART and open the menu level the world entry
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
 * through screen_transition_push_level. */
static void world_map_restore_entry_level(void) {
    s32 flags = g_system_flags & ~SYSTEM_FLAG_RESTART;

    g_system_flags = flags;
    if ((flags
            & (SYSTEM_FLAG_STORY_EVENT_REPLAY | SYSTEM_FLAG_TITLE_TUTORIAL | SYSTEM_FLAG_TUTORIAL_EVENT
                | SYSTEM_FLAG_TITLE_BRAVE_STORY))
        == 0) {
        if (location_script_run(g_world_map_marker_state.marker.location_id, LOCATION_SCRIPT_ACTION_TEXT) != 0) {
            world_map_suspend_cursor_level(&g_menu_stack_records[g_menu_stack_depth - 1]);
            menu_stack_push_scrollable_text_level(g_location_script_state.args[0] + TEXT_ID_WORLD_HELP_MESSAGE_BASE);
        } else {
            location_script_dispatch_requests();
        }
        audio_queue_add(AUDIO_COMMAND_PLAY_MUSIC, AUDIO_MUSIC(1, MUSIC_WORLD_MAP));
        audio_queue_add(AUDIO_COMMAND_FADE_IN, 4);
        return;
    }
    if (flags & SYSTEM_FLAG_STORY_EVENT_REPLAY) {
        g_system_flags = flags ^ SYSTEM_FLAG_STORY_EVENT_REPLAY;
        script_set_variable(EVENT_SCRIPT_VAR_WEATHER, g_brave_story_saved_weather_variable);
        screen_transition_push_level(MENU_STACK_LEVEL_BRAVE_STORY_EVENTS, 0, 0);
    } else if (flags & SYSTEM_FLAG_TUTORIAL_EVENT) {
        g_system_flags = flags ^ SYSTEM_FLAG_TUTORIAL_EVENT;
        script_set_variable(EVENT_SCRIPT_VAR_WEATHER, g_brave_story_saved_weather_variable);
        g_audio_queue.music.tracks[0] = 0x1B;
        g_audio_queue.music.tracks[1] = 0x1A;
        g_audio_queue.current_slot = 2;
        screen_transition_push_level(MENU_STACK_LEVEL_TUTORIAL_BATTLE_LIST, 1, 0);
    } else if (flags & SYSTEM_FLAG_TITLE_TUTORIAL) {
        screen_transition_push_level(MENU_STACK_LEVEL_TUTORIAL, 0, 1);
    } else if (flags & SYSTEM_FLAG_TITLE_BRAVE_STORY) {
        screen_transition_push_level(MENU_STACK_LEVEL_BRAVE_STORY, 0, 0);
    }
}

/* Called at the top of wldcore_entrypoint's loop. A nonzero
 * EVENT_SCRIPT_VAR_FORMATION_RETURN_EVENT clears SYSTEM_FLAG_RESTART,
 * restores WORLD's formation menu and makes the entrypoint return 0. */
static s32 world_map_handle_exit_request(void) {
    if (script_get_variable(EVENT_SCRIPT_VAR_FORMATION_RETURN_EVENT) != 0) {
        g_system_flags &= ~SYSTEM_FLAG_RESTART;
        formation_reopen_after_world_map();
        return 1;
    }

    return 0;
}
