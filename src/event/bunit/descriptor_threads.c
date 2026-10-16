/* Descriptor threads, the selection thread, node cursor updates, the bits reader, controller input and event thread
 * state. */

#include "fft/event_bunit.h"
#include "fft/gfx.h"
#include "fft/main.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/menu_overlay.h"
#include "fft/menu_panel.h"
#include "fft/menu_text.h"
#include "fft/sound.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "psx/libetc.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Returns s32: the value is the 0/1 flag g_bunit_menu_thread_running, and
 * both call sites declare and use an s32 result. */
s32 bunit_menu_run_descriptor_thread(s32 id, menu_entry_t* desc) {
    if (g_bunit_menu_thread_running == 0) {
        if (thread_is_running(id) != 0) {
            return 1;
        }
        g_menu_thread_menu_data = desc;
        thread_start(id, desc->thread_entry);
        thread_set_parameters(id, (s32)g_menu_thread_menu_data, 0, 0);
        g_bunit_menu_thread_running = 1;
    }
    g_bunit_menu_thread_running = thread_is_running(id);
    return g_bunit_menu_thread_running;
}

/* Start the text thread for g_text_selection_id (ids above 0x1FFFF are
 * stashed in g_bunit_help_screen_id instead); thread_data + 0x38 is the text
 * thread's dialog type, as in menu_text_start_help_thread. */
void bunit_text_start_selection_thread(s32 thread_data) {
    if (g_text_selection_id > 0x1FFFF) {
        g_bunit_help_screen_id = g_text_selection_id;
        return;
    }
    if (g_text_selection_id <= 0) {
        return;
    }
    text_save_section_pointers();
    text_relocate_pointer_table(g_help_text_table);
    thread_start(THREAD_SCREEN_SLOT_TEXT, text_run_dialog_thread_variant);
    thread_set_parameters(THREAD_SCREEN_SLOT_TEXT, thread_data + 0x38, g_text_selection_id, 0);
    g_menu_input_lock_mode = 1;
}

/* Select the menu's node table, handle directional/confirm/cancel input, and
 * draw the selected node's cursor.
 *
 * The target reloads the active table and node index during confirmation.
 * Separate linker aliases preserve those loads; volatile accesses change the
 * instruction schedule. */
