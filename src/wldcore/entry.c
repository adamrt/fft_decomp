/* The world-map loop initializes its subsystems, advances the menu stack and draws each map or
 * captured-backdrop frame until control returns to MAIN. */

#include "fft/brave_story.h"
#include "fft/calendar.h"
#include "fft/display_object.h"
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
#include "fft/world_map.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/libgs.h"
#include "psx/libgte.h"
#include "psx/pad.h"
#include "psx/types.h"

extern u16* g_location_script_default_base;

/* Sound resource the sound-novel menu passes to suzuki_sound_append_resource. */
extern struct suzuki_sound_resource* g_sound_novel_default_sound_resource;
extern GsOT_TAG g_world_map_ot_tags[2][16];
extern GsOT_TAG g_world_map_aux_ot_tags[2][4];

static void map_loop_finish(void);
static void map_frame_draw(void);
static void map_graphics_init(void);
static s32 formation_return_handle(void);
static void map_screen_init(void);
static void subsystems_init(void);
static void map_state_init(void);
static void entry_level_restore(void);
static void backdrop_update(void);

/* Run world-map frames until a menu level stops the loop, then return why as a
 * world_map_result_e. A pending Formation return reopens WORLD's Formation screen
 * and returns before map initialization; a card-menu restart initializes the map
 * again instead of returning. */
world_map_result_e wldcore_entrypoint(void) {
    s32 buffer_index;

    for (;;) {
        if (formation_return_handle() != 0) {
            return 0;
        }
        subsystems_init();
        world_map_fade_start(0, 0x10);
        for (;;) {
            if ((g_world_map_flags & WORLD_MAP_FLAG_RUNNING) == 0) {
                break;
            }
            file_load_poll(&g_file_cd_state);
            music_queue_process();
            /* Each display buffer has its own 0xE000-byte GS packet area. */
            buffer_index = GsGetActiveBuff();
            g_gfx_active_buffer_index = buffer_index;
            GsSetWorkBase(g_world_map_primitive_buffers + buffer_index * 0xE000);
            world_map_fade_update();
            world_map_pulse_dots();
            menu_stack_dispatch_update();
            backdrop_update();
            if (g_world_map_flags & WORLD_MAP_FLAG_DRAW_MAP) {
                map_frame_draw();
            } else {
                world_map_frame_draw();
            }
            if (g_world_map_flags & WORLD_MAP_FLAG_DISPLAY_OFF) {
                SetDispMask(0);
            } else {
                SetDispMask(1);
            }
            system_frame_stub();
        }
        if (g_world_map_flags & WORLD_MAP_FLAG_RESTART) {
            continue;
        }
        map_loop_finish();
        {
            s32 flags = g_world_map_flags;

            if (flags & WORLD_MAP_FLAG_RETURN_TO_TITLE) {
                return WORLD_MAP_RESULT_TITLE;
            }
            if (flags & WORLD_MAP_FLAG_DEEP_DUNGEON_BATTLE) {
                return WORLD_MAP_RESULT_DEEP_DUNGEON_BATTLE;
            }
            if (flags & WORLD_MAP_FLAG_TUTORIAL_EVENT) {
                return WORLD_MAP_RESULT_EVENT;
            }
            /* Returning through a local keeps two separate WORLD_MAP_RESULT_EVENT
             * returns; a plain return here cross-jumps into the one above. */
            {
                world_map_result_e result;

                if (flags & WORLD_MAP_FLAG_STORY_EVENT_REPLAY) {
                    result = WORLD_MAP_RESULT_EVENT;
                } else {
                    /* WORLD_MAP_RESULT_RANDOM_BATTLE or WORLD_MAP_RESULT_NONE */
                    result = (flags & WORLD_MAP_FLAG_RANDOM_BATTLE) > 0;
                }
                return result;
            }
        }
    }
}

/* When WORLD_MAP_FLAG_DRAW_MAP goes from set to clear, capture the map
 * backdrop again (world_map_backdrop_capture) for the frames that no longer redraw the
 * map. */
static void backdrop_update(void) {
    if ((g_world_map_flags & WORLD_MAP_FLAG_DRAW_MAP) == 0
        && (g_world_map_previous_flags & WORLD_MAP_FLAG_DRAW_MAP) != 0) {
        world_map_backdrop_capture();
    }
    g_world_map_previous_flags = g_world_map_flags;
}

/* Capture the map backdrop: draw the map tiles, routes, dots and auxiliary
 * records once, store the drawn image in g_world_map_scratch_buffer (which
 * world_map_frame_draw uploads behind each later frame), then draw
 * the menu frame over it. */
