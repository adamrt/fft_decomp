#include "fft/main_runtime.h"
#include "fft/thread.h"
#include "fft/wldcore.h"
#include "fft/world.h"

void wldcore_read_nine_bit_record(u32* bits, s32 record, u8* dest);

/* Pushes the proposition-attempt detail text window as menu level 0x2a, the
 * partner of the 0x80088180 pop. Records the entry on the new level, decodes
 * that entry's last-attempt date out of the packed nine-bit array at
 * g_main_proposition_last_attempt_date_bits (dest[0] month 1-12, dest[1] day
 * 1-31), and sets up the shared scrollable text window at g_wldcore_scrollable_text_window for text
 * 0x8800 + entry with the month name (0xb84f + month) and the day as its two
 * text substitutions. Then hides the parent window-render level's window and
 * render record with flag 0x10 and starts WORLD thread 14 on text 0xb849.
 *
 * Addressing follows wldcore_menu_push_tutorial_topic_text_level: the target holds one base of
 * g_wldcore_scrollable_text_window+0x34 (its text_id) and reaches the earlier fields at negative
 * displacements, so the original held a pointer biased to that field.
 * image_y is the one field the target still stores absolutely, so it is
 * written as g_wldcore_scrollable_text_window.image_y.
 *
 * Both decoded date bytes are read into locals before the substitution stores.
 * alias.c cannot prove the stack reads do not alias the stores through window,
 * so spelling date[1] inline kept its lbu below sw v0,0x34(v1), which left the
 * first lbu's load-delay shadow with no filler but li a1,0xb84f. Reading month
 * and day first lets the two lbu pack adjacent, carries day in a2 into the jal
 * delay slot, and leaves the constant hoisted above sw v0,0x30(v1), which is
 * exactly where the target materialises it. */
void wldcore_menu_push_proposition_attempt_detail_level(s32 value) {
    wldcore_text_scrollable_window_t* window;
    s32* anchor;
    s32 month;
    s32 day;
    u8 unused[8];
    u8 date[2];

    wldcore_wait_for_file_load();
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].window_index = value;
    wldcore_read_nine_bit_record(g_main_proposition_last_attempt_date_bits, value, date);

    /* Opaque to cse, or it rewrites the displacements as absolute $at stores. */
    anchor = &g_wldcore_scrollable_text_window.text_id;
    __asm__("" : "=r"(anchor) : "0"(anchor));
    window = (wldcore_text_scrollable_window_t*)(anchor - 13);

    window->text_id = value + 0x8800;
    window->image_x = 0;
    g_wldcore_scrollable_text_window.image_y = 0x70;
    window->base.x = -122;
    window->base.y = -40;
    window->text_width = 0xdc;
    window->rows_per_page = 8;
    window->priority = 8;
    window->image_coordinate_mode = 1;
    window->extra_render_index = -1;
    month = date[0];
    day = date[1];
    window->text_substitutions[0] = month + 0xb84f;
    window->text_substitutions[1] = day;
    wldcore_text_init_scrollable_window(window);
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                 .window_render.window_index]
        .flags |= 0x10;
    g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                        .window_render.render_index]
        .flags |= 0x10;
    g_world_text_substitution_values[0] = value + 0x6000;
    world_thread_set_parameters(0xE, 0x19, 0xB849, 0);
    g_wldcore_menu_stack_types[g_wldcore_menu_stack_depth + 1] = WLDCORE_MENU_LEVEL_PROPOSITION_ATTEMPT_DETAIL;
    g_wldcore_menu_stack_depth = g_wldcore_menu_stack_depth + 1;
}
