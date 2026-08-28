#include "fft/battle_text.h"
#include "fft/main_sound.h"
#include "fft/world.h"
#include "psx/pad.h"

/*
 * Handle confirmed name-entry keys after the initial 10-update delay.
 *
 * Row 0 selects special keys; other rows decode a character from the key
 * table. Encoded 0xfe is submitted as 0xfa. State and sound requests are
 * left for the surrounding menu handlers.
 */
void world_name_handle_entry_confirmed_keys(void) {
    u8 previous;
    s32 character;

    if (g_world_name_entry_confirm_message_shown == 0) {
        g_world_name_entry_input_delay = 0;
        world_thread_set_parameters(2, 0x21, 0xC015, 0);
        g_world_name_entry_confirm_message_shown = 1;
    }
    if (g_world_name_entry_input_delay < 10) {
        g_world_name_entry_input_delay++;
    }
    world_name_update_entry_cursor();
    g_world_name_entry_confirm_message_shown = 0;
    if ((g_world_input_primary_repeat & (PSX_PAD_CROSS | PSX_PAD_START)) && g_world_name_entry_input_delay >= 10) {
        previous = g_world_name_entry_state;
        g_world_name_entry_state = 1;
        g_world_name_entry_saved_state = previous;
    } else {
        g_world_name_entry_confirm_message_shown = 1;
    }
    if ((g_world_input_primary_repeat & PSX_PAD_CIRCLE) && g_world_name_entry_input_delay >= 10) {
        g_world_menu_sound_effect_id = 0x3C;
        if (g_world_name_entry_list_cursor == 0) {
            if (g_world_name_entry_cursor_index == 5) {
                previous = g_world_name_entry_state;
                g_world_name_entry_state = 1;
                g_world_name_entry_confirm_message_shown = 0;
                g_world_name_entry_saved_state = previous;
            } else {
                world_name_apply_entry_key(g_world_name_entry_key_codes[g_world_name_entry_cursor_index]);
            }
            g_world_menu_sound_effect_id = MAIN_SFX_CONFIRM;
            return;
        }
        character = world_text_get_encoded_char_at_position(g_world_name_entry_character_rows,
            g_world_name_entry_key_ids[g_world_name_entry_list_cursor], g_world_name_entry_cursor_index);
        if (character == TEXT_END_WAIT_FOR_CONFIRM) {
            character = TEXT_SPACE;
        }
        world_name_apply_entry_key(character);
    }
}