void bunit_menu_update_node_cursor(s16 mode) {
    point16_t pos;
    u32 flags;
    help_navigation_record_t* node;
    u16 x;
    u16 y;
    s32 running;
    s32 text_id;
    s32 node_index;

    if (g_bunit_help_cursor_initialized == 0) {
        /* Monster-ability help bases for the two ability nodes. */
        if (mode == 0) {
            g_bunit_help_node_23_help_base = -0x6800;
            g_bunit_help_node_24_help_base = -0x6800;
        } else {
            g_bunit_help_node_23_help_base = 0x7800;
            g_bunit_help_node_24_help_base = 0x7800;
        }
        g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
        g_bunit_help_cursor_initialized = 1;
        g_bunit_help_node_index = 0;
        if (g_bunit_help_screen_id == MENU_GRID_HELP_BANNER_TOP) {
            g_bunit_help_nodes = g_bunit_help_nodes_banner_top;
        } else if (g_bunit_help_screen_id == MENU_GRID_HELP_BANNER_BOTTOM) {
            g_bunit_help_nodes = g_bunit_help_nodes_banner_bottom;
        } else {
            g_bunit_help_nodes = g_bunit_help_nodes_unit_browse;
        }
    }
    running = thread_is_running(THREAD_SCREEN_SLOT_TEXT);
    if (running == 0) {
        flags = g_bunit_input_primary_repeat;
        node_index = g_bunit_help_node_index;
        if (flags & PSX_PAD_CROSS) {
            g_text_selection_id = g_bunit_help_screen_id;
            g_bunit_help_screen_id = MENU_GRID_HELP_NONE;
            g_sound_queued_effect_id = MAIN_SFX_CANCEL;
            g_bunit_help_cursor_initialized = 0;
        } else if (flags & PSX_PAD_CIRCLE) {
            text_id = 0;
            if (g_bunit_help_nodes[node_index].value != 0) {
                text_id = g_bunit_help_nodes[node_index].value[0];
            }
            if ((g_bunit_help_node_index_reload - 0x12) >= 0xAU || text_id != 0) {
                /* The target zero-extends help_base here (lhu): a -0x6800 base adds 0x9800. */
                g_text_selection_id
                    = text_id + (u16)g_bunit_help_nodes_reload[g_bunit_help_node_index_reload_2].help_base;
                bunit_text_start_selection_thread(3);
            }
        } else if (flags & PSX_PAD_UP) {
            node_index = g_bunit_help_nodes[node_index].destination[0];
        } else if (flags & PSX_PAD_DOWN) {
            node_index = g_bunit_help_nodes[node_index].destination[1];
        } else if (flags & PSX_PAD_RIGHT) {
            node_index = g_bunit_help_nodes[node_index].destination[2];
        } else if (flags & PSX_PAD_LEFT) {
            node_index = g_bunit_help_nodes[node_index].destination[3];
        }
        if (g_panel_selected_billboard.team_kind != 0) {
            if (g_bunit_help_node_index == 2 && node_index == 6) {
                node_index = 7;
            }
            if (g_bunit_help_node_index == 7 && node_index == 6) {
                node_index = 2;
            }
        }
        if (node_index != g_bunit_help_node_index) {
            g_sound_queued_effect_id = MAIN_SFX_CURSOR_MOVE;
            g_bunit_help_node_index = node_index;
        }
    }
    node = &g_bunit_help_nodes[g_bunit_help_node_index];
    x = node->x;
    pos.x = x;
    y = node->y;
    pos.y = y;
    if (node->vertical_cursor != 0) {
        pos.y = y + 1;
        menu_update_and_draw_bouncing_cursor_variant(&pos, &g_bunit_help_cursor_anim_state, running);
    } else {
        pos.x = x - 2;
        pos.y = y + 6;
        menu_update_and_draw_sliding_cursor_variant(&pos, &g_bunit_help_cursor_anim_state, running);
    }
}

/* Start the bit reader on a new data stream. */
void bunit_bits_init_reader(const u8* data) {
    g_bunit_bits_reader_reset = 1;
    g_bunit_bits_cursor = data;
}

/* Reads a `bit_count`-bit value (taken modulo 32), most significant bit
 * first, from the bitstream at g_bunit_bits_cursor. The first read after a
 * reset starts at bit 7 of the current byte. */
u32 bunit_bits_read(s32 bit_count) {
    u32 accumulator;

    if (bit_count == 0) {
        return 0;
    }

    bit_count &= 0x1f;
    if (g_bunit_bits_reader_reset != 0) {
        g_bunit_bits_index = 7;
        g_bunit_bits_reader_reset = 0;
    }

    bit_count--;
    accumulator = 0;
    if (bit_count != -1) {
        do {
            if ((*g_bunit_bits_cursor >> g_bunit_bits_index--) & 1) {
                accumulator |= 1 << bit_count;
            }
            bit_count--;
            if (g_bunit_bits_index < 0) {
                g_bunit_bits_index = 7;
                g_bunit_bits_cursor++;
            }
        } while (bit_count != -1);
    }
    return accumulator;
}

/*
 * Publish newly pressed buttons and two repeat rates for BUNIT menus.
 *
 * CARD's latch/activation/repeat logic with a slower secondary repeat
 * (period + 2) and a soft reset on L1+R1+Start+Select. Both masks are
 * also mirrored to g_bunit_input_primary_repeat/g_bunit_input_secondary_repeat.
 */
