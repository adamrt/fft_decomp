#include "fft/world.h"
#include "psx/types.h"

/*
 * Copies the 0xFE-terminated byte list (at most 16 entries) into
 * g_world_name_entry_text and starts menu thread 2 for it.
 *
 * Three shapes are load-bearing here. `sentinel` is one shared variable, so
 * its `li` is live across the entry test, the overflow store and the loop
 * test as in retail; the self-assignment through an asm gives `value` an
 * unbounded `nonzero_bits` so the retail `andi v0,v0,0xff` survives; and the
 * loop setup sits inside an explicit guard so `count = 0` lands in the
 * guard's delay slot and the terminator address in the loop preheader. The
 * laundered `&g_world_name_entry_text[16]` keeps `cse` from folding it into
 * the base it already built for `g_world_name_entry_text[0]`.
 */
void world_menu_open_scrollable_list(u8* list) {
    u8* terminator;
    s32 count;
    u8 value;
    /* Pin required: unpinned, `sentinel` and `count` swap v1/a1. */
    register s32 sentinel __asm__("$3");

    g_world_name_entry_original_name = list;
    g_world_name_entry_cursor_index = 0;
    g_world_name_entry_list_cursor = 1;
    world_thread_start(2, world_text_message_box_thread);
    world_thread_set_parameters(2, 0x21, 0xC015, 0);
    world_menu_init_scrollable_list_core(0, 0, 0);
    g_world_menu_cursor_position = 1;
    world_gfx_bind_data_pointer(7);
    g_world_text_section_pointers[28] = g_world_text_name_entry_keys_data;
    g_world_name_entry_cursor = 0;
    sentinel = 0xFE;
    value = list[0];
    g_world_name_entry_text[0] = value;
    __asm__("" : "=r"(value) : "0"(value));
    value = value & 0xFF;
    if (value != sentinel) {
        count = 0;
        __asm__("" : "=r"(terminator) : "0"(&g_world_name_entry_text[16]));
        while (value != sentinel) {
            if (count >= 16) {
                *terminator = sentinel;
                break;
            }
            count++;
            value = list[count];
            g_world_name_entry_text[count] = value;
        }
    }
    world_name_apply_entry_key(-4);
    g_world_menu_sound_effect_id = 0;
    g_world_name_entry_state = 0;
    world_menu_open_name_entry_key_list();
    world_script_set_vsync_mode_and_event_speed(2);
}
