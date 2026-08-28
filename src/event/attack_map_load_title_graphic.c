#include "fft/attack.h"
#include "fft/battle.h"
#include "fft/main_file.h"
#include "fft/script_variables.h"
#include "fft/thread.h"
#include "psx/types.h"

void attack_map_load_title_graphic(void) {
    s32 group;
    s32 image;
    s32 title;
    s32 adjusted;
    s32 group_input;

    image = (s32)battle_menu_alloc_memory(0x2800);
    title = battle_script_get_variable(EVENT_SCRIPT_VAR_CURRENT_MAP);
    adjusted = title - 1;
    group_input = adjusted;
    if (adjusted < 0) {
        group_input = title + 2;
    }
    group_input = (group = group_input >> 2);
    group = adjusted - group * 4;

    if (adjusted != 0xff) {
        title = group_input * 5 + 0xdac;
        g_battle_thread_call_target = (void (*)(void))main_file_load_checked_to_address;
        battle_thread_call_on_main_stack(title, 0x2800, image);
    }

    LoadImage(&g_attack_map_title_image_rect, image + group * 0xa00);
    LoadImage(&g_attack_map_title_palette_rect, &g_attack_map_title_palette_data);
    battle_menu_free_memory(image);
}
