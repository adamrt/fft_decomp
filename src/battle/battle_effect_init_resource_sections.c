#include "fft/battle.h"
#include "psx/types.h"

s32 battle_effect_init_resource_sections(battle_effect_resource_t* resource) {
    g_battle_effect_model_header = &resource->words[0];
    g_battle_effect_model_vertices = &resource->words[resource->words[0] / 4];
    D_801B8A34 = &resource->words[resource->words[2] / 4];
    g_battle_effect_model_commands = &resource->words[resource->words[4] / 4];
    g_battle_effect_model_command_index = 0;
    return resource->words[5];
}
