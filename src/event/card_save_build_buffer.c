#include "fft/event_card.h"
#include "psx/types.h"

/*
 * CARD.OUT save-image serializer, runtime 0x801c01ac-0x801c09b4.
 *
 * The 0x1e00-byte memory-card image is filled from the SCUS globals, then a
 * per-128-byte-block parity bit is appended through the primary bitstream
 * writer.
 */
void card_save_build_buffer(s32 slot) {
    s32 i;
    s32 j;
    s32 unused_18;
    s32 day;
    s32 month;
    s32 unused_24;
    s32 unused_28;
    u8* block;
    u8 parity;
    s32 unused_34;
    s32 unused_38[0x40];
    s32 unused_138;
    party_data_t* unit;

    for (i = 0; i < PARTY_ROSTER_SLOT_COUNT; i++) {
        unit = main_party_get_data_pointer(i);
        if (unit->party_id != PARTY_ID_NONE && unit->sprite_set < 4) {
            break;
        }
    }

    bcopy(unit->name, g_card_save_buffer_pointer->name, sizeof(unit->name));
    g_card_save_buffer_pointer->name_terminator = 0xfe;
    g_card_save_buffer_pointer->job_id = unit->job_id;
    g_card_save_buffer_pointer->level = unit->level;

    day = battle_script_get_variable(EVENT_SCRIPT_VAR_DAY);
    month = battle_script_get_variable(EVENT_SCRIPT_VAR_MONTH);
    g_card_save_buffer_pointer->day = day;
    g_card_save_buffer_pointer->month = month;
    g_card_save_buffer_pointer->location = battle_script_get_variable(EVENT_SCRIPT_VAR_LOCATION);
    g_card_save_buffer_pointer->slot = 0xff;
    g_card_save_buffer_pointer->elapsed_seconds
        = g_main_system_play_time_minutes * 60 + g_main_system_play_time_seconds + g_main_system_play_time_hours * 3600;
    g_card_save_buffer_pointer->format_version = 4;

    card_build_save_file_header(slot, unit->level, g_card_save_buffer_pointer);

    for (i = 0; i < PARTY_ROSTER_SLOT_COUNT; i++) {
        bcopy(&g_main_party_data[i], g_card_save_buffer_pointer->party_records[i],
            sizeof(g_card_save_buffer_pointer->party_records[i]));
    }

    bcopy(g_main_item_quantities, g_card_save_buffer_pointer->item_quantities,
        sizeof(g_card_save_buffer_pointer->item_quantities));
    bcopy(g_main_item_poached_quantities, g_card_save_buffer_pointer->poached_item_quantities,
        sizeof(g_card_save_buffer_pointer->poached_item_quantities));
    bcopy(g_main_item_location_flags, g_card_save_buffer_pointer->item_location_flags,
        sizeof(g_card_save_buffer_pointer->item_location_flags));
    g_card_save_buffer_pointer->_unknown_1c88 = D_80057b1c;
    bcopy(g_main_script_variables, g_card_save_buffer_pointer->script_variables, sizeof(g_main_script_variables));
    bcopy(&g_main_game_options, g_card_save_buffer_pointer->game_options, 4);
    bcopy(g_main_item_type_order_tables.order_0, g_card_save_buffer_pointer->item_type_order_0, 0xc);
    bcopy(g_main_item_type_order_tables.order_1, g_card_save_buffer_pointer->item_type_order_1, 8);
    bcopy(g_main_item_type_order_tables.order_2, g_card_save_buffer_pointer->item_type_order_2, 7);
    bcopy(g_main_item_type_order_tables.order_3, g_card_save_buffer_pointer->item_type_order_3, 5);
    bcopy(g_main_item_type_order_tables.order_4, g_card_save_buffer_pointer->item_type_order_4, 5);
    bcopy(g_main_item_type_order_tables.order_5, g_card_save_buffer_pointer->item_type_order_5, 7);
    bcopy(g_main_item_type_order_tables.order_6, g_card_save_buffer_pointer->item_type_order_6, 5);
    bcopy(g_main_weapon_page_order, g_card_save_buffer_pointer->weapon_page_order, 0x8a);
    bcopy(g_main_helmet_page_order, g_card_save_buffer_pointer->helmet_page_order, 0x1d);
    bcopy(g_main_armor_page_order, g_card_save_buffer_pointer->armor_page_order, 0x25);
    bcopy(g_main_accessory_page_order, g_card_save_buffer_pointer->accessory_page_order, 0x21);
    bcopy(g_main_item_page_order, g_card_save_buffer_pointer->item_page_order, 0x15);
    bcopy(g_main_treasure_acquisition_date_bits, g_card_save_buffer_pointer->treasure_acquisition_date_bits, 0x35);
    bcopy(g_main_land_discovery_date_bits, g_card_save_buffer_pointer->land_discovery_date_bits, 0x12);
    bcopy(g_main_proposition_last_attempt_date_bits, g_card_save_buffer_pointer->proposition_last_attempt_date_bits,
        0x6c);
    bcopy(g_main_proposition_states, g_card_save_buffer_pointer->proposition_states,
        sizeof(g_card_save_buffer_pointer->proposition_states));
    bcopy(g_main_saved_records, g_card_save_buffer_pointer->saved_records, 0x118);
    bcopy(g_main_saved_data_bits, g_card_save_buffer_pointer->saved_data_bits, sizeof(g_main_saved_data_bits));
    bcopy(g_main_secondary_saved_data_bits, g_card_save_buffer_pointer->secondary_saved_data_bits,
        sizeof(g_card_save_buffer_pointer->secondary_saved_data_bits));
    bcopy(g_main_brave_story_character_ages, g_card_save_buffer_pointer->brave_story_character_ages,
        sizeof(g_main_brave_story_character_ages));
    g_card_save_buffer_pointer->proposition_count = g_main_save_proposition_count;
    bcopy(g_main_active_propositions, g_card_save_buffer_pointer->active_propositions,
        sizeof(g_card_save_buffer_pointer->active_propositions));

    card_bits_init_primary_reader(g_card_save_buffer_pointer->parity_bits);
    block = (u8*)g_card_save_buffer_pointer;
    for (i = 0; (u32)i < CARD_SAVE_PARITY_BLOCK_COUNT; i++) {
        parity = 0;
        card_bits_init_secondary_reader(block);
        for (j = 0; j < CARD_SAVE_PARITY_BITS_PER_BLOCK; j++) {
            parity += card_bits_read_secondary(1);
        }
        block += CARD_SAVE_PARITY_BLOCK_BYTES;
        card_bits_write_primary(parity & 1);
    }
}