void world_map_backdrop_capture(void) {
    RECT rect;
    s32 buffer_index;

    DrawSync(0);
    VSync(0);
    GsSwapDispBuff();
    buffer_index = GsGetActiveBuff();
    g_gfx_active_buffer_index = buffer_index;
    GsSetWorkBase(g_world_map_primitive_buffers + buffer_index * 0xE000);
    GsClearOt(0, 0, &g_world_map_aux_ordering_tables[g_gfx_active_buffer_index]);
    event_transition_sprite_draw(&g_world_map_aux_ordering_tables[g_gfx_active_buffer_index]);
    /* The map passes run with the stack moved into the 1 KB scratchpad, starting
     * at its last word. */
    world_map_switch_to_stack(getScratchAddr(255));
    world_map_tiles_draw(&g_world_map_aux_ordering_tables[g_gfx_active_buffer_index]);
    world_map_routes_draw(&g_world_map_aux_ordering_tables[g_gfx_active_buffer_index]);
    world_map_project_dots();
    display_object_gs_list_draw(&g_world_map_aux_ordering_tables[g_gfx_active_buffer_index], g_display_object_aux_queue,
        g_display_object_aux_queue_count);
    world_map_restore_previous_stack();
    GsSortClear(0, 0, 0, &g_world_map_aux_ordering_tables[g_gfx_active_buffer_index]);
    GsDrawOt(&g_world_map_aux_ordering_tables[g_gfx_active_buffer_index]);
    DrawSync(0);
    world_map_display_rect_set(g_gfx_active_buffer_index, &rect);
    StoreImage(&rect, g_world_map_scratch_buffer);
    DrawSync(0);
    g_picture_background_loaded_set = -1;
    GsClearOt(0, 0, &g_world_map_ordering_tables[g_gfx_active_buffer_index]);
    world_map_marker_update();
    world_map_menus_update(&g_world_map_ordering_tables[g_gfx_active_buffer_index]);
    hud_value_draw(&g_world_map_ordering_tables[g_gfx_active_buffer_index]);
    calendar_date_draw(&g_world_map_ordering_tables[g_gfx_active_buffer_index]);
    location_label_draw(&g_world_map_ordering_tables[g_gfx_active_buffer_index]);
    display_object_gs_list_draw(
        &g_world_map_ordering_tables[g_gfx_active_buffer_index], g_display_object_queue, g_display_object_queue_count);
    GsDrawOt(&g_world_map_ordering_tables[g_gfx_active_buffer_index]);
    DrawSync(0);
}

/* Per-frame WLDCORE render pass with the 3D map: clears both ordering
 * tables, projects and draws the map tiles into the auxiliary table and the
 * map dots and menus into the main one, then sorts the auxiliary table's
 * clear, draws both tables and clears WORLD_MAP_FLAG_DRAW_MAP. The map and
 * HUD passes run on the scratchpad stack. Sibling of
 * world_map_frame_draw. */
static void map_frame_draw(void) {
    GsClearOt(0, 0, &g_world_map_aux_ordering_tables[g_gfx_active_buffer_index]);
    GsClearOt(0, 0, &g_world_map_ordering_tables[g_gfx_active_buffer_index]);
    event_transition_sprite_draw(&g_world_map_ordering_tables[g_gfx_active_buffer_index]);
    world_map_switch_to_stack(getScratchAddr(255));
    world_map_project_tiles();
    world_map_tiles_draw(&g_world_map_aux_ordering_tables[g_gfx_active_buffer_index]);
    world_map_restore_previous_stack();
    travel_step();
    world_map_switch_to_stack(getScratchAddr(255));
    world_map_routes_draw(&g_world_map_aux_ordering_tables[g_gfx_active_buffer_index]);
    world_map_path_animation_draw(&g_world_map_aux_ordering_tables[g_gfx_active_buffer_index]);
    world_map_project_dots();
    world_map_marker_update();
    world_map_restore_previous_stack();
    world_map_menus_update(&g_world_map_ordering_tables[g_gfx_active_buffer_index]);
    world_map_switch_to_stack(getScratchAddr(255));
    hud_value_draw(&g_world_map_ordering_tables[g_gfx_active_buffer_index]);
    calendar_date_draw(&g_world_map_ordering_tables[g_gfx_active_buffer_index]);
    location_label_draw(&g_world_map_ordering_tables[g_gfx_active_buffer_index]);
    display_object_gs_list_draw(&g_world_map_aux_ordering_tables[g_gfx_active_buffer_index], g_display_object_aux_queue,
        g_display_object_aux_queue_count);
    display_object_gs_list_draw(
        &g_world_map_ordering_tables[g_gfx_active_buffer_index], g_display_object_queue, g_display_object_queue_count);
    world_map_restore_previous_stack();
    DrawSync(0);
    world_map_pad_read();
    VSync(0);
    GsSwapDispBuff();
    GsSortClear(0, 0, 0, &g_world_map_aux_ordering_tables[g_gfx_active_buffer_index]);
    GsDrawOt(&g_world_map_aux_ordering_tables[g_gfx_active_buffer_index]);
    GsDrawOt(&g_world_map_ordering_tables[g_gfx_active_buffer_index]);
    g_world_map_flags &= ~WORLD_MAP_FLAG_DRAW_MAP;
}

