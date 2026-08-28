#include "fft/data.h"
#include "fft/main_runtime.h"
#include "fft/main_sound.h"
#include "fft/main_unit.h"
#include "fft/script_variables.h"
#include "fft/wldcore.h"
#include "fft/world.h"
#include "psx/types.h"

/* Resets state used when entering the world map.
 *
 * The target derives each fade box's attribute word through the record base
 * while it walks the rectangle fields separately.
 */

/* Neither is declared in include/fft/wldcore.h; the signatures follow their
 * definitions in src/wldcore/. */
void wldcore_write_nine_bit_record(u32* bits, s32 record, u8* source);
s32 wldcore_month_day_to_day_of_year(s32 month, s32 day);

void wldcore_init_world_map_state(void) {
    wldcore_menu_cursor_state_t* cursor;
    party_data_t* party;
    u8 record[2];
    s32 day_of_year;
    s32 zodiac;
    s32 month;
    s32 day;
    s32 i;
    s32 offset;

    g_wldcore_scratch_buffer = (s32*)0x801d0000;
    g_wldcore_loaded_message_block = -1;
    g_wldcore_loaded_picture_set = -1;
    g_wldcore_menu_ordering_table_offset = 1;
    g_wldcore_screen_brightness_rgb[0] = 0x80;
    g_wldcore_screen_brightness_rgb[1] = 0x80;
    g_wldcore_screen_brightness_rgb[2] = 0x80;
    g_wldcore_previous_system_flags = 0;
    g_active_graphics_buffer_index = 0;
    g_wldcore_map_zoom_motion.flags = 0;
    g_wldcore_random_battle_squad_id = 0;
    g_wldcore_random_battle_entd_id = 0;
    g_wldcore_menu_result = 0;
    g_main_system_flags = (g_main_system_flags & 0x206d0000) | 1;
    /* The three pointers are republished from a table adjacent to g_wldcore_rumor_location_masks;
     * 0x80096a54/58/5c carry no catalog name, so they are reached as indices
     * off the catalogued neighbour rather than given invented spellings. */
    g_wldcore_script_base = (u16*)(&g_wldcore_rumor_location_masks)[2];
    g_wldcore_sound_novel_sound_resource = (main_sound_resource_t*)(&g_wldcore_rumor_location_masks)[3];
    g_wldcore_picture_buffer = (u32*)(&g_wldcore_rumor_location_masks)[4];

    for (i = 0; i < 2; i++) {
        g_wldcore_screen_fade_state.flags[i] = 0;
        *(s32*)&g_wldcore_screen_fade_state.priorities[i] = 0xf;
        /* A typed attribute store folds the address and loses five target
         * instructions; this indexed offset preserves the verified output.
         * The rectangle stores index through a decayed pointer: plain
         * boxes[i] also changes the address arithmetic. */
        *(u32*)((u8*)g_wldcore_screen_fade_state.boxes + (i << 4)) = 0x60000000;
        ((GsBOXF*)g_wldcore_screen_fade_state.boxes)[i].x = -0x80;
        ((GsBOXF*)g_wldcore_screen_fade_state.boxes)[i].y = -0x78;
        ((GsBOXF*)g_wldcore_screen_fade_state.boxes)[i].w = 0x100;
        ((GsBOXF*)g_wldcore_screen_fade_state.boxes)[i].h = 0xf0;
        ((GsBOXF*)g_wldcore_screen_fade_state.boxes)[i].r = 0;
        ((GsBOXF*)g_wldcore_screen_fade_state.boxes)[i].g = 0;
        ((GsBOXF*)g_wldcore_screen_fade_state.boxes)[i].b = 0;
    }
    g_wldcore_screen_fade_state.flags[0] = 2;

    cursor = g_wldcore_menu_cursor_states;
    for (offset = 0; offset < 0xa8; offset += 8) {
        cursor->scroll_offset = 0;
        ((wldcore_menu_cursor_state_t*)((u8*)g_wldcore_menu_cursor_states + offset))->selected_index = 0;
        cursor++;
    }

    g_wldcore_hud_color.r = 0x80;
    g_wldcore_hud_color.g = 0x80;
    g_wldcore_hud_color.b = 0x80;
    g_wldcore_date_display_x = -0x78;
    g_wldcore_date_display_y = -0x64;
    g_wldcore_location_marker_shade = 0x80;
    g_wldcore_proposition_selected_entry = 0;

    if (!(g_main_system_flags & 0x200000)) {
        main_sound_unload_scenario_music_and_tunes();
    }

    g_wldcore_audio_queue.volume = 0x7f;
    g_wldcore_context_value_display_position.x = 0x30;
    g_wldcore_context_value_display_position.y = 0x54;
    /* The target stores the whole word (sw), not the u16 the sorts read. */
    *(s32*)&g_wldcore_hud_ot_priority = 4;
    g_wldcore_audio_queue.count = 0;
    g_wldcore_audio_queue.music.flags = 0;
    g_wldcore_audio_queue.music.tracks[2] = 0;
    g_wldcore_audio_queue.music.tracks[1] = 0;
    g_wldcore_audio_queue.current_command = 0;
    g_wldcore_sound_novel_countdown_frames = 0;
    g_wldcore_location_entry_state.flags = 0;

    if (g_main_system_flags & 0x400000) {
        for (i = 0; i < 0x40; i++)
            world_script_set_variable(0x3c0 + i, 1);
        for (i = 0; i < 0x40; i++)
            world_script_set_variable(0x2d1 + i, 1);
        for (i = 0; i < 0x30; i++)
            world_script_set_variable(0x22c + i, 1);
        for (i = 0; i < 0x2b; i++)
            world_script_set_variable(0x200 + i, 1);
        for (i = 0; i < 0x40; i++)
            world_script_set_variable(0x1bc + i, 1);

        if (!(PadRead(0) & 0x800)) {
            record[0] = 1;
            record[1] = 1;
            for (i = 0; i < 0x2f; i++) {
                world_script_set_variable(0x321 + i, 1);
                wldcore_write_nine_bit_record(g_main_treasure_acquisition_date_bits, i, record);
            }
            for (i = 0; i < 0x10; i++) {
                world_script_set_variable(0x350 + i, 1);
                wldcore_write_nine_bit_record(g_main_land_discovery_date_bits, i, record);
            }
            for (i = 0; i < 0x60; i++)
                world_script_set_variable(0x360 + i, 4);
        }

        world_script_set_variable(EVENT_SCRIPT_VAR_WAR_FUNDS, 0x7530);
        world_script_set_variable(EVENT_SCRIPT_VAR_SHOP_ITEM_AVAILABILITY, 0x14);
        world_script_set_variable(EVENT_SCRIPT_VAR_PROPOSITIONS_ENABLED, 1);
        world_script_set_variable(EVENT_SCRIPT_VAR_FUR_SHOP_ENABLED, 1);

        month = 1;
        day = 1;
        world_script_set_variable(EVENT_SCRIPT_VAR_PLAYER_BIRTH_MONTH, 1);
        world_script_set_variable(EVENT_SCRIPT_VAR_PLAYER_BIRTH_DAY, day);
        day_of_year = wldcore_month_day_to_day_of_year(month, day);
        wldcore_convert_date_to_zodiac_date(&month, &day);
        party = main_party_get_data_pointer(0);
        party->birthday_day = day_of_year;
        /* The separate s32 temp keeps the `month` reload a full word: folding
         * the expression straight into the u8 field narrows it to an lbu. */
        zodiac = ((day_of_year & 0x100) >> 8) | ((month - 1) << 4);
        party->zodiac = zodiac;
    }
}
