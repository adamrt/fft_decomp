#include "fft/world.h"
#include "psx/libc.h"
#include "psx/types.h"

/*
 * Exact inverse of world_card_build_save_image: restore the SCUS globals from the
 * 0x1e00-byte save image in g_world_load_work_buffer.
 *
 * Returns 0 when the image is unusable -- the slot byte still reads 0xff
 * (never written), the format version is not 4, or a 128-byte block's parity
 * disagrees with the bit stored for it.  Blocks 0..2 are the descriptor and
 * are not covered, so the primary cursor skips their three bits.
 *
 * A non-zero partial_load is the tutorial's partial load: it skips both the parity
 * sweep and the play-time restore, keeping the clock the caller already has.
 *
 * Constructs the match depends on:
 *   - `j`, the parity-sweep counter, doubles as the item-order "found" flag.
 *     A separate flag local swaps the $s1/$s2 roles of the flag and `i`.
 *   - `i = 0;` written above `j = 0;` before the suzuki_sound_set_type call,
 *     with the loop spelled `for (; i < 11; i++)`. That run is one basic
 *     block, so the list scheduler orders the two zeroing moves by source
 *     order, and the flag is live across the call in a callee-saved register.
 */
s32 world_card_load_globals_from_save_image(s32 partial_load) {
    s32 i;
    s32 j;
    u8 parity;
    u8* block;

    if (g_world_load_work_buffer->summary.slot == 0xff) {
        return 0;
    }
    if (g_world_load_work_buffer->summary.format_version != 4) {
        return 0;
    }

    if (partial_load == 0) {
        world_bit_cursor_set_primary(g_world_load_work_buffer->parity_bits);
        /* The sweep reads the image as raw 128-byte blocks; block 3 starts mid-field. */
        block = (u8*)g_world_load_work_buffer + 3 * 0x80;
        bits_read_primary(3);
        for (i = 3; i >= 0 && i < 0x3c; i++) {
            parity = 0;
            world_bit_cursor_set_secondary(block);
            for (j = 0; j < 0x400; j++) {
                parity += bits_read_secondary(1);
            }
            block += 0x80;
            parity &= 1;
            if (parity != (u8)bits_read_primary(1)) {
                return 0;
            }
        }
    }

    for (i = 0; i < PARTY_ROSTER_SLOT_COUNT; i++) {
        bcopy(g_world_load_work_buffer->party_records[i], &g_main_party_units[i],
            sizeof(g_world_load_work_buffer->party_records[i]));
    }

    bcopy(g_world_load_work_buffer->item_quantities, g_main_item_quantities,
        sizeof(g_world_load_work_buffer->item_quantities));
    bcopy(g_world_load_work_buffer->poached_item_quantities, g_main_item_poached_quantities,
        sizeof(g_world_load_work_buffer->poached_item_quantities));
    bcopy(g_world_load_work_buffer->item_location_flags, g_main_item_location_flags,
        sizeof(g_world_load_work_buffer->item_location_flags));
    D_80057b1c = g_world_load_work_buffer->_unknown_1c88;
    bcopy(g_world_load_work_buffer->script_variables, g_main_script_variables,
        sizeof(g_world_load_work_buffer->script_variables));
    bcopy(&g_world_load_work_buffer->options, &g_main_game_options, sizeof(g_world_load_work_buffer->options));
    i = 0;
    j = 0;
    suzuki_sound_set_type(g_main_game_options.fields.sound_mode);

    for (; i < 11; i++) {
        if (g_world_load_work_buffer->item_type_order_0[i] == 11) {
            j = 1;
        }
    }
    if (!j) {
        bcopy(g_world_load_work_buffer->item_type_order_0, g_main_item_type_order_tables.order_0,
            sizeof(g_world_load_work_buffer->item_type_order_0));
    }

    j = 0;
    g_main_item_type_order_tables.order_0[11] = -1;
    for (i = 0; i < 7; i++) {
        if (g_world_load_work_buffer->item_type_order_1[i] == 11) {
            j = 1;
        }
    }
    if (!j) {
        bcopy(g_world_load_work_buffer->item_type_order_1, g_main_item_type_order_1,
            sizeof(g_world_load_work_buffer->item_type_order_1));
    }

    g_main_item_type_order_1[7] = -1;
    bcopy(g_world_load_work_buffer->item_type_order_2, g_main_item_type_order_2,
        sizeof(g_world_load_work_buffer->item_type_order_2));
    bcopy(g_world_load_work_buffer->item_type_order_3, g_main_item_type_order_3,
        sizeof(g_world_load_work_buffer->item_type_order_3));
    bcopy(g_world_load_work_buffer->item_type_order_4, g_main_item_type_order_4,
        sizeof(g_world_load_work_buffer->item_type_order_4));
    bcopy(g_world_load_work_buffer->item_type_order_5, g_main_item_type_order_5,
        sizeof(g_world_load_work_buffer->item_type_order_5));
    bcopy(g_world_load_work_buffer->item_type_order_6, g_main_item_type_order_6,
        sizeof(g_world_load_work_buffer->item_type_order_6));
    bcopy(g_world_load_work_buffer->weapon_page_order, g_main_weapon_page_order,
        sizeof(g_world_load_work_buffer->weapon_page_order));
    bcopy(g_world_load_work_buffer->helmet_page_order, g_main_helmet_page_order,
        sizeof(g_world_load_work_buffer->helmet_page_order));
    bcopy(g_world_load_work_buffer->armor_page_order, g_main_armor_page_order,
        sizeof(g_world_load_work_buffer->armor_page_order));
    bcopy(g_world_load_work_buffer->accessory_page_order, g_main_accessory_page_order,
        sizeof(g_world_load_work_buffer->accessory_page_order));
    bcopy(g_world_load_work_buffer->item_page_order, g_main_item_page_order,
        sizeof(g_world_load_work_buffer->item_page_order));
    bcopy(g_world_load_work_buffer->treasure_acquisition_date_bits, g_main_treasure_acquisition_date_bits,
        sizeof(g_world_load_work_buffer->treasure_acquisition_date_bits));
    bcopy(g_world_load_work_buffer->land_discovery_date_bits, g_main_land_discovery_date_bits,
        sizeof(g_world_load_work_buffer->land_discovery_date_bits));
    bcopy(g_world_load_work_buffer->proposition_last_attempt_date_bits, g_main_proposition_last_attempt_date_bits,
        sizeof(g_world_load_work_buffer->proposition_last_attempt_date_bits));
    bcopy(g_world_load_work_buffer->proposition_states, g_main_proposition_states,
        sizeof(g_world_load_work_buffer->proposition_states));
    bcopy(
        g_world_load_work_buffer->saved_records, g_main_saved_records, sizeof(g_world_load_work_buffer->saved_records));
    bcopy(g_world_load_work_buffer->saved_data_bits, g_main_saved_data_bits,
        sizeof(g_world_load_work_buffer->saved_data_bits));
    bcopy(g_world_load_work_buffer->secondary_saved_data_bits, g_main_secondary_saved_data_bits,
        sizeof(g_world_load_work_buffer->secondary_saved_data_bits));
    bcopy(g_world_load_work_buffer->brave_story_character_ages, g_main_brave_story_character_ages,
        sizeof(g_world_load_work_buffer->brave_story_character_ages));
    g_main_save_proposition_count = g_world_load_work_buffer->proposition_count;
    bcopy(g_world_load_work_buffer->active_propositions, g_main_active_propositions,
        sizeof(g_world_load_work_buffer->active_propositions));

    if (partial_load == 0) {
        i = g_world_load_work_buffer->elapsed_seconds;
        g_main_system_play_time_hours = i / 3600;
        i %= 3600;
        g_main_system_play_time_minutes = i / 60;
        i %= 60;
        g_main_system_play_time_seconds = i;
    }
    return 1;
}
