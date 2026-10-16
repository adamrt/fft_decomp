/* The formation icon list thread, unit name strings, the display mode and the ordering table pointer. */

#include "fft/main_unit.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "fft/world_formation.h"
#include "fft/world_menu.h"
#include "psx/libgpu.h"
#include "psx/types.h"

void world_menu_formation_icon_list_thread(void) {
    RECT rect;
    menu_icon_record_t records[2];
    s32 cursor;
    menu_entry_t* param;
    u8* buffer;
    s32 i;
    menu_icon_sprites_t* record;

    param = (menu_entry_t*)thread_get_current_parameter_1();
    record = &records[0].base;
    cursor = param->selected_index;
    if (cursor == -1) {
        cursor = 0;
    }
    menu_build_window_sprites(&rect, param, &records[0]);
    SetShadeTex(&record->sprites[0], 0);
    SetShadeTex(&record->sprites[1], 0);
    SetShadeTex(&record->sprites[2], 0);
    SetShadeTex(&record->sprites[3], 0);
    mem_copy_bytes_nonempty(&records[1], record, sizeof(records[1]));
    buffer = menu_build_and_upload_window_frame_image(
        param->inner_width, param->inner_height, &rect, MENU_WINDOW_FRAME_BUILD);
    g_menu_text_state.pen.stride = param->inner_width;
    menu_set_text_origin(8, 9);
    g_menu_text_palette_offset = 0;
    menu_display_text_entry(param->text_id, buffer, &g_menu_text_state.pen);
    LoadImage(&rect, buffer);
    for (i = 0;; i++) {
        thread_yield();
        if (i == 0) {
            menu_free_memory(buffer);
        }
        record = &records[i & 1].base;
        param->selected_index = cursor;
        menu_select_icon_cluts(record);
        menu_update_icon_cursor_sprites(param, record, i, cursor);
        if (menu_should_close_thread(&g_world_formation_menu_input_state.new_buttons) != 0) {
            break;
        }
        if (thread_is_previous_running() == 0) {
            menu_step_wrapping_cursor_on_scroll_buttons(param, &cursor);
            menu_handle_action(param, cursor);
            menu_handle_cancel_input(param);
        }
        setRGB0(&record->sprites[0], g_world_menu_color_red, g_world_menu_color_green, g_world_menu_color_blue);
        setRGB0(&record->sprites[1], g_world_menu_color_red, g_world_menu_color_green, g_world_menu_color_blue);
        setRGB0(&record->sprites[2], g_world_menu_color_red, g_world_menu_color_green, g_world_menu_color_blue);
        setRGB0(&record->sprites[3], g_world_menu_color_red, g_world_menu_color_green, g_world_menu_color_blue);
        menu_submit_icon_primitives(record);
    }
    thread_yield();
    thread_exit_current();
}

/* Rebuild the roster name text section from the party data: one 0xFE
 * terminated name per roster slot, empty for a vacant slot, with a terminator
 * appended to a name that fills all 16 bytes. Bumps the name string
 * revision. */
void world_text_generate_formation_unit_name_string(void) {
    u8* out;
    party_unit_t* unit;
    s32 i;
    s32 j;
    u8 character;

    out = g_text_section_pointers[TEXT_SECTION_ROSTER_UNIT_NAMES];
    for (i = 0; i < PARTY_ROSTER_SLOT_COUNT; i++) {
        unit = main_party_get_unit(i);
        if (unit->party_id == PARTY_ID_NONE) {
            *out = 0xFE;
            out++;
        } else {
            for (j = 0; j < 0x10; j++) {
                character = unit->name[j];
                *out = character;
                if ((character & 0xFE) == 0xFE) {
                    out++;
                    break;
                }
                out++;
                if (j == 0xF) {
                    *out = 0xFE;
                    out++;
                }
            }
        }
    }
    g_world_formation_name_string_revision = g_world_formation_name_string_revision + 1;
}

void world_formation_set_display_mode(s32 mode) {
    g_menu_formation_display_mode = mode;
}

void world_formation_set_ordering_table_pointer(u32* ordering_table) {
    g_world_formation_ordering_table = ordering_table;
}
