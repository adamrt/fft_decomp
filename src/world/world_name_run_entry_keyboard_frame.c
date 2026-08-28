#include "fft/thread.h"
#include "fft/world.h"
#include "psx/pad.h"
#include "psx/types.h"

typedef void (*world_name_entry_handler_t)(void);

extern world_name_entry_handler_t g_world_name_entry_state_handlers[];

/* Name-entry step: on the first call (once menu thread 2 is idle) seeds the
 * entry from list, then runs the current state handler, draws the cursor and
 * key prompts each frame, and returns the entered g_world_name_entry_text name once the
 * state reaches 0xFF (NULL until then). */
u8* world_name_run_entry_keyboard_frame(u8* list) {
    world_menu_point_t cursor;
    s32 busy;

    if (g_world_name_entry_started == 0) {
        if (world_thread_is_running(2) == 0) {
            g_world_name_keyboard_last_state = -1;
            world_menu_open_scrollable_list(list);
            g_world_name_entry_started = 1;
        }
        return 0;
    }
    if (g_world_name_entry_state != 0xFF) {
        busy = world_map_is_busy();
        if (busy == 0) {
            if (g_world_input_primary_repeat & PSX_PAD_L1) {
                world_name_apply_entry_key(-4);
            }
            if (g_world_input_primary_repeat & PSX_PAD_R1) {
                world_name_apply_entry_key(-5);
            }
            if (g_world_input_primary_repeat & PSX_PAD_SQUARE) {
                world_name_apply_entry_key(-2);
            }
        }
        g_world_name_entry_state_handlers[g_world_name_entry_state]();
        cursor.y = 0x3B;
        cursor.x = g_world_name_entry_cursor * 6 + 0x54;
        world_menu_set_draw_priority(0x28);
        world_menu_draw_animated_vertical_cursor(&cursor, &g_world_name_keyboard_text_cursor_anim, busy);
        world_menu_run_script_with_palette_mode(g_world_name_entry_display_script, 0, busy);
        /* The definition's s16/u16 parameter conversions would change this call's codegen. */
        ((void (*)(s32, s32, s32, s32, s16, s32))world_menu_draw_pressable_button)(
            0, 0x37, 0x50, (u16)g_world_input_primary_repeat & PSX_PAD_L1, busy, 6);
        /* The definition's s16/u16 parameter conversions would change this call's codegen. */
        ((void (*)(s32, s32, s32, s32, s16, s32))world_menu_draw_pressable_button)(
            1, 0xB1, 0x50, (u16)g_world_input_primary_repeat & PSX_PAD_R1, busy, 6);
        world_menu_run_script_with_palette_mode(g_world_name_entry_keyboard_script, g_world_input_primary_repeat, busy);
        if (g_world_name_entry_state != g_world_name_keyboard_last_state) {
            g_world_name_keyboard_last_state = g_world_name_entry_state;
        }
        return 0;
    }
    g_world_name_entry_started = 0;
    return g_world_name_entry_text;
}
