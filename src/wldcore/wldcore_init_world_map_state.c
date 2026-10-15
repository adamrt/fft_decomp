#include "fft/script.h"
#include "fft/wldcore.h"
#include "psx/pad.h"
#include "psx/types.h"

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
