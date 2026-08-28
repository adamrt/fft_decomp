#include "fft/main_file.h"
#include "fft/script_variables.h"
#include "fft/world.h"
#include "psx/types.h"

void world_menu_load_text_1bd8(void) {
    s32 i;
    s32 value;
    s32 base;
    s32* src;
    s32* dst;

    g_world_thread_current_id = 0;
    world_init_scene_bindings();
    world_menu_load_common_graphics(0);
    g_world_thread_inner_subroutine_callback = (void (*)(void))main_file_load_checked_to_address;
    world_thread_call_on_main_stack(0x1bd8, 0xe800, g_world_menu_text_file_buffer);
    world_menu_reset_runtime();
    world_script_set_variable(EVENT_SCRIPT_VAR_WORLD_DEBUG_BATTLE_STYLE, 0);
    g_world_menu_thread_menu_data = g_world_menu_default_entries;
    i = 0;
    src = g_world_menu_text_file_buffer;
    base = (s32)src + 0x80;
    dst = (s32*)g_world_text_section_pointers;
    do {
        value = *src++;
        *dst++ = value + base;
        i++;
    } while (i < 0x20);
    g_world_text_location_names = g_world_location_names_resident;
    world_script_set_variable(EVENT_SCRIPT_VAR_RAMZA_MANDATORY_IN_SQUAD, 0);
    world_script_set_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_SQUAD_COUNT, 1);
    world_script_set_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_PRIMARY_SQUAD_ID, 0);
    world_script_set_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_SECONDARY_SQUAD_ID, 1);
}
