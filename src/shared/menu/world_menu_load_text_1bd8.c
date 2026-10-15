#include "fft/script.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

/*
 * Set up the WORLD menu subsystems from the main thread: common graphics,
 * the 0xe800-byte menu text file at sector 0x1bd8 and the default menu
 * entries.
 *
 * The file opens with 0x20 section offsets relative to its byte 0x80; they
 * become g_text_section_pointers, with the location names taken from
 * the resident copy. The deployment script variables are reset to one squad.
 */
void world_menu_load_text_1bd8(void) {
    s32 i;
    s32 value;
    u8* base;
    s32* src;
    u8** dst;

    g_thread_current_id = THREAD_SLOT_MAIN;
    menu_init_subsystem_pointers();
    world_menu_load_common_graphics(0);
    g_thread_call_target = (void (*)(void))main_file_load_checked_to_address;
    thread_call_on_main_stack(DISC_SECTOR_EVENT_WORLD_LZW, 0xe800, g_world_menu_text_file_buffer);
    menu_reset_subsystems();
    script_set_variable(EVENT_SCRIPT_VAR_WORLD_DEBUG_BATTLE_STYLE, 0);
    g_menu_thread_menu_data = g_world_menu_default_entries;
    i = 0;
    src = g_world_menu_text_file_buffer;
    base = (u8*)&src[TEXT_SECTION_COUNT];
    dst = g_text_section_pointers;
    do {
        value = *src++;
        /* Byte-pointer addition reverses the target's final addu operands. */
        *dst++ = (u8*)(value + (s32)base);
        i++;
    } while (i < TEXT_SECTION_COUNT);
    g_text_section_pointers[TEXT_SECTION_LOCATION_NAMES] = g_world_location_names_resident;
    script_set_variable(EVENT_SCRIPT_VAR_RAMZA_MANDATORY_IN_SQUAD, 0);
    script_set_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_SQUAD_COUNT, 1);
    script_set_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_PRIMARY_SQUAD_ID, 0);
    script_set_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_SECONDARY_SQUAD_ID, 1);
}
