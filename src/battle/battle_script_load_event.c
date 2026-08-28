#include "fft/battle_text.h"
#include "fft/event.h"
#include "fft/main_runtime.h"

void battle_script_load_event(s32 id) {
    event_file_block_t* block;
    u32 off;

    do {
        battle_thread_yield();
        g_battle_thread_call_target = (void (*)(void))main_file_call_build_header;
    } while (battle_thread_call_on_main_stack(
                 id * EVENT_BLOCK_SECTORS + EVENT_ARCHIVE_START_SECTOR, EVENT_BLOCK_BYTES, g_event_script_buffer)
        != 0);

    do {
        battle_thread_yield();
        g_battle_thread_call_target = main_file_is_still_loading;
    } while (battle_thread_call_on_main_stack() != 0);

    block = g_battle_event_block;
    off = block->text_offset_or_marker;
    if (off != EVENT_TEXT_OFFSET_PROCESSED) {
        g_battle_text_section_pointers[0] = (u8*)(off + (u32)block);
        block->text_offset_or_marker = EVENT_TEXT_OFFSET_PROCESSED;
    }
}
