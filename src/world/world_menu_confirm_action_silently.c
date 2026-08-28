#include "fft/world.h"
#include "psx/pad.h"
#include "psx/types.h"

typedef struct world_menu_confirm_entry world_menu_confirm_entry_t;
extern void world_menu_handle_entry_confirm(world_menu_confirm_entry_t* entry, s32 row_index);

void world_menu_confirm_action_silently(s32 entry) {
    g_world_menu_sound_muted = 1;
    g_world_menu_new_button_input = PSX_PAD_CIRCLE;
    world_menu_handle_entry_confirm((world_menu_confirm_entry_t*)entry, 0);
    g_world_menu_sound_muted = 0;
    g_world_menu_new_button_input = 0;
}
