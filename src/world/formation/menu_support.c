/* The formation icon list thread, unit name strings, the display mode and the ordering table pointer. */

#include "fft/formation.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/menu_cursor.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/menu_window.h"
#include "fft/party.h"
#include "fft/screen.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "fft/unit_party.h"
#include "psx/libgpu.h"
#include "psx/types.h"

static s32 g_roster_names_revision = 0;

/* WORLD's simple menu window thread: like menu_icon_list_thread, but it reads the formation
 * screen's input, ignores input while the previous thread runs and tints the window with the world
 * menu color. */
void screen_icon_list_thread(void) {
    RECT rect;
    menu_icon_record_t records[2];
    s32 cursor;
    menu_entry_t* param;
    u8* buffer;
    s32 i;
    menu_icon_sprites_t* record;

    param = (menu_entry_t*)thread_current_parameter_1_get();
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
    mem_copy_bytes_nonempty(&records[1], &records[0], sizeof(records[1]));
    buffer = menu_build_and_upload_window_frame_image(
        param->inner_width, param->inner_height, &rect, MENU_WINDOW_FRAME_BUILD);
    g_menu_window_text_pen.stride = param->inner_width;
    menu_set_text_origin(8, 9);
    g_menu_text_palette_offset = 0;
    menu_display_text_entry(param->text_id, buffer, &g_menu_window_text_pen);
    LoadImage(&rect, buffer);

    /* The sprite records alternate by frame, since the GPU may still be
     * reading last frame's; the frame image is freed once its upload has had a
     * frame to finish. */
    for (i = 0;; i++) {
        thread_yield();
        if (i == 0) {
            menu_free_memory(buffer);
        }
        record = &records[i & 1].base;
        param->selected_index = cursor;
        menu_select_icon_cluts(record);
        menu_update_icon_cursor_sprites(param, record, i, cursor);
        if (menu_should_close_thread(&g_screen_input_state.new_buttons) != 0) {
            break;
        }
        if (thread_is_previous_running() == 0) {
            menu_row_cursor_update(param, &cursor);
            menu_handle_action(param, cursor);
            menu_handle_cancel_input(param);
        }
        setRGB0(&record->sprites[0], g_screen_color_red, g_screen_color_green, g_screen_color_blue);
        setRGB0(&record->sprites[1], g_screen_color_red, g_screen_color_green, g_screen_color_blue);
        setRGB0(&record->sprites[2], g_screen_color_red, g_screen_color_green, g_screen_color_blue);
        setRGB0(&record->sprites[3], g_screen_color_red, g_screen_color_green, g_screen_color_blue);
        menu_submit_icon_primitives(record);
    }
    thread_yield();
    thread_current_exit();
}

/* Rebuild the roster name text section from the party data: one 0xFE
 * terminated name per roster slot, empty for a vacant slot, with a terminator
 * appended to a name that fills all 16 bytes. A 0xFF byte also ends a name
 * and is copied as its terminator. Bumps the name string revision. */
void formation_roster_names_build(void) {
    u8* out = g_text_section_pointers[TEXT_SECTION_ROSTER_UNIT_NAMES];
    party_unit_t* unit;
    s32 i;
    s32 j;
    u8 character;

    for (i = 0; i < PARTY_ROSTER_SLOT_COUNT; i++) {
        unit = party_unit_get(i);
        if (unit->party_id == PARTY_ID_NONE) {
            *out = 0xFE;
            out++;
        } else {
            for (j = 0; j < UNIT_NAME_SIZE; j++) {
                character = unit->name[j];
                *out = character;
                if ((character & 0xFE) == 0xFE) {
                    out++;
                    break;
                }
                out++;
                if (j == UNIT_NAME_SIZE - 1) {
                    *out = 0xFE;
                    out++;
                }
            }
        }
    }
    g_roster_names_revision++;
}

/* Set whether the menu cursors hold their bob: at 1 the cursor threads reset their bob state to 2 with the
 * timer cleared every frame; the title script switches it between 0 and 1. */
void menu_cursor_bob_hold_set(s32 mode) {
    g_menu_cursor_bob_hold = mode;
}

/* Set the ordering table the formation view zoom thread draws into. */
void formation_backdrop_otag_set(u32* ordering_table) {
    g_formation_ordering_table = ordering_table;
}
