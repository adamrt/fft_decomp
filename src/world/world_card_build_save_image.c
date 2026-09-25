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
        if (g_world_formation_unit_pointers[i]->sprite_set < 4) {
            break;
        }
    }
    if (i == g_world_formation_unit_count) {
        i = 0;
    }

    bcopy(g_world_formation_unit_pointers[i]->name, g_world_load_work_buffer->name, 0x10);
    g_world_load_work_buffer->name_terminator = 0xfe;
    g_world_load_work_buffer->job_id = g_world_formation_unit_pointers[i]->job_id;
    g_world_load_work_buffer->level = g_world_formation_unit_pointers[i]->level;

    script_var_2f = world_script_get_variable(EVENT_SCRIPT_VAR_DAY);
    script_var_2e = world_script_get_variable(EVENT_SCRIPT_VAR_MONTH);
    g_world_load_work_buffer->day = script_var_2f;
    g_world_load_work_buffer->month = script_var_2e;
    location = world_script_get_variable(EVENT_SCRIPT_VAR_LOCATION);
    slot_marker = (slot >= 0) ? 0xff : 0;
    g_world_load_work_buffer->location = location;
    g_world_load_work_buffer->slot = slot_marker;
    g_world_load_work_buffer->elapsed_seconds
        = g_main_system_play_time_minutes * 60 + g_main_system_play_time_seconds + g_main_system_play_time_hours * 3600;
    g_world_load_work_buffer->format_version = 4;

    world_card_build_save_file_header(
        slot, g_world_formation_unit_pointers[i]->level, (card_save_header_t*)g_world_load_work_buffer);

    for (i = 0; i < PARTY_ROSTER_SLOT_COUNT; i++) {
        bcopy(&g_main_party_data[i], g_world_load_work_buffer->party_records[i], 0xe0);
    }

    bcopy(g_main_item_quantities, g_world_load_work_buffer->item_quantities, 0x100);
    bcopy(g_main_item_poached_quantities, g_world_load_work_buffer->poached_item_quantities, 0x100);
    bcopy(g_main_item_location_flags, g_world_load_work_buffer->item_location_flags, 0x80);
    g_world_load_work_buffer->field_1c88 = D_80057b1c;
    bcopy(g_main_script_variables, g_world_load_work_buffer->script_variables, 0x400);
    bcopy(&g_main_game_options, &g_world_load_work_buffer->options, 4);
    bcopy(g_main_item_type_order_tables.order_0, g_world_load_work_buffer->item_type_order_0, 0xc);
    bcopy(g_main_item_type_order_1, g_world_load_work_buffer->item_type_order_1, 8);
    bcopy(g_main_item_type_order_2, g_world_load_work_buffer->item_type_order_2, 7);
    bcopy(g_main_item_type_order_3, g_world_load_work_buffer->item_type_order_3, 5);
    bcopy(g_main_item_type_order_4, g_world_load_work_buffer->item_type_order_4, 5);
    bcopy(g_main_item_type_order_5, g_world_load_work_buffer->item_type_order_5, 7);
    bcopy(g_main_item_type_order_6, g_world_load_work_buffer->item_type_order_6, 5);
    bcopy(g_main_weapon_page_order, g_world_load_work_buffer->weapon_page_order, 0x8a);
    bcopy(g_main_helmet_page_order, g_world_load_work_buffer->helmet_page_order, 0x1d);
    bcopy(g_main_armor_page_order, g_world_load_work_buffer->armor_page_order, 0x25);
    bcopy(g_main_accessory_page_order, g_world_load_work_buffer->accessory_page_order, 0x21);
    bcopy(g_main_item_page_order, g_world_load_work_buffer->item_page_order, 0x15);
    bcopy(g_main_treasure_acquisition_date_bits, g_world_load_work_buffer->treasure_acquisition_date_bits, 0x35);
    bcopy(g_main_land_discovery_date_bits, g_world_load_work_buffer->land_discovery_date_bits, 0x12);
    bcopy(
        g_main_proposition_last_attempt_date_bits, g_world_load_work_buffer->proposition_last_attempt_date_bits, 0x6c);
    bcopy(g_main_proposition_states, g_world_load_work_buffer->proposition_states, 0x60);
    bcopy(g_main_saved_records, g_world_load_work_buffer->saved_records, 0x118);
    bcopy(g_main_saved_data_bits, g_world_load_work_buffer->saved_data_bits, 0xa0);
    bcopy(g_main_secondary_saved_data_bits, g_world_load_work_buffer->secondary_saved_data_bits, 8);
    bcopy(g_main_brave_story_character_ages, g_world_load_work_buffer->brave_story_character_ages, 0x40);
    g_world_load_work_buffer->proposition_count = g_main_save_proposition_count;
    bcopy(g_main_active_propositions, g_world_load_work_buffer->active_propositions, 0x48);

    world_bit_cursor_set_primary(g_world_load_work_buffer->parity_bits);
    block = (u8*)g_world_load_work_buffer;
    for (i = 0; (u32)i < 0x3c; i++) {
        parity = 0;
        world_bit_cursor_set_secondary(block);
        for (j = 0; j < 0x400; j++) {
            parity += world_bit_cursor_read_secondary(1);
        }
        block += 0x80;
        world_bit_cursor_write_primary(parity & 1);
    }
}