/* Per-frame WLDCORE render pass: clears the current ordering table, runs the
 * menu frame and location-label/display-object passes, draws screen fades, waits for
 * VSync and, unless WORLD_MAP_FLAG_NO_BACKGROUND_IMAGE is set, uploads the
 * captured map backdrop to the new draw buffer before drawing the ordering
 * table over it. */
void world_map_frame_draw(void) {
    RECT rect;

    GsClearOt(0, 0, &g_world_map_ordering_tables[g_gfx_active_buffer_index]);
    DrawSync(0);
    event_transition_sprite_draw(&g_world_map_ordering_tables[g_gfx_active_buffer_index]);
    world_map_menus_update(&g_world_map_ordering_tables[g_gfx_active_buffer_index]);
    world_map_switch_to_stack(getScratchAddr(255));
    hud_value_draw(&g_world_map_ordering_tables[g_gfx_active_buffer_index]);
    calendar_date_draw(&g_world_map_ordering_tables[g_gfx_active_buffer_index]);
    location_label_draw(&g_world_map_ordering_tables[g_gfx_active_buffer_index]);
    display_object_gs_list_draw(
        &g_world_map_ordering_tables[g_gfx_active_buffer_index], g_display_object_queue, g_display_object_queue_count);
    world_map_fade_boxes_draw();
    world_map_restore_previous_stack();
    DrawSync(0);
    world_map_pad_read();
    VSync(0);
    GsSwapDispBuff();
    if (!(g_world_map_flags & WORLD_MAP_FLAG_NO_BACKGROUND_IMAGE)) {
        world_map_display_rect_set(g_gfx_active_buffer_index == 0, &rect);
        LoadImage(&rect, g_world_map_scratch_buffer);
    }
    GsDrawOt(&g_world_map_ordering_tables[g_gfx_active_buffer_index]);
}

/* Initialize the WLDCORE subsystems before entering the world loop, ending
 * with the menu level the world entry opens. */