static void bunit_input_update_controller(void) {
    s16 bit;
    s32 index;
    u32 current_input;
    u32 filtered_input;
    s32* counter_base;
    u32 speed, initial_delay, secondary_repeat_period, repeat_period;
    speed = menu_get_event_speed();
    initial_delay = g_main_input_repeat_initial_delay / speed;
    repeat_period = g_main_input_repeat_period / speed;
    secondary_repeat_period = g_main_input_secondary_repeat_period / speed + 2;
    current_input = PadRead(0);
    if (current_input == (PSX_PAD_L1 | PSX_PAD_R1 | PSX_PAD_SELECT | PSX_PAD_START)) {
        main_system_reset_game();
    }
    if (current_input & g_bunit_input_latched_button) {
        current_input &= g_bunit_input_latched_button;
    } else if (current_input == 0) {
        g_bunit_input_latched_button = 0;
        g_input_activation_timer += speed;
    } else {
        if (g_input_activation_timer < 4) {
            if (current_input & (PSX_PAD_UP | PSX_PAD_RIGHT | PSX_PAD_DOWN | PSX_PAD_LEFT)) {
                current_input &= PSX_PAD_UP | PSX_PAD_RIGHT | PSX_PAD_DOWN | PSX_PAD_LEFT;
                g_input_activation_timer = 10;
            }
        }
        if (g_input_activation_timer >= 4) {
            g_input_activation_timer = 0;
            for (index = 0; index < 16; index++) {
                bit = 1 << index;
                if (current_input & bit) {
                    current_input &= bit;
                    g_bunit_input_latched_button = bit;
                }
            }
        } else {
            current_input = 0;
            g_bunit_input_latched_button = 0;
        }
    }
    filtered_input = current_input;
    if (g_input_activation_timer >= 10) {
        g_input_activation_timer = 10;
    }
    g_menu_input_newly_pressed = filtered_input;
    for (index = 0; index < 16; index++) {
        s32 mask = 1 << index;

        if (mask & g_menu_input_newly_pressed) {
            if (mask & g_bunit_input_previous) {
                g_menu_input_newly_pressed &= ~mask;
            } else {
                g_bunit_input_previous |= mask;
            }
        } else {
            g_bunit_input_previous &= ~mask;
        }
    }
    counter_base = g_input_repeat_counters;
    for (index = 0; index < 16; index++) {
        if (index == 4) {
            index = 12;
        }
        if (current_input & (1 << index)) {
            counter_base[index]++;
        } else {
            counter_base[index] = 0;
        }
        if ((counter_base[index] >= initial_delay && counter_base[index] % repeat_period != 0)
            || (counter_base[index] >= 2 && counter_base[index] < initial_delay)) {
            current_input &= ~(1 << index);
        }
        if (counter_base[index] >= initial_delay) {
            current_input |= 1 << (index + 16);
        }
        if ((counter_base[index] >= initial_delay && counter_base[index] % secondary_repeat_period != 0)
            || (counter_base[index] >= 2 && counter_base[index] < initial_delay << 1)) {
            filtered_input &= ~(1 << index);
        }
    }
    current_input &= 0xf00ff00f;
    filtered_input &= 0xf00ff00f;
    current_input |= g_menu_input_newly_pressed & 0x0ff0;
    filtered_input |= g_menu_input_newly_pressed & 0x0ff0;
    g_menu_input_primary_repeat = current_input;
    g_menu_input_secondary_repeat = filtered_input;
    g_bunit_input_primary_repeat = current_input;
    g_bunit_input_secondary_repeat = filtered_input;
}

/*
 * Per-frame input update of the BUNIT screen.
 *
 * Reads the pad, then clears the 32-bit input values while the text thread runs or a
 * help screen is open, on the frame the text ends and during a screen fade.
 * The help text state goes 1 on the text thread's first frame (which queues
 * the window-open sound), 2 after it and 0 otherwise.
 */
void bunit_menu_update_event_thread_state(void) {
    s32 running;

    bunit_input_update_controller();
    running = thread_is_running(THREAD_SCREEN_SLOT_TEXT);

    if (running != 0 || g_bunit_help_screen_id != MENU_GRID_HELP_NONE) {
        menu_input_clear_state();
        if (running != 0) {
            g_bunit_help_text_state = (g_bunit_help_text_state != 0) ? 2 : 1;
        } else {
            g_bunit_help_text_state = 0;
        }
    } else if (g_bunit_help_text_state != 0 || gfx_get_fade_state() != 0) {
        g_bunit_help_text_state = 0;
        menu_input_clear_state();
    }

    if (running == 0) {
        g_menu_input_lock_mode = 0;
    }
    if (g_bunit_help_text_state == 1) {
        g_sound_queued_effect_id = MAIN_SFX_WINDOW_OPEN;
    }
}
