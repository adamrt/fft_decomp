#include "fft/world.h"
#include "psx/types.h"

/* Points one of the graphics-data pointers at its resident copy. */
void world_gfx_bind_data_pointer(u32 index) {
    switch (index) {
    case 0:
        g_world_text_skillset_names = g_world_text_skillset_names_data;
        break;
    case 1:
        g_world_text_job_names = g_world_text_job_names_data;
        break;
    case 2:
        g_world_text_ability_names = g_world_text_ability_names_data;
        break;
    case 3:
        g_world_text_section_pointers[24] = g_world_text_formation_messages_data;
        break;
    case 4:
        g_world_text_section_pointers[29] = g_world_text_dismiss_unit_replies_data;
        break;
    case 5:
        g_world_text_section_pointers[29] = g_world_text_job_descriptions_data;
        break;
    case 7:
        g_world_text_section_pointers[27] = g_world_text_name_entry_keyboard_data;
        break;
    case 8:
        g_world_text_section_pointers[31] = g_world_text_shop_dialogue_data;
        break;
    }
}