static void subsystems_init(void) {
    SetDispMask(0);
    map_state_init();
    world_map_input_repeat_init();
    world_map_load_texture();
    map_graphics_init();
    display_object_pools_init();
    world_map_tile_atlas_init();
    world_map_menu_state_init();
    world_map_dots_init();
    world_map_stub_init();
    world_map_marker_and_camera_init();
    menu_stack_reset();
    map_screen_init();
    entry_level_restore();
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
    g_world_map_flags |= WORLD_MAP_FLAG_SCREEN_CLEARED;
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
static void map_graphics_init(void) {
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
static void map_state_init(void) {
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
    g_world_map_previous_flags = 0;
    g_gfx_active_buffer_index = 0;
    g_world_map_zoom.flags = 0;
    g_random_battle_squad_id = 0;
    g_random_battle_entd_id = 0;
    g_menu_blocked_buttons = 0;
    g_world_map_flags
        = (g_world_map_flags
              & (WORLD_MAP_FLAG_RESTART | WORLD_MAP_FLAG_STORY_EVENT_REPLAY | WORLD_MAP_FLAG_TITLE_TUTORIAL
                  | WORLD_MAP_FLAG_TUTORIAL_EVENT | WORLD_MAP_FLAG_UNLOCK_ALL | WORLD_MAP_FLAG_TITLE_BRAVE_STORY))
        | WORLD_MAP_FLAG_RUNNING;
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
    g_calendar_display_position.x = -0x78;
    g_calendar_display_position.y = -0x64;
    g_location_label_shade = 0x80;
    g_location_selected = 0;

    if (!(g_world_map_flags & WORLD_MAP_FLAG_TUTORIAL_EVENT)) {
        music_all_unload();
    }

    g_music_queue.volume = 0x7f;
    g_hud_value_position.x = 0x30;
    g_hud_value_position.y = 0x54;
    g_hud_ot_priority = 4;
    g_music_queue.count = 0;
    g_music_queue.music.flags = 0;
    g_music_queue.music.tracks[1] = 0;
    g_music_queue.music.tracks[0] = 0;
    g_music_queue.current_command = MUSIC_QUEUE_COMMAND_NONE;
    g_sound_novel_countdown_frames = 0;
    g_travel.flags = 0;

    /* Debug unlock (nothing sets WORLD_MAP_FLAG_UNLOCK_ALL): every profile,
     * rumor, route, location and story event; unless Start is held, every
     * treasure and unexplored land found on 1 January and every proposition
     * variable at 4; then 30,000 war funds, shop stock level 20, propositions
     * and the fur shop enabled, and a 1 January birthday for the first party
     * member. */
    if (g_world_map_flags & WORLD_MAP_FLAG_UNLOCK_ALL) {
        for (i = 0; i < EVENT_SCRIPT_VAR_PERSON_PROFILE_COUNT; i++)
            script_variable_set(EVENT_SCRIPT_VAR_PERSON_PROFILE_FIRST + i, 1);
        for (i = 0; i < EVENT_SCRIPT_VAR_RUMOR_COUNT; i++)
            script_variable_set(EVENT_SCRIPT_VAR_RUMOR_FIRST + i, 1);
        for (i = 0; i < WORLD_MAP_ROUTE_COUNT; i++)
            script_variable_set(EVENT_SCRIPT_VAR_PATH_VISIBLE_FIRST + i, 1);
        for (i = 0; i < LOCATION_COUNT; i++)
            script_variable_set(EVENT_SCRIPT_VAR_LOCATION_VISIBLE_FIRST + i, 1);
        for (i = 0; i < EVENT_SCRIPT_VAR_STORY_EVENT_COUNT; i++)
            script_variable_set(EVENT_SCRIPT_VAR_STORY_EVENT_FIRST + i, 1);

        if (!(PadRead(0) & PSX_PAD_START)) {
            date.month = 1;
            date.day = 1;
            for (i = 0; i < EVENT_SCRIPT_VAR_TREASURE_COUNT; i++) {
                script_variable_set(EVENT_SCRIPT_VAR_TREASURE_FIRST + i, 1);
                calendar_date_write(g_brave_story_treasure_acquisition_date_bits, i, &date);
            }
            for (i = 0; i < EVENT_SCRIPT_VAR_UNEXPLORED_LAND_COUNT; i++) {
                script_variable_set(EVENT_SCRIPT_VAR_UNEXPLORED_LAND_FIRST + i, 1);
                calendar_date_write(g_brave_story_land_discovery_date_bits, i, &date);
            }
            for (i = 0; i < EVENT_SCRIPT_VAR_PROPOSITION_COUNT; i++)
                script_variable_set(EVENT_SCRIPT_VAR_PROPOSITION_FIRST + i, 4);
        }

        script_variable_set(EVENT_SCRIPT_VAR_WAR_FUNDS, 30000);
        script_variable_set(EVENT_SCRIPT_VAR_SHOP_ITEM_AVAILABILITY, 20);
        script_variable_set(EVENT_SCRIPT_VAR_PROPOSITIONS_ENABLED, 1);
        script_variable_set(EVENT_SCRIPT_VAR_FUR_SHOP_ENABLED, 1);

        month = 1;
        day = 1;
        script_variable_set(EVENT_SCRIPT_VAR_PLAYER_BIRTH_MONTH, 1);
        script_variable_set(EVENT_SCRIPT_VAR_PLAYER_BIRTH_DAY, day);
        day_of_year = calendar_date_to_day_index(month, day);
        calendar_date_to_zodiac_convert(&month, &day);
        party = party_unit_get(0);
        party->birthday_day = day_of_year;
        /* The separate s32 temp keeps the `month` reload a full 32-bit value: folding
         * the expression straight into the u8 field narrows it to an lbu. */
        zodiac = ((day_of_year & (PARTY_BIRTHDAY_DAY_HIGH_BIT_MASK << 8)) >> 8) | ((month - 1) << PARTY_ZODIAC_SHIFT);
        party->zodiac = zodiac;
    }
}

/* Finalize one WLDCORE loop pass and publish the world status. */
static void map_loop_finish(void) {
    DrawSync(0);
    VSync(0);
    SetDispMask(0);
    music_queue_wait_until_empty();
    if ((g_world_map_flags & WORLD_MAP_FLAG_TUTORIAL_EVENT) == 0) {
        music_all_unload();
    }
    script_variable_set(EVENT_SCRIPT_VAR_CURRENT_MAP, g_travel_next_map_id);
}

/* Load the picture for the marker's location, capture the first map backdrop,
 * then reset the GPU, clear both ordering tables and VRAM, and swap buffers
 * with the display still off. */
static void map_screen_init(void) {
    s32 reserved_stack_words[2];
    s32 buffer_index;

    SetDispMask(0);
    g_world_map_marker_state.marker.picture_id = location_picture_load(g_world_map_marker_state.marker.location_id);
    file_pending_wait();
    buffer_index = GsGetActiveBuff();
    g_gfx_active_buffer_index = buffer_index;
    GsSetWorkBase(g_world_map_primitive_buffers + buffer_index * 0xE000);
    world_map_backdrop_capture();
    VSync(0);
    ResetGraph(1);
    GsClearOt(0, 0, &g_world_map_aux_ordering_tables[g_gfx_active_buffer_index]);
    GsClearOt(0, 0, &g_world_map_ordering_tables[g_gfx_active_buffer_index]);
    world_map_image_buffers_clear(0);
    GsSwapDispBuff();
    SetDispMask(0);
}

/* Clear WORLD_MAP_FLAG_RESTART and open the menu level the world entry
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
 * through screen_transition_open. */
static void entry_level_restore(void) {
    s32 flags = g_world_map_flags & ~WORLD_MAP_FLAG_RESTART;

    g_world_map_flags = flags;
    if ((flags
            & (WORLD_MAP_FLAG_STORY_EVENT_REPLAY | WORLD_MAP_FLAG_TITLE_TUTORIAL | WORLD_MAP_FLAG_TUTORIAL_EVENT
                | WORLD_MAP_FLAG_TITLE_BRAVE_STORY))
        == 0) {
        if (location_script_run(g_world_map_marker_state.marker.location_id, LOCATION_SCRIPT_ACTION_TEXT) != 0) {
            world_map_cursor_level_suspend(&g_menu_stack_records[g_menu_stack_depth - 1]);
            menu_stack_scrollable_text_open(g_location_script_state.args[0] + TEXT_ID_WORLD_HELP_MESSAGE_BASE);
        } else {
            location_script_requests_dispatch();
        }
        music_queue_add(MUSIC_QUEUE_COMMAND_PLAY_TRACK, MUSIC_QUEUE_TRACK(1, MUSIC_WORLD_MAP));
        music_queue_add(MUSIC_QUEUE_COMMAND_FADE_IN, 4);
        return;
    }
    if (flags & WORLD_MAP_FLAG_STORY_EVENT_REPLAY) {
        g_world_map_flags = flags ^ WORLD_MAP_FLAG_STORY_EVENT_REPLAY;
        script_variable_set(EVENT_SCRIPT_VAR_WEATHER, g_brave_story_saved_weather_variable);
        screen_transition_open(MENU_STACK_LEVEL_BRAVE_STORY_EVENTS, 0, 0);
    } else if (flags & WORLD_MAP_FLAG_TUTORIAL_EVENT) {
        g_world_map_flags = flags ^ WORLD_MAP_FLAG_TUTORIAL_EVENT;
        script_variable_set(EVENT_SCRIPT_VAR_WEATHER, g_brave_story_saved_weather_variable);
        g_music_queue.music.tracks[0] = 0x1B;
        g_music_queue.music.tracks[1] = 0x1A;
        g_music_queue.current_slot = 2;
        screen_transition_open(MENU_STACK_LEVEL_TUTORIAL_BATTLE_LIST, 1, 0);
    } else if (flags & WORLD_MAP_FLAG_TITLE_TUTORIAL) {
        screen_transition_open(MENU_STACK_LEVEL_TUTORIAL, 0, 1);
    } else if (flags & WORLD_MAP_FLAG_TITLE_BRAVE_STORY) {
        screen_transition_open(MENU_STACK_LEVEL_BRAVE_STORY, 0, 0);
    }
}

/* Called at the top of wldcore_entrypoint's loop. A nonzero
 * EVENT_SCRIPT_VAR_FORMATION_RETURN_EVENT clears WORLD_MAP_FLAG_RESTART,
 * restores WORLD's formation menu and makes the entrypoint return 0. */
static s32 formation_return_handle(void) {
    if (script_variable_get(EVENT_SCRIPT_VAR_FORMATION_RETURN_EVENT) != 0) {
        g_world_map_flags &= ~WORLD_MAP_FLAG_RESTART;
        formation_world_map_return_run();
        return 1;
    }

    return 0;
}
