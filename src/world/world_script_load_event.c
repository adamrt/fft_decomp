#include "fft/event.h"
#include "fft/main_runtime.h"
#include "fft/world.h"

/* WORLD twin of battle_script_load_event: read one event block from the archive on
 * the main thread, wait for the load, then patch the text-section pointer. */
void world_script_load_event(s32 id) {
    event_file_block_t* block;
    u32 off;

    do {
        world_thread_yield();
        g_world_thread_call_target = (void (*)(void))main_file_call_build_header;
    } while (world_thread_call_on_main_stack(
                 id * EVENT_BLOCK_SECTORS + EVENT_ARCHIVE_START_SECTOR, EVENT_BLOCK_BYTES, g_event_script_buffer)
        != 0);

    do {
        world_thread_yield();
        g_world_thread_call_target = main_file_is_still_loading;
    } while (world_thread_call_on_main_stack() != 0);

    block = (event_file_block_t*)g_world_event_script;
    off = block->text_offset_or_marker;
    if (off != EVENT_TEXT_OFFSET_PROCESSED) {
        g_world_text_section_pointers[0] = (u8*)(off + (u32)block);
        block->text_offset_or_marker = EVENT_TEXT_OFFSET_PROCESSED;
    }
}
