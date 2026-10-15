#include "fft/script.h"
#include "fft/world.h"
#include "psx/libc.h"
#include "psx/types.h"

/*
 * WORLD twin of card_save_build_buffer (CARD.OUT 0x801c01ac): serialize the
 * whole 0x1e00-byte save image into g_world_load_work_buffer, then append one
 * parity bit per 128-byte block through the primary bitstream writer.
 *
 * The descriptor block at +0x100 takes its leader from the first formation
 * unit whose sprite_set is below 4, the calendar from script variables
 * 0x2e/0x2f/0x31 and the play time from the MAIN second/minute/hour counters.
 * Everything from +0x124 on is a straight bcopy of the SCUS globals, in the
 * same order the CARD twin uses.
 */
void world_card_build_save_image(s32 slot) {
    s32 i;
    s32 j;
    s32 script_var_2f;
    s32 script_var_2e;
    s32 location;
    s32 slot_marker;
    u8* block;
    u8 parity;
    s32 unused[64]; /* unreferenced locals: the target frame is 0x130 */

    for (i = 0; i < g_world_formation_unit_count; i++) {
        if (g_world_unit_pointers[i]->sprite_set < CHARACTER_IDENTITY_RAMZA_END) {
            break;
        }
    }
    if (i == g_world_formation_unit_count) {
        i = 0;
    }

    bcopy(g_world_unit_pointers[i]->name, g_world_load_work_buffer->summary.name,
        sizeof(g_world_load_work_buffer->summary.name));
    g_world_load_work_buffer->summary.name_terminator = 0xfe;
    g_world_load_work_buffer->summary.job_id = g_world_unit_pointers[i]->identity.job_id;
    g_world_load_work_buffer->summary.level = g_world_unit_pointers[i]->status.level;

    script_var_2f = script_get_variable(EVENT_SCRIPT_VAR_DAY);
    script_var_2e = script_get_variable(EVENT_SCRIPT_VAR_MONTH);
    g_world_load_work_buffer->summary.date.day = script_var_2f;
    g_world_load_work_buffer->summary.date.month = script_var_2e;
    location = script_get_variable(EVENT_SCRIPT_VAR_LOCATION);
    slot_marker = (slot >= 0) ? 0xff : 0;
    g_world_load_work_buffer->summary.location = location;
    g_world_load_work_buffer->summary.slot = slot_marker;
    g_world_load_work_buffer->elapsed_seconds
        = g_main_system_play_time_minutes * 60 + g_main_system_play_time_seconds + g_main_system_play_time_hours * 3600;
    g_world_load_work_buffer->summary.format_version = 4;

    world_card_build_save_file_header(slot, g_world_unit_pointers[i]->status.level, &g_world_load_work_buffer->header);

    for (i = 0; i < PARTY_ROSTER_SLOT_COUNT; i++) {
        bcopy(&g_main_party_units[i], g_world_load_work_buffer->party_records[i],
            sizeof(g_world_load_work_buffer->party_records[i]));
    }

    bcopy(g_main_item_quantities, g_world_load_work_buffer->item_quantities,
        sizeof(g_world_load_work_buffer->item_quantities));
    bcopy(g_main_item_poached_quantities, g_world_load_work_buffer->poached_item_quantities,
        sizeof(g_world_load_work_buffer->poached_item_quantities));
    bcopy(g_main_item_location_flags, g_world_load_work_buffer->item_location_flags,
        sizeof(g_world_load_work_buffer->item_location_flags));
    g_world_load_work_buffer->_unknown_1c88 = D_80057b1c;
    bcopy(g_main_script_variables, g_world_load_work_buffer->script_variables,
        sizeof(g_world_load_work_buffer->script_variables));
    bcopy(&g_main_game_options, &g_world_load_work_buffer->options, sizeof(g_world_load_work_buffer->options));
    bcopy(g_main_item_type_order_tables.order_0, g_world_load_work_buffer->item_type_order_0,
        sizeof(g_world_load_work_buffer->item_type_order_0));
    bcopy(g_main_item_type_order_1, g_world_load_work_buffer->item_type_order_1,
        sizeof(g_world_load_work_buffer->item_type_order_1));
    bcopy(g_main_item_type_order_2, g_world_load_work_buffer->item_type_order_2,
        sizeof(g_world_load_work_buffer->item_type_order_2));
    bcopy(g_main_item_type_order_3, g_world_load_work_buffer->item_type_order_3,
        sizeof(g_world_load_work_buffer->item_type_order_3));
    bcopy(g_main_item_type_order_4, g_world_load_work_buffer->item_type_order_4,
        sizeof(g_world_load_work_buffer->item_type_order_4));
    bcopy(g_main_item_type_order_5, g_world_load_work_buffer->item_type_order_5,
        sizeof(g_world_load_work_buffer->item_type_order_5));
    bcopy(g_main_item_type_order_6, g_world_load_work_buffer->item_type_order_6,
        sizeof(g_world_load_work_buffer->item_type_order_6));
    bcopy(g_main_weapon_page_order, g_world_load_work_buffer->weapon_page_order,
        sizeof(g_world_load_work_buffer->weapon_page_order));
    bcopy(g_main_helmet_page_order, g_world_load_work_buffer->helmet_page_order,
        sizeof(g_world_load_work_buffer->helmet_page_order));
    bcopy(g_main_armor_page_order, g_world_load_work_buffer->armor_page_order,
        sizeof(g_world_load_work_buffer->armor_page_order));
    bcopy(g_main_accessory_page_order, g_world_load_work_buffer->accessory_page_order,
        sizeof(g_world_load_work_buffer->accessory_page_order));
    bcopy(g_main_item_page_order, g_world_load_work_buffer->item_page_order,
        sizeof(g_world_load_work_buffer->item_page_order));
    bcopy(g_main_treasure_acquisition_date_bits, g_world_load_work_buffer->treasure_acquisition_date_bits,
        sizeof(g_world_load_work_buffer->treasure_acquisition_date_bits));
    bcopy(g_main_land_discovery_date_bits, g_world_load_work_buffer->land_discovery_date_bits,
        sizeof(g_world_load_work_buffer->land_discovery_date_bits));
    bcopy(g_main_proposition_last_attempt_date_bits, g_world_load_work_buffer->proposition_last_attempt_date_bits,
        sizeof(g_world_load_work_buffer->proposition_last_attempt_date_bits));
    bcopy(g_main_proposition_states, g_world_load_work_buffer->proposition_states,
        sizeof(g_world_load_work_buffer->proposition_states));
    bcopy(
        g_main_saved_records, g_world_load_work_buffer->saved_records, sizeof(g_world_load_work_buffer->saved_records));
    bcopy(g_main_saved_data_bits, g_world_load_work_buffer->saved_data_bits,
        sizeof(g_world_load_work_buffer->saved_data_bits));
    bcopy(g_main_secondary_saved_data_bits, g_world_load_work_buffer->secondary_saved_data_bits,
        sizeof(g_world_load_work_buffer->secondary_saved_data_bits));
    bcopy(g_main_brave_story_character_ages, g_world_load_work_buffer->brave_story_character_ages,
        sizeof(g_world_load_work_buffer->brave_story_character_ages));
    g_world_load_work_buffer->proposition_count = g_main_save_proposition_count;
    bcopy(g_main_active_propositions, g_world_load_work_buffer->active_propositions,
        sizeof(g_world_load_work_buffer->active_propositions));

    world_bit_cursor_set_primary(g_world_load_work_buffer->parity_bits);
    block = (u8*)g_world_load_work_buffer;
    for (i = 0; i >= 0 && i < 0x3c; i++) {
        parity = 0;
        world_bit_cursor_set_secondary(block);
        for (j = 0; j < 0x400; j++) {
            parity += bits_read_secondary(1);
        }
        block += 0x80;
        bits_write_primary(parity & 1);
    }
}
