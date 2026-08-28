/* Menu thread: resolves the current entry's display value (kind 0x11 reads
 * the text binding's halfword table, masked to 11 bits, with 0x2b remapped
 * to 0x63 and bit 15 set; other kinds index g_world_menu_entry_text_id_tables) and hands it to the
 * text-character handler before parking the menu input state. */
#include "fft/data.h"
#include "fft/event.h"
#include "fft/main_sound.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

void world_menu_announce_entry_value_thread(void) {
    world_menu_entry_t* entry;
    world_thread_t* thread;
    s32 kind;
    s32 index;
    s32 value;
    s32 next;

    entry = world_thread_get_current_parameter_1();
    kind = entry->select_text_table;
    if (kind == 2 || kind == -1) {
        next = g_world_thread_current_id + 1;
        thread = &g_world_threads[next];
        while (1) {
            if (thread->is_running != 0) {
                entry = (world_menu_entry_t*)thread->function_parameter_1;
                if (entry->select_text_table != -1) {
                    break;
                }
            }
            thread++;
        }
        kind = entry->select_text_table;
    }
    index = entry->selected_index;
    if (kind == 0x11) {
        value = entry->text_binding->row_text_ids[index];
        value &= 0x7FF;
        if (value == 0x2B) {
            value = 0x63;
        }
        value |= 0x8000;
    } else {
        value = g_world_menu_entry_text_id_tables[kind][index];
    }
    g_world_sound_effect_id_to_play = MAIN_SFX_WINDOW_OPEN;
    world_thread_set_parameters(g_world_thread_current_id, 0x3F, value, 0);
    world_text_character_handling_thread();
    g_world_formation_menu_new_buttons = 0;
    g_world_thread_task_active = 0;
    world_input_store_menu_state(&g_world_formation_menu_input_state);
    world_thread_exit_current();
}
