#include "fft/battle_script.h"
#include "fft/event_attack.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "psx/types.h"

/*
 * Upload the current map's title graphic and its palette.
 *
 * The titles are stored four to a 0x2800-byte file, 0xa00 bytes each, in
 * files five sectors apart from sector 0xdac, indexed by map id - 1. For map
 * 0x100 no file is read and the freshly allocated buffer is uploaded as it
 * is.
 */
void attack_map_load_title_graphic(void) {
    s32 group;
    u8* image;
    s32 title;
    s32 adjusted;
    s32 group_input;

    image = menu_alloc_memory(0x2800);
    title = script_get_variable(EVENT_SCRIPT_VAR_CURRENT_MAP);
    adjusted = title - 1;
    group_input = adjusted;
    if (adjusted < 0) {
        group_input = title + 2;
    }
    group_input = group = group_input >> 2;
    group = adjusted - group * 4;

    if (adjusted != 0xff) {
        title = group_input * 5 + DISC_SECTOR_EVENT_MAPTITLE_BIN;
        g_thread_call_target = (void (*)(void))main_file_load_checked_to_address;
        thread_call_on_main_stack(title, 0x2800, image);
    }

    LoadImage(&g_attack_map_title_image_rect, (u32*)(image + group * 0xa00));
    LoadImage(&g_attack_map_title_palette_rect, &g_attack_map_title_palette_data);
    menu_free_memory(image);
}
