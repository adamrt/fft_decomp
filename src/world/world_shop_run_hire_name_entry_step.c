#include "fft/gfx.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "fft/world.h"
#include "psx/libc.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Soldier office: the name entry keyboard for the recruit, then the fee prompt. */
void world_shop_run_hire_name_entry_step(void) {
    if (g_world_shop_hire_name_entry_initialized == 0) {
        g_world_formation_unit_banner_enabled = 0;
        menu_toggle_unit_status_thread(0);
        g_world_shop_hire_name_entry_initialized = 1;
    }
    g_world_shop_entered_unit_name
        = world_name_run_entry_keyboard_frame(g_world_unit_pointers[g_unit_selected_index]->name, 0);
    if (g_world_shop_entered_unit_name != 0) {
        g_world_shop_hire_name_entry_initialized = 0;
        g_text_substitution_values[0] = g_world_shop_service_fee;
        thread_start(THREAD_SCREEN_SLOT_MESSAGE, world_text_message_box_thread);
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, 0xF82A, 0);
        g_world_shop_menu_step = WORLD_SHOP_STEP_HIRE_FEE_CONFIRM;
    }
}
