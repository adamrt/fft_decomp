#include "fft/battle_text.h"
#include "fft/script_variables.h"
#include "fft/small.h"

typedef struct small_text_name_section_offsets {
    u32 unit_names;
    u32 job_names;
} small_text_name_section_offsets_t;

extern const small_text_name_section_offsets_t g_small_text_name_section_offsets;

void small_text_init_battle_pointers(void) {
    u8* string_data;

    string_data = (u8*)g_small_text_name_data;
    g_battle_text_section_pointers[8] = string_data + g_small_text_name_section_offsets.unit_names;
    g_battle_text_section_pointers[6] = string_data + g_small_text_name_section_offsets.job_names;
    g_battle_text_section_pointers[2] = g_battle_text_section_pointers[22];
    battle_script_set_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_UNIT_LIMIT, 0);
}
