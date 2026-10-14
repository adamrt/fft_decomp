#include "fft/thread.h"
#include "fft/world.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Name-entry step: on the first call (once menu thread 2 is idle) seeds the
 * entry from list, then runs the current state handler, draws the cursor and
 * key prompts each frame, and returns the entered g_world_name_entry_text name once the
 * state reaches 0xFF (NULL until then). */
u8* world_name_run_entry_keyboard_frame(const u8* list, s32 unused_1) {
    point16_t cursor;
    s32 busy;

    if (g_world_name_entry_started == 0) {
        if (thread_is_running(THREAD_SCREEN_SLOT_MESSAGE) == 0) {
            g_world_name_keyboard_last_state = -1;
            world_name_init_entry(list);
            g_world_name_entry_started = 1;
        }
        return 0;
    }
    if (g_world_name_entry_state != 0xFF) {
        busy = world_map_is_busy();
        if (busy == 0) {
            if (g_world_input_primary_repeat & PSX_PAD_L1) {
                world_name_apply_entry_key(WORLD_NAME_ENTRY_KEY_CURSOR_LEFT);
            }
            if (g_world_input_primary_repeat & PSX_PAD_R1) {
                world_name_apply_entry_key(WORLD_NAME_ENTRY_KEY_CURSOR_RIGHT);
            }
            if (g_world_input_primary_repeat & PSX_PAD_SQUARE) {
                world_name_apply_entry_key(WORLD_NAME_ENTRY_KEY_DELETE);
            }
        }
        g_world_name_entry_state_handlers[g_world_name_entry_state]();
        cursor.y = 0x3B;
        cursor.x = g_world_name_entry_cursor * 6 + 0x54;
        world_menu_set_draw_priority(0x28);
        world_menu_draw_animated_vertical_cursor(&cursor, &g_world_name_keyboard_text_cursor_anim, busy);
        menu_run_script_with_palette_mode(&g_world_name_entry_display_script, 0, busy);
        world_menu_draw_pressable_button(MENU_PRESSABLE_BUTTON_L1, 0x37, 0x50,
            g_world_input_primary_repeat & PSX_PAD_L1, busy, MAIN_SFX_PAGE_SWITCH);
        world_menu_draw_pressable_button(MENU_PRESSABLE_BUTTON_R1, 0xB1, 0x50,
            g_world_input_primary_repeat & PSX_PAD_R1, busy, MAIN_SFX_PAGE_SWITCH);
        menu_run_script_with_palette_mode(&g_world_name_entry_keyboard_script, g_world_input_primary_repeat, busy);
        if (g_world_name_entry_state != g_world_name_keyboard_last_state) {
            g_world_name_keyboard_last_state = g_world_name_entry_state;
        }
        return 0;
    }
    g_world_name_entry_started = 0;
    return g_world_name_entry_text;
}
