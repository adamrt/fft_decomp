#include "fft/main_file.h"
#include "fft/script_variables.h"
#include "fft/world.h"
#include "psx/types.h"

void world_menu_load_text_1c18(void) {
    s32 i;

    world_init_scene_bindings();
    g_world_thread_inner_subroutine_callback = (void (*)(void))main_file_load_checked_to_address;
    world_thread_call_on_main_stack(0x1C18, 0x5800, g_world_menu_text_file_buffer);
    world_menu_reset_runtime();
    world_script_set_variable(EVENT_SCRIPT_VAR_WORLD_DEBUG_BATTLE_STYLE, 0);
    g_world_menu_thread_menu_data = g_world_menu_default_entries;
    for (i = 0; i < 0x20; i++) {
        g_world_text_section_pointers[i]
            = (u8*)(g_world_menu_text_file_buffer[i] + (s32)&g_world_menu_text_file_buffer[0x20]);
    }
    world_script_set_variable(EVENT_SCRIPT_VAR_RAMZA_MANDATORY_IN_SQUAD, 0);
    world_script_set_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_SQUAD_COUNT, 1);
    world_script_set_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_PRIMARY_SQUAD_ID, 0);
    world_script_set_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_SECONDARY_SQUAD_ID, 1);
}
